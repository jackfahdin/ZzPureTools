#include "ZzSplitWorkspacePrivate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>

#include <QtCore/QCryptographicHash>
#include <QtCore/QUuid>
#include <QtCore/QDataStream>
#include <QtCore/QEvent>
#include <QtCore/QMimeData>
#include <QtCore/QSet>
#include <QtCore/QVariant>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QColor>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzTabBarPrivate.h"
#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

namespace {

constexpr auto zzWorkspaceTabMimeType =
    "application/x-zz-split-workspace-tab-v1";
constexpr auto zzWorkspaceDragTokenLifetime = std::chrono::seconds(5);
constexpr auto zzWorkspaceLayoutMagic = "ZZSW";
constexpr quint16 zzWorkspaceLayoutSchemaVersion = 1;
constexpr auto zzWorkspaceLayoutStreamVersion = QDataStream::Qt_6_8;
constexpr qsizetype zzWorkspaceLayoutDigestSize = 32;
constexpr qsizetype zzWorkspaceLayoutHeaderSize = 12;
constexpr qsizetype zzWorkspaceMaximumPayloadSize = 1024 * 1024;
constexpr int zzWorkspaceMaximumNodeCount = 127;
constexpr int zzWorkspaceMaximumStringLength = 256;
constexpr int zzWorkspaceMaximumSavedPageCount = 4096;
constexpr int zzWorkspaceMaximumPageOrder = 65535;

struct ZzWorkspaceLivePage final
{
    QPointer<QWidget> page;
    QString text;
    QIcon icon;
    QString toolTip;
    QString whatsThis;
    QVariant data;
    QColor textColor;
    bool enabled = true;
    bool pinned = false;
    bool modified = false;
    bool attention = false;
    bool closeEnabled = true;
};

struct ZzWorkspaceLiveGroup final
{
    ZzTabGroupId id;
    QPointer<ZzTabWidget> tabs;
    const ZzTabWidget *identity = nullptr;
    std::vector<ZzWorkspaceLivePage> pages;
    QPointer<QWidget> currentPage;
};

struct ZzWorkspaceDesiredPage final
{
    QPointer<QWidget> page;
    QPointer<ZzTabWidget> originalTabs;
    ZzTabGroupId targetId;
    int desiredOrder = 0;
    int sequence = 0;
    bool savedOrder = false;
};

struct ZzWorkspacePreservedPage final
{
    ZzWorkspaceLivePage snapshot;
    ZzTabGroupId preferredGroupId;
};

void zzWriteWorkspaceString(QDataStream &stream, const QString &value)
{
    stream << static_cast<quint16>(value.size());
    for (const QChar character : value) {
        stream << character.unicode();
    }
}

[[nodiscard]] bool zzReadWorkspaceString(
    QDataStream &stream,
    QString *value)
{
    Q_ASSERT(value != nullptr);
    quint16 length = 0;
    stream >> length;
    if (stream.status() != QDataStream::Ok
        || length > zzWorkspaceMaximumStringLength) {
        return false;
    }

    value->clear();
    value->reserve(length);
    for (quint16 index = 0; index < length; ++index) {
        quint16 codeUnit = 0;
        stream >> codeUnit;
        if (stream.status() != QDataStream::Ok) {
            return false;
        }
        value->append(QChar(codeUnit));
    }
    return true;
}

[[nodiscard]] bool zzWriteWorkspaceNode(
    QDataStream &stream,
    const ZzNode *node,
    int depth,
    int *nodeCount,
    int *groupCount)
{
    Q_ASSERT(nodeCount != nullptr);
    Q_ASSERT(groupCount != nullptr);
    if (node == nullptr || depth > ZzSplitWorkspacePrivate::maximumTreeDepth
        || ++(*nodeCount) > zzWorkspaceMaximumNodeCount) {
        return false;
    }
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        const QString id = std::get<ZzLeaf>(node->value).id.value();
        if (id.isEmpty() || id.size() > zzWorkspaceMaximumStringLength
            || ++(*groupCount)
                > ZzSplitWorkspacePrivate::maximumGroupCount) {
            return false;
        }
        stream << static_cast<quint8>(0);
        zzWriteWorkspaceString(stream, id);
        return stream.status() == QDataStream::Ok;
    }

    const auto &branch = std::get<ZzBranch>(node->value);
    if ((branch.orientation != Qt::Horizontal
         && branch.orientation != Qt::Vertical)
        || branch.children.size() < 2
        || branch.children.size()
            > static_cast<std::size_t>(
                ZzSplitWorkspacePrivate::maximumGroupCount)) {
        return false;
    }
    stream << static_cast<quint8>(1)
           << static_cast<quint8>(branch.orientation)
           << static_cast<quint16>(branch.children.size());
    for (const auto &child : branch.children) {
        if (!zzWriteWorkspaceNode(
                stream,
                child.get(),
                depth + 1,
                nodeCount,
                groupCount)) {
            return false;
        }
    }

    QList<int> sizes = branch.splitter != nullptr
        ? branch.splitter->sizes()
        : QList<int> {};
    if (sizes.size()
        != static_cast<qsizetype>(branch.children.size())) {
        sizes.fill(1, static_cast<qsizetype>(branch.children.size()));
    }
    stream << static_cast<quint16>(sizes.size());
    for (const int size : sizes) {
        stream << static_cast<qint32>(std::max(1, size));
    }
    return stream.status() == QDataStream::Ok;
}

[[nodiscard]] bool zzReadWorkspaceNode(
    QDataStream &stream,
    int depth,
    std::optional<Qt::Orientation> parentOrientation,
    int *nodeCount,
    int *groupCount,
    QSet<QString> *ids,
    ZzWorkspaceLayoutNode *node)
{
    Q_ASSERT(nodeCount != nullptr);
    Q_ASSERT(groupCount != nullptr);
    Q_ASSERT(ids != nullptr);
    Q_ASSERT(node != nullptr);
    if (depth > ZzSplitWorkspacePrivate::maximumTreeDepth
        || ++(*nodeCount) > zzWorkspaceMaximumNodeCount) {
        return false;
    }

    quint8 tag = 0;
    stream >> tag;
    if (stream.status() != QDataStream::Ok || tag > 1) {
        return false;
    }
    if (tag == 0) {
        QString rawId;
        if (!zzReadWorkspaceString(stream, &rawId)) {
            return false;
        }
        const ZzTabGroupId id(rawId);
        if (!id.isValid() || ids->contains(id.value())
            || ++(*groupCount)
                > ZzSplitWorkspacePrivate::maximumGroupCount) {
            return false;
        }
        ids->insert(id.value());
        node->leaf = true;
        node->id = id;
        return true;
    }

    quint8 rawOrientation = 0;
    quint16 childCount = 0;
    stream >> rawOrientation >> childCount;
    if (stream.status() != QDataStream::Ok
        || (rawOrientation != static_cast<quint8>(Qt::Horizontal)
            && rawOrientation != static_cast<quint8>(Qt::Vertical))
        || childCount < 2
        || childCount > ZzSplitWorkspacePrivate::maximumGroupCount) {
        return false;
    }
    const auto orientation = static_cast<Qt::Orientation>(rawOrientation);
    if (parentOrientation.has_value()
        && parentOrientation.value() == orientation) {
        return false;
    }

    node->leaf = false;
    node->orientation = orientation;
    node->children.clear();
    node->children.reserve(childCount);
    for (quint16 index = 0; index < childCount; ++index) {
        ZzWorkspaceLayoutNode child;
        if (!zzReadWorkspaceNode(
                stream,
                depth + 1,
                orientation,
                nodeCount,
                groupCount,
                ids,
                &child)) {
            return false;
        }
        node->children.push_back(std::move(child));
    }

    quint16 sizeCount = 0;
    stream >> sizeCount;
    if (stream.status() != QDataStream::Ok || sizeCount != childCount) {
        return false;
    }
    node->sizes.clear();
    node->sizes.reserve(sizeCount);
    for (quint16 index = 0; index < sizeCount; ++index) {
        qint32 size = 0;
        stream >> size;
        if (stream.status() != QDataStream::Ok || size <= 0) {
            return false;
        }
        node->sizes.push_back(size);
    }
    return true;
}

[[nodiscard]] std::optional<ZzWorkspaceLayoutState>
zzDecodeWorkspaceLayout(const QByteArray &encoded)
{
    const qsizetype maximumEncodedSize = zzWorkspaceLayoutHeaderSize
        + zzWorkspaceMaximumPayloadSize + zzWorkspaceLayoutDigestSize;
    if (encoded.size() < zzWorkspaceLayoutHeaderSize
            + zzWorkspaceLayoutDigestSize
        || encoded.size() > maximumEncodedSize
        || encoded.first(4) != QByteArray(zzWorkspaceLayoutMagic, 4)) {
        return std::nullopt;
    }

    QDataStream envelope(encoded);
    envelope.setVersion(zzWorkspaceLayoutStreamVersion);
    char magic[4] {};
    if (envelope.readRawData(magic, 4) != 4) {
        return std::nullopt;
    }
    quint16 schemaVersion = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    envelope >> schemaVersion >> streamVersion >> payloadLength;
    const qint64 expectedSize = zzWorkspaceLayoutHeaderSize
        + static_cast<qint64>(payloadLength)
        + zzWorkspaceLayoutDigestSize;
    if (envelope.status() != QDataStream::Ok
        || schemaVersion != zzWorkspaceLayoutSchemaVersion
        || streamVersion
            != static_cast<quint16>(zzWorkspaceLayoutStreamVersion)
        || payloadLength
            > static_cast<quint32>(zzWorkspaceMaximumPayloadSize)
        || expectedSize != encoded.size()) {
        return std::nullopt;
    }

    const QByteArray payload = encoded.sliced(
        zzWorkspaceLayoutHeaderSize,
        static_cast<qsizetype>(payloadLength));
    const QByteArray digest = encoded.last(zzWorkspaceLayoutDigestSize);
    if (digest != QCryptographicHash::hash(
            payload, QCryptographicHash::Sha256)) {
        return std::nullopt;
    }

    QDataStream stream(payload);
    stream.setVersion(zzWorkspaceLayoutStreamVersion);
    ZzWorkspaceLayoutState state;
    QSet<QString> ids;
    int nodeCount = 0;
    int groupCount = 0;
    if (!zzReadWorkspaceNode(
            stream,
            1,
            std::nullopt,
            &nodeCount,
            &groupCount,
            &ids,
            &state.root)) {
        return std::nullopt;
    }

    QString rawActiveId;
    if (!zzReadWorkspaceString(stream, &rawActiveId)) {
        return std::nullopt;
    }
    state.activeId = ZzTabGroupId(rawActiveId);
    if (!ids.contains(state.activeId.value())) {
        return std::nullopt;
    }

    quint16 pageCount = 0;
    stream >> pageCount;
    if (stream.status() != QDataStream::Ok
        || pageCount > zzWorkspaceMaximumSavedPageCount) {
        return std::nullopt;
    }
    state.pages.reserve(pageCount);
    QSet<QString> keys;
    QSet<QString> groupsWithCurrentPage;
    QHash<QString, QSet<int>> ordersByGroup;
    for (quint16 index = 0; index < pageCount; ++index) {
        QString rawKey;
        QString rawGroupId;
        qint32 order = 0;
        quint8 current = 0;
        if (!zzReadWorkspaceString(stream, &rawKey)
            || !zzReadWorkspaceString(stream, &rawGroupId)) {
            return std::nullopt;
        }
        stream >> order >> current;
        const QString key = rawKey.trimmed();
        const ZzTabGroupId groupId(rawGroupId);
        if (stream.status() != QDataStream::Ok || key.isEmpty()
            || key.size() > zzWorkspaceMaximumStringLength
            || keys.contains(key) || !ids.contains(groupId.value())
            || order < 0 || order > zzWorkspaceMaximumPageOrder
            || ordersByGroup[groupId.value()].contains(order)
            || current > 1
            || (current == 1
                && groupsWithCurrentPage.contains(groupId.value()))) {
            return std::nullopt;
        }
        keys.insert(key);
        ordersByGroup[groupId.value()].insert(order);
        if (current == 1) {
            groupsWithCurrentPage.insert(groupId.value());
        }
        state.pages.push_back(
            {key, groupId, static_cast<int>(order), current == 1});
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
        return std::nullopt;
    }
    return state;
}

void zzRestoreWorkspaceLayoutSizes(
    const ZzWorkspaceLayoutNode &layout,
    ZzNode *node)
{
    if (layout.leaf || node == nullptr
        || std::holds_alternative<ZzLeaf>(node->value)) {
        return;
    }
    auto &branch = std::get<ZzBranch>(node->value);
    if (branch.splitter != nullptr
        && layout.sizes.size()
            == static_cast<qsizetype>(branch.children.size())) {
        branch.splitter->setSizes(layout.sizes);
    }
    const auto childCount = std::min(
        layout.children.size(), branch.children.size());
    for (std::size_t index = 0; index < childCount; ++index) {
        zzRestoreWorkspaceLayoutSizes(
            layout.children[index], branch.children[index].get());
    }
}

[[nodiscard]] ZzTabWidget *zzOwningWorkspaceTabs(QWidget *page)
{
    for (QObject *current = page != nullptr ? page->parent() : nullptr;
         current != nullptr;
         current = current->parent()) {
        if (auto *tabs = qobject_cast<ZzTabWidget *>(current);
            tabs != nullptr) {
            return tabs;
        }
    }
    return nullptr;
}


[[nodiscard]] bool
zzWorkspaceContainsPage(const ZzSplitWorkspacePrivate *workspace,
                        const QWidget *page) {
  if (workspace == nullptr || page == nullptr) {
    return false;
  }
  std::vector<const ZzNode *> leaves;
  ZzSplitWorkspacePrivate::collectLeaves(workspace->root.get(), leaves);
  const bool inPublicTree = std::any_of(
      leaves.cbegin(), leaves.cend(), [page](const ZzNode *leafNode) {
        const auto &leaf = std::get<ZzLeaf>(leafNode->value);
        return !leaf.tabs.isNull() && leaf.tabs->indexOf(page) >= 0;
      });
  if (inPublicTree) {
    return true;
  }
  return std::any_of(
      workspace->restoreTransactionOwners.cbegin(),
      workspace->restoreTransactionOwners.cend(),
      [page](const QPointer<ZzTabWidget> &owner) {
        return !owner.isNull() && owner->indexOf(page) >= 0;
      });
}

[[nodiscard]] bool zzRestoreWorkspacePageMetadata(
    ZzTabWidget *tabs, const ZzWorkspaceLivePage &snapshot, int desiredIndex) {
  QPointer<ZzTabWidget> guardedTabs = tabs;
  const QPointer<QWidget> page = snapshot.page;
  int rebuildCount = 0;
  const auto resolveOrRebuildIndex = [&]() {
    if (guardedTabs.isNull() || page.isNull()) {
      return -1;
    }
    int index = guardedTabs->indexOf(page);
    if (index >= 0) {
      return index;
    }
    ZzTabWidget *const owner = zzOwningWorkspaceTabs(page);
    if (owner != nullptr && owner != guardedTabs) {
      return -1;
    }
    guardedTabs->insertTab(std::clamp(desiredIndex, 0, guardedTabs->count()),
                           page, snapshot.icon, snapshot.text);
    ++rebuildCount;
    return !guardedTabs.isNull() && !page.isNull() ? guardedTabs->indexOf(page)
                                                   : -1;
  };
  constexpr int maximumMetadataRebuildCount = 16;
  while (rebuildCount <= maximumMetadataRebuildCount) {
    const int rebuildsBeforePass = rebuildCount;
    int index = resolveOrRebuildIndex();
    if (index < 0) {
      return false;
    }

    guardedTabs->setTabText(index, snapshot.text);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabIcon(index, snapshot.icon);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabToolTip(index, snapshot.toolTip);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabWhatsThis(index, snapshot.whatsThis);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabEnabled(index, snapshot.enabled);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->fluentTabBar()->setTabData(index, snapshot.data);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->fluentTabBar()->setTabTextColor(index, snapshot.textColor);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabPinned(index, snapshot.pinned);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabModified(index, snapshot.modified);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabAttention(index, snapshot.attention);
    if ((index = resolveOrRebuildIndex()) < 0) {
      return false;
    }
    guardedTabs->setTabCloseEnabled(index, snapshot.closeEnabled);
    if (resolveOrRebuildIndex() < 0) {
      return false;
    }
    if (rebuildCount == rebuildsBeforePass) {
      return true;
    }
  }
  return false;
}

class ZzScopedSignalMute final {
public:
  explicit ZzScopedSignalMute(QObject *object)
      : m_object(object),
        m_previouslyBlocked(object != nullptr && object->blockSignals(true)) {}

  ~ZzScopedSignalMute() {
    if (!m_object.isNull()) {
      m_object->blockSignals(m_previouslyBlocked);
    }
  }

  ZzScopedSignalMute(const ZzScopedSignalMute &) = delete;
  ZzScopedSignalMute &operator=(const ZzScopedSignalMute &) = delete;

private:
  QPointer<QObject> m_object;
  bool m_previouslyBlocked = false;
};

class ZzScopedTabTransferAcceptance final {
public:
  explicit ZzScopedTabTransferAcceptance(ZzTabWidget *tabs)
      : m_tabBar(tabs != nullptr ? tabs->fluentTabBar() : nullptr),
        m_previouslyEnabled(!m_tabBar.isNull() &&
                            m_tabBar->isTabTransferEnabled()) {
    if (!m_tabBar.isNull() && !m_previouslyEnabled) {
      m_tabBar->setTabTransferEnabled(true);
    }
  }

  ~ZzScopedTabTransferAcceptance() {
    if (!m_tabBar.isNull() && !m_previouslyEnabled) {
      m_tabBar->setTabTransferEnabled(false);
    }
  }

  ZzScopedTabTransferAcceptance(const ZzScopedTabTransferAcceptance &) = delete;
  ZzScopedTabTransferAcceptance &operator=(
      const ZzScopedTabTransferAcceptance &) = delete;

  [[nodiscard]] bool isReady() const noexcept {
    return !m_tabBar.isNull() && m_tabBar->isTabTransferEnabled();
  }

private:
  QPointer<ZzTabBar> m_tabBar;
  bool m_previouslyEnabled = false;
};

class ZzWorkspaceRestoreTransaction final {
public:
  ZzWorkspaceRestoreTransaction(ZzSplitWorkspacePrivate *workspace,
                                ZzWorkspaceLayoutState state)
      : m_workspace(workspace),
        m_publicWorkspace(workspace != nullptr ? workspace->q_ptr : nullptr),
        m_state(std::move(state)) {}

  ~ZzWorkspaceRestoreTransaction() {
    if (!m_publicWorkspace.isNull()) {
      m_workspace->restoreTransactionOwners.clear();
      m_workspace->restoreTransactionKeyChanges.clear();
    }
    if (m_escrowTabs.isNull()) {
      return;
    }
    if (m_escrowTabs->count() > 0 && !m_publicWorkspace.isNull() &&
        m_workspace->rootHost != nullptr) {
      if (ensurePublicLeafTabs()) {
        [[maybe_unused]] const bool capturedPagesRestored =
            restoreRemainingCapturedPages();
        [[maybe_unused]] const bool preservedPagesRestored =
            restorePreservedPages();
        [[maybe_unused]] const bool currentPagesRestored =
            restoreOriginalCurrentPages();
      }
    }
    delete m_escrowTabs.data();
  }

  [[nodiscard]] bool run() {
    if (!captureSnapshot()) {
      return false;
    }
    if (!buildStaging()) {
      cleanupStaging();
      return false;
    }
    if (!preparePlan()) {
      return rollbackPages();
    }
    if (!transferPages()) {
      return m_publicWorkspace.isNull() ? false : rollbackPages();
    }
    return commitView();
  }

private:
  static void muteEmitterTree(
      QObject *root,
      std::vector<std::unique_ptr<ZzScopedSignalMute>> &signalMutes) {
    if (root == nullptr) {
      return;
    }
    signalMutes.push_back(std::make_unique<ZzScopedSignalMute>(root));
    const auto emitters = root->findChildren<QObject *>();
    for (QObject *emitter : emitters) {
      signalMutes.push_back(std::make_unique<ZzScopedSignalMute>(emitter));
    }
  }

  [[nodiscard]] bool setCurrentPageSilently(
      const QPointer<ZzTabWidget> &target,
      const QPointer<QWidget> &page) {
    if (m_publicWorkspace.isNull() || target.isNull() || page.isNull()) {
      return false;
    }
    const int desiredIndex = target->indexOf(page);
    if (desiredIndex < 0) {
      return false;
    }
    std::vector<std::unique_ptr<ZzScopedSignalMute>> signalMutes;
    muteEmitterTree(target, signalMutes);
    const QPointer<ZzTabBar> targetBar = target->fluentTabBar();
    const QPointer<QStackedWidget> targetStack =
        target->findChild<QStackedWidget *>(QString(),
                                           Qt::FindDirectChildrenOnly);
    if (targetBar.isNull() || targetStack.isNull()) {
      return false;
    }
    targetBar->setCurrentIndex(desiredIndex);
    if (m_publicWorkspace.isNull() || target.isNull() || targetBar.isNull() ||
        targetStack.isNull() || page.isNull()) {
      return false;
    }
    targetStack->setCurrentIndex(desiredIndex);
    return !m_publicWorkspace.isNull() && !target.isNull() &&
           !targetBar.isNull() && !targetStack.isNull() && !page.isNull() &&
           targetBar->currentIndex() == desiredIndex &&
           targetStack->currentIndex() == desiredIndex &&
           target->currentIndex() == desiredIndex &&
           target->currentWidget() == page;
  }

  [[nodiscard]] bool synchronizePinnedStackOrder(
      const QPointer<ZzTabWidget> &tabs) {
    if (m_publicWorkspace.isNull() || tabs.isNull()) {
      return false;
    }
    const QPointer<QStackedWidget> stack =
        tabs->findChild<QStackedWidget *>(QString(),
                                         Qt::FindDirectChildrenOnly);
    if (stack.isNull() || stack->count() != tabs->count()) {
      return false;
    }
    struct OrderedPage final {
      QPointer<QWidget> page;
      bool pinned = false;
    };
    std::vector<OrderedPage> orderedPages;
    orderedPages.reserve(static_cast<std::size_t>(stack->count()));
    for (int index = 0; index < stack->count(); ++index) {
      const QPointer<QWidget> page = stack->widget(index);
      if (page.isNull()) {
        return false;
      }
      orderedPages.push_back({page, tabs->isTabPinned(index)});
    }
    std::stable_partition(
        orderedPages.begin(), orderedPages.end(),
        [](const OrderedPage &entry) { return entry.pinned; });
    for (std::size_t index = 0; index < orderedPages.size(); ++index) {
      const QPointer<QWidget> page = orderedPages[index].page;
      if (page.isNull() || tabs.isNull() || stack.isNull()) {
        return false;
      }
      const int desiredIndex = static_cast<int>(index);
      if (stack->indexOf(page) == desiredIndex) {
        continue;
      }
      stack->removeWidget(page);
      if (m_publicWorkspace.isNull() || tabs.isNull() || stack.isNull() ||
          page.isNull()) {
        return false;
      }
      if (stack->insertWidget(desiredIndex, page) != desiredIndex ||
          m_publicWorkspace.isNull() || tabs.isNull() || stack.isNull() ||
          page.isNull()) {
        return false;
      }
    }
    if (stack->count() != tabs->count()) {
      return false;
    }
    for (std::size_t index = 0; index < orderedPages.size(); ++index) {
      if (orderedPages[index].page.isNull() ||
          stack->widget(static_cast<int>(index)) !=
              orderedPages[index].page) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool captureSnapshot() {
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    m_treeSnapshot = m_workspace->captureTreeSnapshot();
    m_originalIds = m_workspace->groupIds();
    m_originalSavedPages = m_workspace->savedPages;
    m_originalGroups.reserve(static_cast<std::size_t>(m_originalIds.size()));
    for (const ZzTabGroupId &id : m_originalIds) {
      ZzNode *const leafNode = m_workspace->findLeaf(id);
      QPointer<ZzTabWidget> tabs = leafNode != nullptr
                                       ? std::get<ZzLeaf>(leafNode->value).tabs
                                       : QPointer<ZzTabWidget>{};
      if (tabs.isNull()) {
        return false;
      }
      ZzWorkspaceLiveGroup group;
      group.id = id;
      group.tabs = tabs;
      group.identity = tabs.data();
      group.currentPage = tabs->currentWidget();
      group.pages.reserve(static_cast<std::size_t>(tabs->count()));
      for (int index = 0; index < tabs->count(); ++index) {
        group.pages.push_back(
            {tabs->widget(index), tabs->tabText(index), tabs->tabIcon(index),
             tabs->tabToolTip(index), tabs->tabWhatsThis(index),
             tabs->fluentTabBar()->tabData(index),
             tabs->fluentTabBar()->tabTextColor(index),
             tabs->isTabEnabled(index), tabs->isTabPinned(index),
             tabs->isTabModified(index), tabs->hasTabAttention(index),
             tabs->isTabCloseEnabled(index)});
      }
      m_originalGroups.push_back(std::move(group));
    }
    return true;
  }

  [[nodiscard]] bool buildStaging() {
    if (m_publicWorkspace.isNull() || m_workspace->rootHost == nullptr) {
      return false;
    }
    QPointer<QWidget> guardedRootHost = m_workspace->rootHost;
    m_stagedRoot =
        m_workspace->buildLayoutNode(m_state.root, guardedRootHost, nullptr);
    if (m_publicWorkspace.isNull() || guardedRootHost.isNull() ||
        m_stagedRoot == nullptr) {
      return false;
    }
    m_stagedRoot =
        ZzSplitWorkspacePrivate::normalize(std::move(m_stagedRoot), nullptr);
    m_stagedRootWidget =
        m_workspace->buildNodeWidget(m_stagedRoot.get(), guardedRootHost);
    if (m_publicWorkspace.isNull() || m_stagedRootWidget.isNull()) {
      return false;
    }
    m_stagedRootWidget->hide();
    if (m_publicWorkspace.isNull() || m_stagedRootWidget.isNull()) {
      return false;
    }
    zzRestoreWorkspaceLayoutSizes(m_state.root, m_stagedRoot.get());

    std::vector<ZzNode *> stagedLeaves;
    ZzSplitWorkspacePrivate::collectLeaves(m_stagedRoot.get(), stagedLeaves);
    for (ZzNode *leafNode : stagedLeaves) {
      const auto &leaf = std::get<ZzLeaf>(leafNode->value);
      if (leaf.tabs.isNull()) {
        return false;
      }
      m_stagedTabs.insert(leaf.id, leaf.tabs);
      m_restoredIds.push_back(leaf.id);
      m_stagedTabList.push_back(leaf.tabs);
    }
    m_escrowTabs = new ZzTabWidget;
    m_escrowTabs->hide();
    if (m_restoredIds.isEmpty() || m_escrowTabs.isNull()) {
      return false;
    }
    m_workspace->restoreTransactionOwners = m_stagedTabList;
    m_workspace->restoreTransactionOwners.push_back(m_escrowTabs);
    m_workspace->restoreTransactionKeyChanges.clear();
    return true;
  }

  [[nodiscard]] bool preparePlan() {
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    QHash<QString, ZzWorkspaceLayoutPage> savedByKey;
    QSet<QString> restoredGroupValues;
    for (const ZzTabGroupId &id : m_stagedTabs.keys()) {
      restoredGroupValues.insert(id.value());
    }
    for (const auto &page : m_state.pages) {
      savedByKey.insert(page.key, page);
    }

    int sequence = 0;
    for (const auto &group : m_originalGroups) {
      for (std::size_t index = 0; index < group.pages.size(); ++index) {
        const QPointer<QWidget> page = group.pages[index].page;
        if (page.isNull()) {
          return false;
        }
        const QString key = m_workspace->pageLayoutKey(page);
        ZzWorkspaceDesiredPage desired;
        desired.page = page;
        desired.originalTabs = group.tabs;
        desired.sequence = sequence++;
        const auto saved = savedByKey.constFind(key);
        if (!key.isEmpty() && saved != savedByKey.cend()) {
          desired.targetId = saved->groupId;
          desired.desiredOrder = saved->order;
          desired.savedOrder = true;
          m_livePagesByKey.insert(key, page);
        } else {
          desired.targetId = restoredGroupValues.contains(group.id.value())
                                 ? group.id
                                 : m_state.activeId;
          desired.desiredOrder = static_cast<int>(index);
        }
        m_desiredTargets.insert(page, desired.targetId);
        m_desiredPages[desired.targetId].push_back(std::move(desired));
      }
    }
    for (auto pageIt = m_desiredPages.begin(); pageIt != m_desiredPages.end();
         ++pageIt) {
      std::stable_sort(pageIt->begin(), pageIt->end(),
                       [](const ZzWorkspaceDesiredPage &left,
                          const ZzWorkspaceDesiredPage &right) {
                         if (left.desiredOrder != right.desiredOrder) {
                           return left.desiredOrder < right.desiredOrder;
                         }
                         if (left.savedOrder != right.savedOrder) {
                           return left.savedOrder;
                         }
                         return left.sequence < right.sequence;
                       });
    }
    return true;
  }

  [[nodiscard]] bool transferPages() {
    for (const ZzTabGroupId &targetId : m_restoredIds) {
      QPointer<ZzTabWidget> target = m_stagedTabs.value(targetId);
      auto &pages = m_desiredPages[targetId];
      if (target.isNull()) {
        return false;
      }
      for (const auto &desired : pages) {
        if (desired.page.isNull() || desired.originalTabs.isNull() ||
            desired.originalTabs->indexOf(desired.page) < 0) {
          return false;
        }
        const int sourceIndex = desired.originalTabs->indexOf(desired.page);
        if (!desired.originalTabs->transferTabTo(target, sourceIndex, -1)) {
          return false;
        }
        if (m_publicWorkspace.isNull()) {
          return false;
        }
        target = m_stagedTabs.value(targetId);
        if (desired.page.isNull() || target.isNull() ||
            zzOwningWorkspaceTabs(desired.page) != target) {
          return false;
        }
      }
    }

    return restoreStagedCurrentPages() && stagedViewValid();
  }

  [[nodiscard]] bool restoreStagedCurrentPages() {
    QHash<ZzTabGroupId, QPointer<QWidget>> desiredCurrentPages;
    for (const auto &saved : m_state.pages) {
      if (!saved.current) {
        continue;
      }
      const QPointer<QWidget> page = m_livePagesByKey.value(saved.key);
      if (!page.isNull() && m_desiredTargets.value(page) == saved.groupId) {
        desiredCurrentPages.insert(saved.groupId, page);
      }
    }
    for (const auto &group : m_originalGroups) {
      if (group.currentPage.isNull()) {
        continue;
      }
      const ZzTabGroupId targetId = m_desiredTargets.value(group.currentPage);
      if (targetId.isValid() && !desiredCurrentPages.contains(targetId)) {
        desiredCurrentPages.insert(targetId, group.currentPage);
      }
    }
    for (auto currentIt = desiredCurrentPages.cbegin();
         currentIt != desiredCurrentPages.cend(); ++currentIt) {
      QPointer<ZzTabWidget> target = m_stagedTabs.value(currentIt.key());
      const QPointer<QWidget> page = currentIt.value();
      if (target.isNull() || page.isNull() || target->indexOf(page) < 0) {
        return false;
      }
      if (!setCurrentPageSilently(target, page)) {
        return false;
      }
      target = m_stagedTabs.value(currentIt.key());
      if (target.isNull() || page.isNull() ||
          target->currentIndex() != target->indexOf(page) ||
          target->currentWidget() != page) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] const ZzWorkspaceLivePage *snapshotForPage(
      const QWidget *page) const {
    for (const auto &group : m_originalGroups) {
      const auto found = std::find_if(
          group.pages.cbegin(), group.pages.cend(),
          [page](const ZzWorkspaceLivePage &snapshot) {
            return !snapshot.page.isNull() && snapshot.page == page;
          });
      if (found != group.pages.cend()) {
        return &*found;
      }
    }
    return nullptr;
  }

  [[nodiscard]] ZzTabGroupId transactionGroupForOwner(
      const ZzTabWidget *owner) const {
    for (auto it = m_stagedTabs.cbegin(); it != m_stagedTabs.cend(); ++it) {
      if (!it.value().isNull() && it.value() == owner) {
        return it.key();
      }
    }
    for (const auto &group : m_originalGroups) {
      if (!group.tabs.isNull() && group.tabs.data() == group.identity &&
          group.tabs == owner) {
        return group.id;
      }
    }
    return {};
  }

  [[nodiscard]] static ZzWorkspaceLivePage capturePage(
      ZzTabWidget *tabs, int index) {
    return {tabs->widget(index),
            tabs->tabText(index),
            tabs->tabIcon(index),
            tabs->tabToolTip(index),
            tabs->tabWhatsThis(index),
            tabs->fluentTabBar()->tabData(index),
            tabs->fluentTabBar()->tabTextColor(index),
            tabs->isTabEnabled(index),
            tabs->isTabPinned(index),
            tabs->isTabModified(index),
            tabs->hasTabAttention(index),
            tabs->isTabCloseEnabled(index)};
  }

  [[nodiscard]] bool transferPageFromEscrow(
      const QPointer<ZzTabWidget> &target,
      const QPointer<QWidget> &page,
      int targetIndex,
      const ZzWorkspaceLivePage &snapshot) {
    const QPointer<ZzTabWidget> escrow = m_escrowTabs;
    if (m_publicWorkspace.isNull() || escrow.isNull() || target.isNull() ||
        target == escrow || page.isNull() ||
        zzOwningWorkspaceTabs(page) != escrow) {
      return false;
    }
    int sourceIndex = escrow->indexOf(page);
    if (sourceIndex < 0) {
      return false;
    }

    if (!zzRestoreWorkspacePageMetadata(escrow, snapshot, sourceIndex) ||
        m_publicWorkspace.isNull() || escrow.isNull() || target.isNull() ||
        page.isNull() || zzOwningWorkspaceTabs(page) != escrow) {
      return false;
    }
    sourceIndex = escrow->indexOf(page);
    const int sourceCount = escrow->count();
    if (sourceIndex < 0) {
      return false;
    }

    std::vector<std::unique_ptr<ZzScopedSignalMute>> signalMutes;
    muteEmitterTree(escrow, signalMutes);
    muteEmitterTree(target, signalMutes);
    ZzScopedTabTransferAcceptance transferAcceptance(target);
    if (m_publicWorkspace.isNull() || escrow.isNull() || target.isNull() ||
        page.isNull() || !transferAcceptance.isReady()) {
      return false;
    }

    const bool moved = escrow->transferTabTo(target, sourceIndex, targetIndex);
    if (m_publicWorkspace.isNull() || escrow.isNull() || target.isNull() ||
        page.isNull() || zzOwningWorkspaceTabs(page) != target) {
      return false;
    }
    if (escrow->count() == sourceCount) {
      const QPointer<ZzTabBar> sourceBar = escrow->fluentTabBar();
      sourceBar->removeTab(sourceIndex);
      if (m_publicWorkspace.isNull() || escrow.isNull() || target.isNull() ||
          sourceBar.isNull() || page.isNull()) {
        return false;
      }
    }
    if (!synchronizePinnedStackOrder(target)) {
      return false;
    }
    const QPointer<ZzTabBar> targetBar = target->fluentTabBar();
    const QPointer<QStackedWidget> targetStack =
        target->findChild<QStackedWidget *>(QString(),
                                           Qt::FindDirectChildrenOnly);
    if (targetBar.isNull() || targetStack.isNull()) {
      return false;
    }
    const int targetCurrentIndex = targetBar->currentIndex();
    targetStack->setCurrentIndex(targetCurrentIndex);
    if (m_publicWorkspace.isNull() || escrow.isNull() || target.isNull() ||
        targetBar.isNull() || targetStack.isNull() || page.isNull() ||
        targetStack->currentIndex() != targetCurrentIndex) {
      return false;
    }
    return moved && escrow->count() == sourceCount - 1 &&
           escrow->indexOf(page) < 0 && target->indexOf(page) >= 0 &&
           zzOwningWorkspaceTabs(page) == target;
  }

  [[nodiscard]] bool restoreCommittedPages() {
    if (m_escrowTabs.isNull()) {
      return false;
    }
    for (const ZzTabGroupId &targetId : m_restoredIds) {
      QPointer<ZzTabWidget> target = m_stagedTabs.value(targetId);
      const auto &pages = m_desiredPages[targetId];
      if (target.isNull()) {
        return false;
      }
      for (std::size_t index = 0; index < pages.size(); ++index) {
        const QPointer<QWidget> page = pages[index].page;
        const ZzWorkspaceLivePage *const snapshot =
            snapshotForPage(page);
        if (page.isNull() || snapshot == nullptr ||
            zzOwningWorkspaceTabs(page) != m_escrowTabs ||
            !transferPageFromEscrow(target, page,
                                    static_cast<int>(index), *snapshot)) {
          return false;
        }
        if (m_publicWorkspace.isNull() || m_escrowTabs.isNull()) {
          return false;
        }
        target = m_stagedTabs.value(targetId);
        if (target.isNull() || page.isNull() ||
            zzOwningWorkspaceTabs(page) != target ||
            target->indexOf(page) != static_cast<int>(index) ||
            !pageMetadataMatches(target, static_cast<int>(index),
                                 *snapshot)) {
          return false;
        }
      }
    }
    return restoreStagedCurrentPages() && stagedViewValid();
  }

  [[nodiscard]] bool pageMetadataMatches(
      const ZzTabWidget *tabs, int index,
      const ZzWorkspaceLivePage &snapshot) const {
    return tabs != nullptr && index >= 0 && index < tabs->count() &&
           tabs->tabText(index) == snapshot.text &&
           tabs->tabIcon(index).cacheKey() == snapshot.icon.cacheKey() &&
           tabs->tabToolTip(index) == snapshot.toolTip &&
           tabs->tabWhatsThis(index) == snapshot.whatsThis &&
           tabs->fluentTabBar()->tabData(index) == snapshot.data &&
           tabs->fluentTabBar()->tabTextColor(index) == snapshot.textColor &&
           tabs->isTabEnabled(index) == snapshot.enabled &&
           tabs->isTabPinned(index) == snapshot.pinned &&
           tabs->isTabModified(index) == snapshot.modified &&
           tabs->hasTabAttention(index) == snapshot.attention &&
           tabs->isTabCloseEnabled(index) == snapshot.closeEnabled;
  }

  [[nodiscard]] bool restoreOriginalGroup(
      const ZzWorkspaceLiveGroup &group) {
    constexpr int maximumGroupRestorePassCount = 16;
    for (int pass = 0; pass <= maximumGroupRestorePassCount; ++pass) {
      ZzNode *const originalNode = m_workspace->findLeaf(group.id);
      QPointer<ZzTabWidget> tabs =
          originalNode != nullptr ? std::get<ZzLeaf>(originalNode->value).tabs
                                  : QPointer<ZzTabWidget>{};
      if (tabs.isNull() || tabs.data() != group.identity) {
        return false;
      }

      bool restart = false;
      for (std::size_t index = 0; index < group.pages.size(); ++index) {
        const auto &snapshot = group.pages[index];
        const QPointer<QWidget> page = snapshot.page;
        if (page.isNull()) {
          continue;
        }
        ZzTabWidget *owner = zzOwningWorkspaceTabs(page);
        if (owner == m_escrowTabs) {
          const QPointer<ZzTabWidget> escrow = m_escrowTabs;
          const bool moved = transferPageFromEscrow(
              tabs, page, static_cast<int>(index), snapshot);
          if (m_publicWorkspace.isNull() || tabs.isNull() ||
              escrow.isNull() || page.isNull()) {
            return false;
          }
          owner = zzOwningWorkspaceTabs(page);
          if (owner == nullptr || owner == escrow) {
            restart = true;
            break;
          }
          if (owner != tabs) {
            continue;
          }
          if (!moved && tabs->indexOf(page) < 0) {
            restart = true;
            break;
          }
        } else if (owner != nullptr && owner != tabs) {
          continue;
        }
        if (m_publicWorkspace.isNull() || tabs.isNull()) {
          return false;
        }
      }
      if (restart) {
        continue;
      }

      if (!group.currentPage.isNull() &&
          zzOwningWorkspaceTabs(group.currentPage) == tabs) {
        if (!setCurrentPageSilently(tabs, group.currentPage)) {
          return false;
        }
        ZzTabWidget *const owner =
            zzOwningWorkspaceTabs(group.currentPage);
        if (owner == nullptr ||
            (owner == tabs && tabs->currentWidget() != group.currentPage)) {
          continue;
        }
      }

      int wantedIndex = 0;
      for (const auto &snapshot : group.pages) {
        const QPointer<QWidget> page = snapshot.page;
        if (page.isNull()) {
          continue;
        }
        ZzTabWidget *const owner = zzOwningWorkspaceTabs(page);
        if (owner != tabs) {
          if (owner == nullptr || owner == m_escrowTabs) {
            restart = true;
            break;
          }
          continue;
        }
        if (tabs->indexOf(page) != wantedIndex ||
            !pageMetadataMatches(tabs, wantedIndex, snapshot)) {
          restart = true;
          break;
        }
        ++wantedIndex;
      }
      if (!restart && !group.currentPage.isNull() &&
          zzOwningWorkspaceTabs(group.currentPage) == tabs &&
          tabs->currentWidget() != group.currentPage) {
        restart = true;
      }
      if (!restart) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool rollbackPages() {
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    bool pagesRestored = rescueTransactionPages();
    for (const auto &group : m_originalGroups) {
      pagesRestored = restoreOriginalGroup(group) && pagesRestored;
      if (m_publicWorkspace.isNull()) {
        return false;
      }
    }

    if (pagesRestored && originalViewValid()) {
      m_workspace->activeId = m_treeSnapshot.activeId;
      ZzSplitWorkspacePrivate::restoreNodeSizes(m_treeSnapshot.root,
                                                m_workspace->root.get());
    }
    m_workspace->savedPages = m_originalSavedPages;
    reconcileSavedPagesWithLiveKeys();
    restoreOldView();
    const bool publicTargetsReady = ensurePublicLeafTabs();
    const bool remainingPagesRestored = publicTargetsReady &&
                                        restoreRemainingCapturedPages();
    const bool preservedPagesRestored = restorePreservedPages();
    cleanupStaging();
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    const bool currentPagesRestored = restoreOriginalCurrentPages();
    pagesRestored = pagesRestored && remainingPagesRestored &&
                    preservedPagesRestored && currentPagesRestored;
    return false;
  }

  [[nodiscard]] bool commitView() {
    if (m_publicWorkspace.isNull() || !stagedViewValid() ||
        !originalViewValid() || m_workspace->rootLayout == nullptr ||
        m_workspace->rootLayout->count() != 1) {
      return rollbackPages();
    }
    if (!rescueTransactionPages() || !stagedStructureValid()) {
      return rollbackPages();
    }
    m_previousRootWidget = m_workspace->rootLayout->itemAt(0)->widget();
    if (m_previousRootWidget.isNull()) {
      return rollbackPages();
    }

    m_workspace->rootLayout->removeWidget(m_previousRootWidget);
    m_oldViewRemoved = true;
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    if (!m_previousRootWidget.isNull()) {
      m_previousRootWidget->hide();
      if (m_publicWorkspace.isNull()) {
        return false;
      }
    }
    if (m_stagedRootWidget.isNull() || m_workspace->rootLayout == nullptr) {
      return rollbackPages();
    }

    m_workspace->rootLayout->addWidget(m_stagedRootWidget);
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    if (m_stagedRootWidget.isNull() || m_workspace->rootLayout == nullptr ||
        m_workspace->rootLayout->count() != 1 ||
        m_workspace->rootLayout->itemAt(0)->widget() != m_stagedRootWidget) {
      return rollbackPages();
    }
    m_stagedRootWidget->show();
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    if (!stagedStructureValid()) {
      return rollbackPages();
    }

    m_previousRoot = std::move(m_workspace->root);
    m_workspace->root = std::move(m_stagedRoot);
    m_workspace->activeId = m_state.activeId;
    m_workspace->savedPages = m_state.pages;
    reconcileSavedPagesWithLiveKeys();
    m_modelCommitted = true;
    return cleanupOldView();
  }

  [[nodiscard]] bool cleanupOldView() {
    if (!m_previousRootWidget.isNull()) {
      delete m_previousRootWidget.data();
    }
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    if (!stagedStructureValid()) {
      return rollbackCommittedView();
    }
    if (!restoreCommittedPages()) {
      return m_publicWorkspace.isNull() ? false : rollbackCommittedView();
    }
    m_previousRoot.reset();
    m_oldViewRemoved = false;
    return m_modelCommitted && stagedViewValid() &&
           m_workspace->rootLayout != nullptr &&
           m_workspace->rootLayout->count() == 1 &&
           m_workspace->rootLayout->itemAt(0)->widget() == m_stagedRootWidget;
  }

  [[nodiscard]] bool rollbackCommittedView() {
    if (m_publicWorkspace.isNull() || !m_modelCommitted ||
        m_previousRoot == nullptr || m_workspace->rootLayout == nullptr) {
      return false;
    }
    if (!m_stagedRootWidget.isNull()) {
      if (!rescueTransactionPages() || m_publicWorkspace.isNull()) {
        return false;
      }
      m_workspace->rootLayout->removeWidget(m_stagedRootWidget);
      if (m_publicWorkspace.isNull()) {
        return false;
      }
      delete m_stagedRootWidget.data();
      if (m_publicWorkspace.isNull()) {
        return false;
      }
    }

    m_stagedRoot = std::move(m_workspace->root);
    m_workspace->root = std::move(m_previousRoot);
    m_workspace->activeId = m_treeSnapshot.activeId;
    m_workspace->savedPages = m_originalSavedPages;
    m_modelCommitted = false;
    m_oldViewRemoved = false;
    ZzSplitWorkspacePrivate::clearSplitterPointers(m_workspace->root.get());
    m_workspace->rebuildView();
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    for (auto &group : m_originalGroups) {
      ZzNode *const node = m_workspace->findLeaf(group.id);
      group.tabs = node != nullptr ? std::get<ZzLeaf>(node->value).tabs
                                   : QPointer<ZzTabWidget>{};
      group.identity = group.tabs.data();
    }
    return rollbackPages();
  }

  void restoreOldView() {
    if (!m_oldViewRemoved || m_publicWorkspace.isNull() ||
        m_workspace->rootLayout == nullptr) {
      return;
    }
    if (!m_stagedRootWidget.isNull()) {
      m_workspace->rootLayout->removeWidget(m_stagedRootWidget);
      if (m_publicWorkspace.isNull()) {
        return;
      }
    }
    if (!m_previousRootWidget.isNull()) {
      m_workspace->rootLayout->addWidget(m_previousRootWidget);
      if (m_publicWorkspace.isNull() || m_previousRootWidget.isNull()) {
        return;
      }
      m_previousRootWidget->show();
    }
    m_oldViewRemoved = false;
  }

  void cleanupStaging() {
    if (!m_stagedRootWidget.isNull()) {
      if (!m_publicWorkspace.isNull() && m_workspace->rootLayout != nullptr) {
        m_workspace->rootLayout->removeWidget(m_stagedRootWidget);
        if (m_publicWorkspace.isNull()) {
          m_stagedRoot.reset();
          return;
        }
      }
      delete m_stagedRootWidget.data();
    }
    m_stagedRoot.reset();
  }

  [[nodiscard]] bool isStagedOwner(const ZzTabWidget *owner) const {
    return std::any_of(
        m_stagedTabList.cbegin(), m_stagedTabList.cend(),
        [owner](const QPointer<ZzTabWidget> &tabs) {
          return !tabs.isNull() && tabs == owner;
        });
  }

  [[nodiscard]] bool isOriginalOwner(const ZzTabWidget *owner) const {
    return std::any_of(
        m_originalGroups.cbegin(), m_originalGroups.cend(),
        [owner](const ZzWorkspaceLiveGroup &group) {
          return !group.tabs.isNull() && group.tabs == owner &&
                 group.tabs.data() == group.identity;
        });
  }

  [[nodiscard]] bool rescueNonSnapshotPages() {
    if (m_publicWorkspace.isNull() || m_escrowTabs.isNull()) {
      return false;
    }
    std::vector<QPointer<ZzTabWidget>> transactionOwners = m_stagedTabList;
    transactionOwners.reserve(
        transactionOwners.size() + m_originalGroups.size());
    for (const auto &group : m_originalGroups) {
      if (group.tabs.isNull() || group.tabs.data() != group.identity) {
        continue;
      }
      const auto duplicate = std::find(
          transactionOwners.cbegin(), transactionOwners.cend(), group.tabs);
      if (duplicate == transactionOwners.cend()) {
        transactionOwners.push_back(group.tabs);
      }
    }

    for (const QPointer<ZzTabWidget> &owner : transactionOwners) {
      QPointer<ZzTabWidget> source = owner;
      int index = 0;
      while (!source.isNull() && index < source->count()) {
        const QPointer<QWidget> page = source->widget(index);
        if (page.isNull()) {
          return false;
        }
        if (snapshotForPage(page) != nullptr) {
          ++index;
          continue;
        }
        const auto alreadyPreserved = std::find_if(
            m_preservedPages.cbegin(), m_preservedPages.cend(),
            [page](const ZzWorkspacePreservedPage &preserved) {
              return !preserved.snapshot.page.isNull() &&
                     preserved.snapshot.page == page;
            });
        if (alreadyPreserved == m_preservedPages.cend()) {
          m_preservedPages.push_back(
              {capturePage(source, index),
               transactionGroupForOwner(source)});
        }

        const QPointer<ZzTabWidget> escrow = m_escrowTabs;
        const int sourceCount = source->count();
        const bool moved = source->transferTabTo(escrow, index);
        if (m_publicWorkspace.isNull() || source.isNull() || escrow.isNull() ||
            page.isNull()) {
          return false;
        }
        if (source->count() == sourceCount) {
          const QPointer<ZzTabBar> sourceBar = source->fluentTabBar();
          if (sourceBar.isNull()) {
            return false;
          }
          sourceBar->removeTab(index);
          if (m_publicWorkspace.isNull() || source.isNull() ||
              sourceBar.isNull() || escrow.isNull() || page.isNull()) {
            return false;
          }
        }
        if (!moved || source->count() != sourceCount - 1 ||
            source->indexOf(page) >= 0 ||
            zzOwningWorkspaceTabs(page) != escrow) {
          return false;
        }
      }
      if (source.isNull()) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] QPointer<ZzTabWidget> publicTargetTabs(
      const ZzTabGroupId &preferredId) const {
    if (m_publicWorkspace.isNull()) {
      return {};
    }
    const auto resolve = [this](const ZzTabGroupId &id) {
      ZzNode *const node = id.isValid() ? m_workspace->findLeaf(id) : nullptr;
      return node != nullptr
                 ? std::get<ZzLeaf>(node->value).tabs
                 : QPointer<ZzTabWidget>{};
    };
    QPointer<ZzTabWidget> target = resolve(preferredId);
    if (target.isNull()) {
      target = resolve(m_workspace->activeId);
    }
    if (!target.isNull()) {
      return target;
    }
    const QList<ZzTabGroupId> ids = m_workspace->groupIds();
    return ids.isEmpty() ? QPointer<ZzTabWidget>{} : resolve(ids.constFirst());
  }

  [[nodiscard]] bool ensurePublicLeafTabs() {
    if (m_publicWorkspace.isNull() || m_workspace->root == nullptr ||
        m_workspace->rootHost == nullptr ||
        m_workspace->rootLayout == nullptr) {
      return false;
    }
    std::vector<ZzNode *> leaves;
    ZzSplitWorkspacePrivate::collectLeaves(m_workspace->root.get(), leaves);
    const bool rebuildRequired = leaves.empty() || std::any_of(
        leaves.cbegin(), leaves.cend(), [](const ZzNode *leafNode) {
          return std::get<ZzLeaf>(leafNode->value).tabs.isNull();
        });
    if (rebuildRequired) {
      m_workspace->rebuildView();
      if (m_publicWorkspace.isNull()) {
        return false;
      }
      leaves.clear();
      ZzSplitWorkspacePrivate::collectLeaves(m_workspace->root.get(), leaves);
    }
    return !leaves.empty() &&
           std::all_of(
               leaves.cbegin(), leaves.cend(), [](const ZzNode *leafNode) {
                 return !std::get<ZzLeaf>(leafNode->value).tabs.isNull();
               }) &&
           m_workspace->rootLayout != nullptr &&
           m_workspace->rootLayout->count() == 1;
  }

  [[nodiscard]] bool restoreRemainingCapturedPages() {
    if (m_publicWorkspace.isNull() || m_escrowTabs.isNull()) {
      return false;
    }
    bool restoredAll = true;
    for (const auto &group : m_originalGroups) {
      for (const auto &snapshot : group.pages) {
        const QPointer<QWidget> page = snapshot.page;
        if (page.isNull()) {
          restoredAll = false;
          continue;
        }
        ZzTabWidget *const owner = zzOwningWorkspaceTabs(page);
        if (owner != m_escrowTabs) {
          continue;
        }
        QPointer<ZzTabWidget> target = publicTargetTabs(group.id);
        if (target.isNull() ||
            !transferPageFromEscrow(target, page, target->count(), snapshot) ||
            m_publicWorkspace.isNull() || target.isNull() || page.isNull()) {
          restoredAll = false;
          continue;
        }
        const int targetIndex = target->indexOf(page);
        if (targetIndex < 0 ||
            !pageMetadataMatches(target, targetIndex, snapshot)) {
          restoredAll = false;
        }
      }
    }
    return restoredAll;
  }

  [[nodiscard]] bool restorePreservedPages() {
    if (m_publicWorkspace.isNull() || m_escrowTabs.isNull()) {
      return false;
    }
    bool restoredAll = true;
    for (const auto &preserved : m_preservedPages) {
      const QPointer<QWidget> page = preserved.snapshot.page;
      if (page.isNull()) {
        restoredAll = false;
        continue;
      }
      ZzTabWidget *const owner = zzOwningWorkspaceTabs(page);
      if (owner != m_escrowTabs) {
        continue;
      }
      QPointer<ZzTabWidget> target = publicTargetTabs(
          preserved.preferredGroupId);
      if (target.isNull() ||
          !transferPageFromEscrow(target, page, target->count(),
                                  preserved.snapshot) ||
          m_publicWorkspace.isNull() || target.isNull() || page.isNull()) {
        restoredAll = false;
        continue;
      }
      const int targetIndex = target->indexOf(page);
      if (targetIndex < 0 ||
          !pageMetadataMatches(target, targetIndex, preserved.snapshot)) {
        restoredAll = false;
      }
    }
    return restoredAll;
  }

  [[nodiscard]] bool restoreOriginalCurrentPages() {
    if (m_publicWorkspace.isNull()) {
      return false;
    }
    bool restoredAll = true;
    for (const auto &group : m_originalGroups) {
      if (m_publicWorkspace.isNull()) {
        return false;
      }
      const QPointer<QWidget> currentPage = group.currentPage;
      if (currentPage.isNull()) {
        continue;
      }
      ZzTabWidget *const owner = zzOwningWorkspaceTabs(currentPage);
      if (owner == nullptr || m_workspace->findLeaf(owner) == nullptr) {
        continue;
      }
      if (!setCurrentPageSilently(owner, currentPage)) {
        restoredAll = false;
      }
      if (m_publicWorkspace.isNull()) {
        return false;
      }
    }
    for (const auto &group : m_originalGroups) {
      if (m_publicWorkspace.isNull()) {
        return false;
      }
      const QPointer<QWidget> currentPage = group.currentPage;
      if (currentPage.isNull()) {
        continue;
      }
      ZzTabWidget *const owner = zzOwningWorkspaceTabs(currentPage);
      if (owner != nullptr && m_workspace->findLeaf(owner) != nullptr &&
          owner->currentWidget() != currentPage) {
        restoredAll = false;
      }
    }
    return restoredAll;
  }

  void reconcileSavedPagesWithLiveKeys() {
    for (auto it = m_livePagesByKey.cbegin();
         it != m_livePagesByKey.cend(); ++it) {
      const QPointer<QWidget> page = it.value();
      const auto changed = page.isNull()
                               ? m_workspace->restoreTransactionKeyChanges.cend()
                               : m_workspace->restoreTransactionKeyChanges.constFind(
                                     page.data());
      if (changed == m_workspace->restoreTransactionKeyChanges.cend() ||
          changed.value() == it.key()) {
        continue;
      }
      m_workspace->savedPages.erase(
          std::remove_if(
              m_workspace->savedPages.begin(),
              m_workspace->savedPages.end(),
              [&it](const ZzWorkspaceLayoutPage &saved) {
                return saved.key == it.key();
              }),
          m_workspace->savedPages.end());
    }
  }

  [[nodiscard]] bool rescueTransactionPages() {
    if (m_publicWorkspace.isNull() || m_escrowTabs.isNull()) {
      return false;
    }

    bool rescuedAllPages = true;
    std::vector<std::unique_ptr<ZzScopedSignalMute>> signalMutes;
    muteEmitterTree(m_escrowTabs, signalMutes);
    for (const QPointer<ZzTabWidget> &tabs : m_stagedTabList) {
      if (!tabs.isNull()) {
        muteEmitterTree(tabs, signalMutes);
      }
    }
    for (const auto &group : m_originalGroups) {
      if (!group.tabs.isNull() && group.tabs.data() == group.identity) {
        muteEmitterTree(group.tabs, signalMutes);
      }
    }

    if (!rescueNonSnapshotPages()) {
      rescuedAllPages = false;
    }

    for (const auto &group : m_originalGroups) {
      for (const auto &snapshot : group.pages) {
        const QPointer<QWidget> page = snapshot.page;
        if (page.isNull()) {
          rescuedAllPages = false;
          continue;
        }
        QPointer<ZzTabWidget> owner = zzOwningWorkspaceTabs(page);
        if (owner == m_escrowTabs) {
          continue;
        }
        if (owner.isNull()) {
          if (page->parent() != nullptr) {
            continue;
          }
          const QPointer<ZzTabWidget> escrow = m_escrowTabs;
          const bool registered = zzRestoreWorkspacePageMetadata(
              escrow, snapshot, escrow->count());
          if (m_publicWorkspace.isNull() || escrow.isNull()) {
            return false;
          }
          if (!registered || page.isNull() ||
              zzOwningWorkspaceTabs(page) != escrow) {
            rescuedAllPages = false;
          }
          continue;
        }
        if (!isStagedOwner(owner) && !isOriginalOwner(owner)) {
          continue;
        }
        const QPointer<ZzTabWidget> escrow = m_escrowTabs;
        int sourceIndex = owner->indexOf(page);
        if (sourceIndex < 0) {
          const bool registered = zzRestoreWorkspacePageMetadata(
              owner, snapshot, owner->count());
          if (m_publicWorkspace.isNull() || escrow.isNull()) {
            return false;
          }
          if (!registered || owner.isNull() || page.isNull() ||
              zzOwningWorkspaceTabs(page) != owner) {
            rescuedAllPages = false;
            continue;
          }
          sourceIndex = owner->indexOf(page);
        }
        if (sourceIndex < 0) {
          rescuedAllPages = false;
          continue;
        }
        const int sourceCount = owner->count();
        const bool moved = owner->transferTabTo(escrow, sourceIndex);
        if (m_publicWorkspace.isNull() || escrow.isNull()) {
          return false;
        }
        if (page.isNull() || zzOwningWorkspaceTabs(page) != escrow) {
          rescuedAllPages = false;
          continue;
        }
        if (owner.isNull()) {
          rescuedAllPages = false;
          continue;
        }
        if (owner->count() == sourceCount) {
          const QPointer<ZzTabBar> sourceBar = owner->fluentTabBar();
          if (sourceBar.isNull()) {
            rescuedAllPages = false;
            continue;
          }
          sourceBar->removeTab(sourceIndex);
          if (m_publicWorkspace.isNull() || escrow.isNull()) {
            return false;
          }
          if (owner.isNull() || sourceBar.isNull() || page.isNull()) {
            rescuedAllPages = false;
            continue;
          }
        }
        if (!moved || owner->count() != sourceCount - 1 ||
            owner->indexOf(page) >= 0 ||
            zzOwningWorkspaceTabs(page) != escrow) {
          rescuedAllPages = false;
        }
      }
    }

    for (const auto &group : m_originalGroups) {
      for (const auto &snapshot : group.pages) {
        ZzTabWidget *const owner = zzOwningWorkspaceTabs(snapshot.page);
        if (snapshot.page.isNull() || isStagedOwner(owner) ||
            isOriginalOwner(owner)) {
          rescuedAllPages = false;
        }
      }
    }
    return rescuedAllPages;
  }

  [[nodiscard]] bool stagedStructureValid() const {
    if (m_publicWorkspace.isNull() || m_stagedRootWidget.isNull()) {
      return false;
    }
    return std::all_of(
        m_restoredIds.cbegin(), m_restoredIds.cend(),
        [this](const ZzTabGroupId &targetId) {
          return !m_stagedTabs.value(targetId).isNull();
        });
  }

  [[nodiscard]] bool stagedViewValid() const {
    if (m_publicWorkspace.isNull() || m_stagedRootWidget.isNull()) {
      return false;
    }
    for (const ZzTabGroupId &targetId : m_restoredIds) {
      const QPointer<ZzTabWidget> target = m_stagedTabs.value(targetId);
      const auto &pages = m_desiredPages[targetId];
      if (target.isNull() ||
          target->count() != static_cast<int>(pages.size())) {
        return false;
      }
      for (std::size_t index = 0; index < pages.size(); ++index) {
        if (pages[index].page.isNull() ||
            target->widget(static_cast<int>(index)) != pages[index].page ||
            zzOwningWorkspaceTabs(pages[index].page) != target) {
          return false;
        }
      }
    }
    return true;
  }

  [[nodiscard]] bool originalViewValid() const {
    if (m_publicWorkspace.isNull() ||
        m_workspace->groupIds() != m_originalIds) {
      return false;
    }
    for (const auto &group : m_originalGroups) {
      ZzNode *const node = m_workspace->findLeaf(group.id);
      const QPointer<ZzTabWidget> tabs =
          node != nullptr ? std::get<ZzLeaf>(node->value).tabs
                          : QPointer<ZzTabWidget>{};
      if (tabs.isNull() || tabs.data() != group.identity) {
        return false;
      }
    }
    return true;
  }

  ZzSplitWorkspacePrivate *m_workspace = nullptr;
  QPointer<ZzSplitWorkspace> m_publicWorkspace;
  ZzWorkspaceLayoutState m_state;
  ZzTreeSnapshot m_treeSnapshot;
  QList<ZzTabGroupId> m_originalIds;
  std::vector<ZzWorkspaceLayoutPage> m_originalSavedPages;
  std::vector<ZzWorkspaceLiveGroup> m_originalGroups;
  std::unique_ptr<ZzNode> m_stagedRoot;
  QPointer<QWidget> m_stagedRootWidget;
  QHash<ZzTabGroupId, QPointer<ZzTabWidget>> m_stagedTabs;
  QList<ZzTabGroupId> m_restoredIds;
  std::vector<QPointer<ZzTabWidget>> m_stagedTabList;
  QPointer<ZzTabWidget> m_escrowTabs;
  QHash<ZzTabGroupId, std::vector<ZzWorkspaceDesiredPage>> m_desiredPages;
  QHash<QWidget *, ZzTabGroupId> m_desiredTargets;
  QHash<QString, QPointer<QWidget>> m_livePagesByKey;
  std::vector<ZzWorkspacePreservedPage> m_preservedPages;
  QPointer<QWidget> m_previousRootWidget;
  std::unique_ptr<ZzNode> m_previousRoot;
  bool m_oldViewRemoved = false;
  bool m_modelCommitted = false;
};

/** @brief 绘制工作区唯一共享的当前拖放目标区域。 */
class ZzWorkspaceDropOverlay final : public QWidget
{
public:
    explicit ZzWorkspaceDropOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("zzSplitWorkspaceDropOverlay"));
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QColor fill = palette().color(QPalette::Highlight);
        fill.setAlpha(72);
        QColor stroke = palette().color(QPalette::Highlight);
        stroke.setAlpha(180);
        QPainter painter(this);
        painter.fillRect(rect(), fill);
        painter.setPen(QPen(stroke, 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

[[nodiscard]] QPoint zzWorkspacePosition(
    QWidget *watched,
    ZzSplitWorkspace *workspace,
    const QPoint &position)
{
    return watched == workspace
        ? position
        : watched->mapTo(workspace, position);
}

} // namespace

ZzNode::ZzNode(ZzBranch branch)
    : value(std::move(branch))
{
}

ZzNode::ZzNode(ZzLeaf leaf)
    : value(std::move(leaf))
{
}

ZzSplitWorkspacePrivate::ZzSplitWorkspacePrivate(
    ZzSplitWorkspace *publicObject)
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    auto *workspaceLayout = new QVBoxLayout(q_ptr);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    rootHost = new QWidget(q_ptr);
    rootLayout = new QVBoxLayout(rootHost);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    workspaceLayout->addWidget(rootHost);

    activeId = createGroupId();
    root = std::make_unique<ZzNode>(
        ZzLeaf {activeId, new ZzTabWidget(rootHost)});
    prepareTabs(std::get<ZzLeaf>(root->value).tabs);
    rootLayout->addWidget(std::get<ZzLeaf>(root->value).tabs);

    q_ptr->setAcceptDrops(true);

    QObject::connect(
        qApp,
        &QApplication::focusChanged,
        q_ptr,
        [this](QWidget *, QWidget *focused) {
            handleFocusChanged(focused);
        });
}

ZzSplitWorkspacePrivate::~ZzSplitWorkspacePrivate() = default;

QList<ZzTabGroupId> ZzSplitWorkspacePrivate::groupIds() const
{
    std::vector<const ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    QList<ZzTabGroupId> result;
    result.reserve(static_cast<qsizetype>(leaves.size()));
    for (const ZzNode *leafNode : leaves) {
        result.push_back(std::get<ZzLeaf>(leafNode->value).id);
    }
    return result;
}

ZzNode *ZzSplitWorkspacePrivate::findLeaf(
    const ZzTabGroupId &id) const noexcept
{
    if (!id.isValid()) {
        return nullptr;
    }
    std::vector<ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    const auto found = std::find_if(
        leaves.cbegin(),
        leaves.cend(),
        [&id](const ZzNode *node) {
            return std::get<ZzLeaf>(node->value).id == id;
        });
    return found != leaves.cend() ? *found : nullptr;
}

ZzNode *ZzSplitWorkspacePrivate::findLeaf(
    const ZzTabWidget *tabs) const noexcept
{
    if (tabs == nullptr) {
        return nullptr;
    }
    std::vector<ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    const auto found = std::find_if(
        leaves.cbegin(),
        leaves.cend(),
        [tabs](const ZzNode *node) {
            return std::get<ZzLeaf>(node->value).tabs == tabs;
        });
    return found != leaves.cend() ? *found : nullptr;
}

std::optional<ZzTabGroupId> ZzSplitWorkspacePrivate::splitGroup(
    const ZzTabGroupId &source,
    Qt::Orientation orientation,
    ZzSplitPlacement placement,
    const ZzTabGroupId &requestedId,
    bool rebuildViewAfterCommit)
{
    const QList<ZzTabGroupId> ids = groupIds();
    ZzNode *const sourceNode = findLeaf(source);
    if (sourceNode == nullptr
        || ids.size() >= maximumGroupCount
        || (orientation != Qt::Horizontal && orientation != Qt::Vertical)
        || (placement != ZzSplitPlacement::Before
            && placement != ZzSplitPlacement::After)) {
        return std::nullopt;
    }

    const ZzTabGroupId newId = requestedId.isValid()
        ? requestedId
        : createGroupId();
    if (std::find(ids.cbegin(), ids.cend(), newId) != ids.cend()) {
        return std::nullopt;
    }

    const bool mergesWithParent = sourceNode->parent != nullptr
        && std::holds_alternative<ZzBranch>(sourceNode->parent->value)
        && std::get<ZzBranch>(sourceNode->parent->value).orientation
            == orientation;
    if (!mergesWithParent
        && nodeDepth(sourceNode) >= maximumTreeDepth) {
        return std::nullopt;
    }

    auto newLeaf = std::make_unique<ZzNode>(
        ZzLeaf {newId, new ZzTabWidget(rootHost)});
    prepareTabs(std::get<ZzLeaf>(newLeaf->value).tabs);
    if (mergesWithParent) {
        ZzNode *const parent = sourceNode->parent;
        auto &siblings = std::get<ZzBranch>(parent->value).children;
        const auto sourceIt = std::find_if(
            siblings.begin(),
            siblings.end(),
            [sourceNode](const auto &child) {
                return child.get() == sourceNode;
            });
        Q_ASSERT(sourceIt != siblings.end());
        const auto offset = placement == ZzSplitPlacement::After ? 1 : 0;
        newLeaf->parent = parent;
        siblings.insert(sourceIt + offset, std::move(newLeaf));
    } else {
        ZzNode *const oldParent = sourceNode->parent;
        std::unique_ptr<ZzNode> *owner = &root;
        if (oldParent != nullptr) {
            auto &siblings = std::get<ZzBranch>(oldParent->value).children;
            const auto sourceIt = std::find_if(
                siblings.begin(),
                siblings.end(),
                [sourceNode](const auto &child) {
                    return child.get() == sourceNode;
                });
            Q_ASSERT(sourceIt != siblings.end());
            owner = &*sourceIt;
        }

        auto oldLeaf = std::move(*owner);
        ZzBranch branch;
        branch.orientation = orientation;
        if (placement == ZzSplitPlacement::Before) {
            branch.children.push_back(std::move(newLeaf));
            branch.children.push_back(std::move(oldLeaf));
        } else {
            branch.children.push_back(std::move(oldLeaf));
            branch.children.push_back(std::move(newLeaf));
        }
        auto replacement = std::make_unique<ZzNode>(std::move(branch));
        replacement->parent = oldParent;
        for (auto &child : std::get<ZzBranch>(replacement->value).children) {
            child->parent = replacement.get();
        }
        *owner = std::move(replacement);
    }

    root = normalize(std::move(root), nullptr);
    if (rebuildViewAfterCommit) {
        rebuildView();
    }
    return newId;
}

bool ZzSplitWorkspacePrivate::removeEmptyGroup(
    const ZzTabGroupId &id,
    bool rebuildViewAfterCommit)
{
    const QList<ZzTabGroupId> ids = groupIds();
    ZzNode *const leafNode = findLeaf(id);
    const QPointer<ZzTabWidget> tabs = leafNode != nullptr
        ? std::get<ZzLeaf>(leafNode->value).tabs
        : QPointer<ZzTabWidget> {};
    if (ids.size() <= 1 || leafNode == nullptr
        || (!tabs.isNull() && tabs->count() != 0)) {
        return false;
    }

    ZzNode *const parent = leafNode->parent;
    Q_ASSERT(parent != nullptr);
    auto &siblings = std::get<ZzBranch>(parent->value).children;
    const auto leafIt = std::find_if(
        siblings.begin(),
        siblings.end(),
        [leafNode](const auto &child) {
            return child.get() == leafNode;
        });
    Q_ASSERT(leafIt != siblings.end());
    siblings.erase(leafIt);

    root = normalize(std::move(root), nullptr);
    if (activeId == id) {
        activeId = groupIds().constFirst();
    }
    if (rebuildViewAfterCommit) {
        rebuildView();
    }
    return true;
}

ZzTabGroupId ZzSplitWorkspacePrivate::adjacentGroup(
    Qt::Edge direction) const
{
    if (direction != Qt::LeftEdge && direction != Qt::TopEdge
        && direction != Qt::RightEdge && direction != Qt::BottomEdge) {
        return {};
    }

    ZzNode *const activeNode = findLeaf(activeId);
    if (activeNode == nullptr) {
        return {};
    }
    const auto *activeTabs =
        std::get<ZzLeaf>(activeNode->value).tabs.data();
    if (activeTabs == nullptr) {
        return {};
    }
    const QPoint activeCenter = activeTabs->mapToGlobal(
        activeTabs->rect().center());

    std::vector<const ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    const ZzNode *best = nullptr;
    int bestPrimary = std::numeric_limits<int>::max();
    int bestSecondary = std::numeric_limits<int>::max();
    qsizetype bestOrder = std::numeric_limits<qsizetype>::max();
    for (qsizetype index = 0;
         index < static_cast<qsizetype>(leaves.size());
         ++index) {
        const ZzNode *candidate = leaves[static_cast<std::size_t>(index)];
        if (candidate == activeNode) {
            continue;
        }
        const auto *tabs = std::get<ZzLeaf>(candidate->value).tabs.data();
        if (tabs == nullptr) {
            continue;
        }
        const QPoint center = tabs->mapToGlobal(tabs->rect().center());
        const int deltaX = center.x() - activeCenter.x();
        const int deltaY = center.y() - activeCenter.y();
        const bool inDirection = direction == Qt::LeftEdge ? deltaX < 0
            : direction == Qt::RightEdge                  ? deltaX > 0
            : direction == Qt::TopEdge                    ? deltaY < 0
                                                          : deltaY > 0;
        if (!inDirection) {
            continue;
        }
        const int primary = (direction == Qt::LeftEdge
                             || direction == Qt::RightEdge)
            ? std::abs(deltaX)
            : std::abs(deltaY);
        const int secondary = (direction == Qt::LeftEdge
                               || direction == Qt::RightEdge)
            ? std::abs(deltaY)
            : std::abs(deltaX);
        if (primary < bestPrimary
            || (primary == bestPrimary && secondary < bestSecondary)
            || (primary == bestPrimary && secondary == bestSecondary
                && index < bestOrder)) {
            best = candidate;
            bestPrimary = primary;
            bestSecondary = secondary;
            bestOrder = index;
        }
    }
    return best != nullptr ? std::get<ZzLeaf>(best->value).id
                           : ZzTabGroupId {};
}

bool ZzSplitWorkspacePrivate::transferTab(
    const ZzTabGroupId &source,
    int sourceIndex,
    const ZzTabGroupId &target,
    int targetIndex)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = q_ptr;
    ZzNode *const sourceNode = findLeaf(source);
    ZzNode *const targetNode = findLeaf(target);
    QPointer<ZzTabWidget> sourceTabs = sourceNode != nullptr
        ? std::get<ZzLeaf>(sourceNode->value).tabs
        : QPointer<ZzTabWidget> {};
    QPointer<ZzTabWidget> targetTabs = targetNode != nullptr
        ? std::get<ZzLeaf>(targetNode->value).tabs
        : QPointer<ZzTabWidget> {};
    if (sourceTabs.isNull() || targetTabs.isNull()
        || sourceIndex < 0 || sourceIndex >= sourceTabs->count()) {
        return false;
    }
    QPointer<QWidget> page = sourceTabs->widget(sourceIndex);
    if (page.isNull()) {
        return false;
    }

    const bool transferred = sourceTabs->transferTabTo(
        targetTabs, sourceIndex, targetIndex);
    if (guardedWorkspace.isNull()) {
        return transferred;
    }
    if (!transferred || sourceTabs.isNull() || targetTabs.isNull()
        || page.isNull()) {
        return false;
    }
    ZzNode *const resolvedTargetNode = findLeaf(target);
    const QPointer<ZzTabWidget> resolvedTarget =
        resolvedTargetNode != nullptr
        ? std::get<ZzLeaf>(resolvedTargetNode->value).tabs
        : QPointer<ZzTabWidget> {};
    return resolvedTarget == targetTabs
        && resolvedTarget->indexOf(page) >= 0;
}

bool ZzSplitWorkspacePrivate::setPageLayoutKey(
    QWidget *page,
    const QString &key)
{
    pageKeys.erase(
        std::remove_if(
            pageKeys.begin(),
            pageKeys.end(),
            [this](const ZzWorkspacePageKey &entry) {
                return entry.page.isNull()
                    || !zzWorkspaceContainsPage(this, entry.page);
            }),
        pageKeys.end());

    if (page == nullptr || !zzWorkspaceContainsPage(this, page)) {
        return false;
    }

    const QString normalized = key.trimmed();
    if (normalized.size() > zzWorkspaceMaximumStringLength) {
        return false;
    }
    const auto existing = std::find_if(
        pageKeys.begin(),
        pageKeys.end(),
        [page](const ZzWorkspacePageKey &entry) {
            return entry.page == page;
        });
    if (!normalized.isEmpty()) {
        const auto duplicate = std::find_if(
            pageKeys.cbegin(),
            pageKeys.cend(),
            [&normalized, page](const ZzWorkspacePageKey &entry) {
                return entry.page != page && entry.key == normalized;
            });
        if (duplicate != pageKeys.cend()) {
            return false;
        }
    }

    const QString previousKey = existing != pageKeys.end()
        ? existing->key
        : QString {};
    if (normalized.isEmpty()) {
        if (existing != pageKeys.end()) {
            pageKeys.erase(existing);
        }
    } else if (existing != pageKeys.end()) {
        existing->key = normalized;
    } else {
        pageKeys.push_back({page, normalized});
    }
    if (!previousKey.isEmpty() && previousKey != normalized) {
        savedPages.erase(
            std::remove_if(
                savedPages.begin(),
                savedPages.end(),
                [&previousKey](const ZzWorkspaceLayoutPage &saved) {
                    return saved.key == previousKey;
                }),
            savedPages.end());
    }
    if (!restoreTransactionOwners.empty()) {
        restoreTransactionKeyChanges.insert(page, normalized);
    }
    return true;
}

QString ZzSplitWorkspacePrivate::pageLayoutKey(
    const QWidget *page) const
{
    if (!zzWorkspaceContainsPage(this, page)) {
        return {};
    }
    const auto found = std::find_if(
        pageKeys.cbegin(),
        pageKeys.cend(),
        [page](const ZzWorkspacePageKey &entry) {
            return !entry.page.isNull() && entry.page == page;
        });
    return found != pageKeys.cend() ? found->key : QString {};
}

QByteArray ZzSplitWorkspacePrivate::saveLayout() const
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(zzWorkspaceLayoutStreamVersion);
    int nodeCount = 0;
    int groupCount = 0;
    if (!zzWriteWorkspaceNode(
            stream, root.get(), 1, &nodeCount, &groupCount)
        || !activeId.isValid()
        || activeId.value().size() > zzWorkspaceMaximumStringLength) {
        return {};
    }
    zzWriteWorkspaceString(stream, activeId.value());

    const QList<ZzTabGroupId> ids = groupIds();
    QHash<ZzTabGroupId, int> groupOrder;
    QSet<QString> groupValues;
    for (qsizetype index = 0; index < ids.size(); ++index) {
        groupOrder.insert(ids.at(index), static_cast<int>(index));
        groupValues.insert(ids.at(index).value());
    }

    std::vector<ZzWorkspaceLayoutPage> pages;
    QSet<QString> liveKeys;
    QSet<QString> missingKeys;
    for (const ZzTabGroupId &id : ids) {
        ZzNode *const leafNode = findLeaf(id);
        const QPointer<ZzTabWidget> tabs = leafNode != nullptr
            ? std::get<ZzLeaf>(leafNode->value).tabs
            : QPointer<ZzTabWidget> {};
        if (tabs.isNull()) {
            return {};
        }
        for (int index = 0; index < tabs->count(); ++index) {
            QWidget *const page = tabs->widget(index);
            const QString key = pageLayoutKey(page);
            if (key.isEmpty()) {
                continue;
            }
            if (key.size() > zzWorkspaceMaximumStringLength
                || liveKeys.contains(key)
                || index > zzWorkspaceMaximumPageOrder) {
                return {};
            }
            liveKeys.insert(key);
            pages.push_back(
                {key, id, index, tabs->currentWidget() == page});
        }
    }
    for (const auto &saved : savedPages) {
        if (!liveKeys.contains(saved.key)
            && groupValues.contains(saved.groupId.value())) {
            pages.push_back(saved);
            missingKeys.insert(saved.key);
        }
    }
    if (pages.size()
        > static_cast<std::size_t>(zzWorkspaceMaximumSavedPageCount)) {
        return {};
    }
    std::stable_sort(
        pages.begin(),
        pages.end(),
        [&groupOrder, &missingKeys](
            const ZzWorkspaceLayoutPage &left,
            const ZzWorkspaceLayoutPage &right) {
            return std::tuple(
                       groupOrder.value(left.groupId),
                       left.order,
                       !missingKeys.contains(left.key),
                       left.key)
                < std::tuple(
                       groupOrder.value(right.groupId),
                       right.order,
                       !missingKeys.contains(right.key),
                       right.key);
        });
    ZzTabGroupId orderedGroup;
    int nextOrder = 0;
    for (auto &page : pages) {
        if (page.groupId != orderedGroup) {
            orderedGroup = page.groupId;
            nextOrder = 0;
        }
        page.order = std::max(page.order, nextOrder);
        if (page.order > zzWorkspaceMaximumPageOrder) {
            return {};
        }
        nextOrder = page.order + 1;
    }
    QSet<QString> groupsWithCurrent;
    for (auto &page : pages) {
        if (page.current
            && groupsWithCurrent.contains(page.groupId.value())) {
            page.current = false;
        }
        if (page.current) {
            groupsWithCurrent.insert(page.groupId.value());
        }
    }

    stream << static_cast<quint16>(pages.size());
    for (const auto &page : pages) {
        zzWriteWorkspaceString(stream, page.key);
        zzWriteWorkspaceString(stream, page.groupId.value());
        stream << static_cast<qint32>(page.order)
               << static_cast<quint8>(page.current ? 1 : 0);
    }
    if (stream.status() != QDataStream::Ok
        || payload.size() > zzWorkspaceMaximumPayloadSize) {
        return {};
    }

    QByteArray encoded;
    encoded.reserve(
        zzWorkspaceLayoutHeaderSize + payload.size()
        + zzWorkspaceLayoutDigestSize);
    QDataStream envelope(&encoded, QIODevice::WriteOnly);
    envelope.setVersion(zzWorkspaceLayoutStreamVersion);
    envelope.writeRawData(zzWorkspaceLayoutMagic, 4);
    envelope << zzWorkspaceLayoutSchemaVersion
             << static_cast<quint16>(zzWorkspaceLayoutStreamVersion)
             << static_cast<quint32>(payload.size());
    if (envelope.writeRawData(payload.constData(), payload.size())
            != payload.size()
        || envelope.status() != QDataStream::Ok) {
        return {};
    }
    encoded.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return encoded;
}

bool ZzSplitWorkspacePrivate::restoreLayout(const QByteArray &encoded) {
  if (!restoreTransactionOwners.empty()) {
    return false;
  }
  const auto decoded = zzDecodeWorkspaceLayout(encoded);
  if (!decoded.has_value()) {
    return false;
  }
  return ZzWorkspaceRestoreTransaction(this, decoded.value()).run();
}

ZzTabGroupId ZzSplitWorkspacePrivate::savedGroupForPageKey(
    const QString &key) const
{
    const QString normalized = key.trimmed();
    if (normalized.isEmpty()
        || normalized.size() > zzWorkspaceMaximumStringLength) {
        return {};
    }
    const auto found = std::find_if(
        savedPages.cbegin(),
        savedPages.cend(),
        [&normalized](const ZzWorkspaceLayoutPage &saved) {
            return saved.key == normalized;
        });
    return found != savedPages.cend() ? found->groupId
                                     : ZzTabGroupId {};
}

ZzWorkspaceTransferResult ZzSplitWorkspacePrivate::moveTabToDropZone(
    const ZzTabGroupId &source,
    int sourceIndex,
    const ZzTabGroupId &target,
    ZzWorkspaceDropZone zone)
{
    ZzWorkspaceTransferResult result;
    result.sourceId = source;
    result.zone = zone;
    switch (zone) {
    case ZzWorkspaceDropZone::Center:
    case ZzWorkspaceDropZone::Left:
    case ZzWorkspaceDropZone::Top:
    case ZzWorkspaceDropZone::Right:
    case ZzWorkspaceDropZone::Bottom:
        break;
    default:
        return result;
    }

    QPointer<ZzSplitWorkspace> guardedWorkspace = q_ptr;
    ZzNode *const sourceNode = findLeaf(source);
    ZzNode *const targetNode = findLeaf(target);
    QPointer<ZzTabWidget> sourceTabs = sourceNode != nullptr
        ? std::get<ZzLeaf>(sourceNode->value).tabs
        : QPointer<ZzTabWidget> {};
    QPointer<ZzTabWidget> originalTargetTabs = targetNode != nullptr
        ? std::get<ZzLeaf>(targetNode->value).tabs
        : QPointer<ZzTabWidget> {};
    if (sourceTabs.isNull() || originalTargetTabs.isNull()
        || sourceIndex < 0 || sourceIndex >= sourceTabs->count()) {
        return result;
    }
    QPointer<QWidget> page = sourceTabs->widget(sourceIndex);
    if (page.isNull()) {
        return result;
    }
    result.page = page;

    if (zone == ZzWorkspaceDropZone::Center) {
        result.committed = transferTab(source, sourceIndex, target, -1);
        result.destinationId = target;
        return result;
    }

    const Qt::Orientation orientation =
        zone == ZzWorkspaceDropZone::Left
            || zone == ZzWorkspaceDropZone::Right
        ? Qt::Horizontal
        : Qt::Vertical;
    ZzSplitPlacement placement =
        zone == ZzWorkspaceDropZone::Left
            || zone == ZzWorkspaceDropZone::Top
        ? ZzSplitPlacement::Before
        : ZzSplitPlacement::After;
    if (orientation == Qt::Horizontal
        && q_ptr->layoutDirection() == Qt::RightToLeft) {
        placement = placement == ZzSplitPlacement::Before
            ? ZzSplitPlacement::After
            : ZzSplitPlacement::Before;
    }

    const ZzTreeSnapshot snapshot = captureTreeSnapshot();
    const auto temporaryId = splitGroup(
        target, orientation, placement, {}, false);
    if (!temporaryId.has_value() || guardedWorkspace.isNull()) {
        return result;
    }
    result.destinationId = temporaryId.value();
    sourceTabs = findLeaf(source) != nullptr
        ? std::get<ZzLeaf>(findLeaf(source)->value).tabs
        : QPointer<ZzTabWidget> {};
    ZzNode *const temporaryNode = findLeaf(temporaryId.value());
    QPointer<ZzTabWidget> temporaryTabs = temporaryNode != nullptr
        ? std::get<ZzLeaf>(temporaryNode->value).tabs
        : QPointer<ZzTabWidget> {};
    const auto discardTemporary = [&]() {
        if (guardedWorkspace.isNull()) {
            return;
        }
        ZzNode *const currentTemporaryNode = findLeaf(temporaryId.value());
        const QPointer<ZzTabWidget> currentTemporaryTabs =
            currentTemporaryNode != nullptr
            ? std::get<ZzLeaf>(currentTemporaryNode->value).tabs
            : QPointer<ZzTabWidget> {};
        const bool sameTemporaryIdentity = currentTemporaryNode != nullptr
            && (currentTemporaryTabs == temporaryTabs
                || (currentTemporaryTabs.isNull()
                    && temporaryTabs.isNull()));
        if (!sameTemporaryIdentity) {
            if (!temporaryTabs.isNull() && temporaryTabs->count() == 0) {
                delete temporaryTabs.data();
            }
            return;
        }
        if (removeEmptyGroup(temporaryId.value(), false)) {
            delete temporaryTabs.data();
            return;
        }
        restoreTreeSnapshot(snapshot);
    };
    if (sourceTabs.isNull() || temporaryTabs.isNull() || page.isNull()
        || sourceTabs->indexOf(page) != sourceIndex
        || !sourceTabs->transferTabTo(temporaryTabs, sourceIndex)) {
        discardTemporary();
        return result;
    }
    if (guardedWorkspace.isNull()) {
        result.committed = true;
        return result;
    }

    ZzNode *const resolvedTemporaryNode = findLeaf(temporaryId.value());
    const QPointer<ZzTabWidget> resolvedTemporary =
        resolvedTemporaryNode != nullptr
        ? std::get<ZzLeaf>(resolvedTemporaryNode->value).tabs
        : QPointer<ZzTabWidget> {};
    if (page.isNull() || temporaryTabs.isNull()
        || resolvedTemporary != temporaryTabs
        || resolvedTemporary->indexOf(page) < 0) {
        discardTemporary();
        return result;
    }

    ZzNode *const resolvedOriginalTargetNode = findLeaf(target);
    const QPointer<ZzTabWidget> resolvedOriginalTarget =
        resolvedOriginalTargetNode != nullptr
        ? std::get<ZzLeaf>(resolvedOriginalTargetNode->value).tabs
        : QPointer<ZzTabWidget> {};
    if (originalTargetTabs.isNull()
        || resolvedOriginalTarget != originalTargetTabs) {
        ZzNode *const resolvedSourceNode = findLeaf(source);
        QPointer<ZzTabWidget> resolvedSource =
            resolvedSourceNode != nullptr
            ? std::get<ZzLeaf>(resolvedSourceNode->value).tabs
            : QPointer<ZzTabWidget> {};
        const int temporaryIndex = temporaryTabs->indexOf(page);
        if (!resolvedSource.isNull() && temporaryIndex >= 0) {
            temporaryTabs->transferTabTo(
                resolvedSource, temporaryIndex, sourceIndex);
        }
        if (guardedWorkspace.isNull()) {
            return result;
        }
        if (!temporaryTabs.isNull() && temporaryTabs->count() == 0) {
            discardTemporary();
            if (guardedWorkspace.isNull()) {
                return result;
            }
        }
        ZzNode *const invalidTargetNode = findLeaf(target);
        const QPointer<ZzTabWidget> invalidTargetTabs =
            invalidTargetNode != nullptr
            ? std::get<ZzLeaf>(invalidTargetNode->value).tabs
            : QPointer<ZzTabWidget> {};
        if (invalidTargetNode != nullptr && invalidTargetTabs.isNull()) {
            removeEmptyGroup(target, false);
        }
        rebuildView();
        return result;
    }

    const ZzTabGroupId activeBeforeSourceRemoval = activeId;
    ZzNode *const currentSourceNode = findLeaf(source);
    const QPointer<ZzTabWidget> currentSourceTabs =
        currentSourceNode != nullptr
        ? std::get<ZzLeaf>(currentSourceNode->value).tabs
        : QPointer<ZzTabWidget> {};
    const bool sameSourceIdentity = currentSourceNode != nullptr
        && (currentSourceTabs == sourceTabs
            || (currentSourceTabs.isNull() && sourceTabs.isNull()));
    if (sameSourceIdentity) {
        result.sourceRemoved = removeEmptyGroup(source);
        if (guardedWorkspace.isNull()) {
            result.committed = true;
            return result;
        }
    }
    if (!result.sourceRemoved) {
        rebuildView();
        if (guardedWorkspace.isNull()) {
            result.committed = true;
            return result;
        }
    }

    const bool activeChanged = activeId != temporaryId.value()
        || (result.sourceRemoved && activeBeforeSourceRemoval == source);
    activeId = temporaryId.value();
    ZzNode *const activeNode = findLeaf(temporaryId.value());
    QPointer<ZzTabWidget> activeTabs = activeNode != nullptr
        ? std::get<ZzLeaf>(activeNode->value).tabs
        : QPointer<ZzTabWidget> {};
    if (!activeTabs.isNull()) {
        activeTabs->setFocus(Qt::OtherFocusReason);
    }
    result.committed = true;
    result.layoutChanged = true;
    result.groupAdded = true;
    result.activeChanged = activeChanged;
    return result;
}

ZzTreeSnapshot ZzSplitWorkspacePrivate::captureTreeSnapshot() const
{
    return ZzTreeSnapshot {captureNodeSnapshot(root.get()), activeId};
}

bool ZzSplitWorkspacePrivate::restoreTreeSnapshot(
    const ZzTreeSnapshot &snapshot)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = q_ptr;
    root = buildSnapshotNode(snapshot.root, nullptr);
    activeId = snapshot.activeId;
    rebuildView();
    if (guardedWorkspace.isNull()) {
        return false;
    }
    restoreNodeSizes(snapshot.root, root.get());
    return true;
}

bool ZzSplitWorkspacePrivate::eventFilter(QObject *watched, QEvent *event)
{
    auto *watchedWidget = qobject_cast<QWidget *>(watched);
    if (watchedWidget == nullptr || event == nullptr) {
        return false;
    }
    switch (event->type()) {
    case QEvent::DragEnter:
        return handleDragEnter(
            watchedWidget, static_cast<QDragEnterEvent *>(event));
    case QEvent::DragMove:
        return handleDragMove(
            watchedWidget, static_cast<QDragMoveEvent *>(event));
    case QEvent::DragLeave:
        return handleDragLeave(static_cast<QDragLeaveEvent *>(event));
    case QEvent::Drop:
        return handleDrop(watchedWidget, static_cast<QDropEvent *>(event));
    default:
        return false;
    }
}

bool ZzSplitWorkspacePrivate::handleDragEnter(
    QWidget *watched,
    QDragEnterEvent *event)
{
    if (watched == nullptr || event == nullptr
        || !ensureDragToken(event->mimeData())) {
        if (event != nullptr) {
            event->ignore();
        }
        discardDragTokens();
        hideDropOverlay();
        return event != nullptr
            && (event->mimeData()->hasFormat(
                    QString::fromLatin1(zzWorkspaceTabMimeType))
                || event->mimeData()->hasFormat(ZzTabMimeData::format()));
    }
    const QPoint position = zzWorkspacePosition(
        watched, q_ptr, event->position().toPoint());
    const ZzTabGroupId target = groupAt(position);
    if (!target.isValid()) {
        event->ignore();
        hideDropOverlay();
        return true;
    }
    showDropOverlay(dropZoneRect(target, dropZoneAt(target, position)));
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

bool ZzSplitWorkspacePrivate::handleDragMove(
    QWidget *watched,
    QDragMoveEvent *event)
{
    if (watched == nullptr || event == nullptr
        || !dragRecord(event->mimeData()).has_value()) {
        if (event != nullptr) {
            event->ignore();
        }
        discardDragTokens();
        hideDropOverlay();
        return event != nullptr
            && event->mimeData()->hasFormat(
                QString::fromLatin1(zzWorkspaceTabMimeType));
    }
    const QPoint position = zzWorkspacePosition(
        watched, q_ptr, event->position().toPoint());
    const ZzTabGroupId target = groupAt(position);
    if (!target.isValid()) {
        event->ignore();
        hideDropOverlay();
        return true;
    }
    showDropOverlay(dropZoneRect(target, dropZoneAt(target, position)));
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

bool ZzSplitWorkspacePrivate::handleDragLeave(QDragLeaveEvent *event)
{
    hideDropOverlay();
    discardDragTokens();
    if (event == nullptr) {
        return false;
    }
    event->accept();
    return true;
}

bool ZzSplitWorkspacePrivate::handleDrop(
    QWidget *watched,
    QDropEvent *event)
{
    const auto record = event == nullptr
        ? std::nullopt
        : dragRecord(event->mimeData());
    if (watched == nullptr || event == nullptr || !record.has_value()) {
        if (event != nullptr) {
            event->ignore();
        }
        discardDragTokens();
        hideDropOverlay();
        return event != nullptr
            && event->mimeData()->hasFormat(
                QString::fromLatin1(zzWorkspaceTabMimeType));
    }
    const QPoint position = zzWorkspacePosition(
        watched, q_ptr, event->position().toPoint());
    const ZzTabGroupId target = groupAt(position);
    if (!target.isValid()) {
        event->ignore();
        hideDropOverlay();
        discardDragTokens();
        return true;
    }
    const ZzWorkspaceDropZone zone = dropZoneAt(target, position);
    const ZzWorkspaceDragRecord stableRecord = record.value();
    discardDragTokens();
    hideDropOverlay();
    const bool committed = q_ptr->moveTabToDropZone(
        stableRecord.sourceId,
        stableRecord.sourceIndex,
        target,
        zone);
    if (!committed) {
        event->ignore();
        return true;
    }
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

void ZzSplitWorkspacePrivate::handleFocusChanged(QWidget *focused)
{
    for (QWidget *current = focused;
         current != nullptr && current != q_ptr;
         current = current->parentWidget()) {
        if (auto *tabs = qobject_cast<ZzTabWidget *>(current);
            tabs != nullptr) {
            ZzNode *const node = findLeaf(tabs);
            if (node == nullptr) {
                return;
            }
            const ZzTabGroupId id = std::get<ZzLeaf>(node->value).id;
            if (activeId != id) {
                activeId = id;
                Q_EMIT q_ptr->activeGroupChanged(id);
            }
            return;
        }
    }
}

int ZzSplitWorkspacePrivate::nodeDepth(const ZzNode *node) noexcept
{
    int depth = 0;
    for (const ZzNode *current = node;
         current != nullptr;
         current = current->parent) {
        ++depth;
    }
    return depth;
}

void ZzSplitWorkspacePrivate::collectLeaves(
    ZzNode *node,
    std::vector<ZzNode *> &leaves)
{
    if (node == nullptr) {
        return;
    }
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        leaves.push_back(node);
        return;
    }
    for (auto &child : std::get<ZzBranch>(node->value).children) {
        collectLeaves(child.get(), leaves);
    }
}

void ZzSplitWorkspacePrivate::collectLeaves(
    const ZzNode *node,
    std::vector<const ZzNode *> &leaves)
{
    if (node == nullptr) {
        return;
    }
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        leaves.push_back(node);
        return;
    }
    for (const auto &child : std::get<ZzBranch>(node->value).children) {
        collectLeaves(child.get(), leaves);
    }
}

std::unique_ptr<ZzNode> ZzSplitWorkspacePrivate::normalize(
    std::unique_ptr<ZzNode> node,
    ZzNode *parent)
{
    node->parent = parent;
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        return node;
    }

    auto &branch = std::get<ZzBranch>(node->value);
    for (auto &child : branch.children) {
        child = normalize(std::move(child), node.get());
    }

    std::vector<std::unique_ptr<ZzNode>> flattened;
    flattened.reserve(branch.children.size());
    for (auto &child : branch.children) {
        if (std::holds_alternative<ZzBranch>(child->value)
            && std::get<ZzBranch>(child->value).orientation
                == branch.orientation) {
            auto &grandchildren =
                std::get<ZzBranch>(child->value).children;
            for (auto &grandchild : grandchildren) {
                grandchild->parent = node.get();
                flattened.push_back(std::move(grandchild));
            }
        } else {
            child->parent = node.get();
            flattened.push_back(std::move(child));
        }
    }
    branch.children = std::move(flattened);

    if (branch.children.size() == 1) {
        auto onlyChild = std::move(branch.children.front());
        onlyChild->parent = parent;
        return onlyChild;
    }
    return node;
}

void ZzSplitWorkspacePrivate::rebuildView()
{
    std::vector<ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    for (ZzNode *leafNode : leaves) {
        auto &leaf = std::get<ZzLeaf>(leafNode->value);
        if (leaf.tabs.isNull()) {
            leaf.tabs = new ZzTabWidget(rootHost);
            prepareTabs(leaf.tabs);
        }
        auto *tabs = leaf.tabs.data();
        tabs->setParent(rootHost);
    }

    if (rootLayout->count() > 0) {
        QWidget *const previousRoot = rootLayout->itemAt(0)->widget();
        rootLayout->removeWidget(previousRoot);
        if (qobject_cast<QSplitter *>(previousRoot) != nullptr) {
            QPointer<ZzSplitWorkspace> guardedWorkspace = q_ptr;
            QPointer<QWidget> guardedPreviousRoot = previousRoot;
            previousRoot->setParent(nullptr);
            if (guardedWorkspace.isNull()) {
                delete guardedPreviousRoot.data();
                return;
            }
            delete previousRoot;
            if (guardedWorkspace.isNull()) {
                return;
            }
        }
    }
    clearSplitterPointers(root.get());
    QWidget *const rootWidget = buildNodeWidget(root.get(), rootHost);
    rootLayout->addWidget(rootWidget);
    rootWidget->show();
}

QWidget *ZzSplitWorkspacePrivate::buildNodeWidget(
    ZzNode *node,
    QWidget *parent)
{
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        auto &leaf = std::get<ZzLeaf>(node->value);
        if (leaf.tabs.isNull()) {
            leaf.tabs = new ZzTabWidget(rootHost);
            prepareTabs(leaf.tabs);
        }
        auto *tabs = leaf.tabs.data();
        tabs->setParent(parent);
        return tabs;
    }

    auto &branch = std::get<ZzBranch>(node->value);
    branch.splitter = new QSplitter(branch.orientation, parent);
    branch.splitter->setChildrenCollapsible(false);
    for (auto &child : branch.children) {
        branch.splitter->addWidget(
            buildNodeWidget(child.get(), branch.splitter));
    }
    return branch.splitter;
}

void ZzSplitWorkspacePrivate::clearSplitterPointers(ZzNode *node)
{
    if (node == nullptr || std::holds_alternative<ZzLeaf>(node->value)) {
        return;
    }
    auto &branch = std::get<ZzBranch>(node->value);
    branch.splitter = nullptr;
    for (auto &child : branch.children) {
        clearSplitterPointers(child.get());
    }
}

ZzTreeNodeSnapshot ZzSplitWorkspacePrivate::captureNodeSnapshot(
    const ZzNode *node)
{
    Q_ASSERT(node != nullptr);
    ZzTreeNodeSnapshot snapshot;
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        const auto &leaf = std::get<ZzLeaf>(node->value);
        snapshot.id = leaf.id;
        snapshot.tabs = leaf.tabs;
        return snapshot;
    }

    snapshot.leaf = false;
    const auto &branch = std::get<ZzBranch>(node->value);
    snapshot.orientation = branch.orientation;
    if (branch.splitter != nullptr) {
        snapshot.sizes = branch.splitter->sizes();
    }
    snapshot.children.reserve(branch.children.size());
    for (const auto &child : branch.children) {
        snapshot.children.push_back(captureNodeSnapshot(child.get()));
    }
    return snapshot;
}

std::unique_ptr<ZzNode> ZzSplitWorkspacePrivate::buildSnapshotNode(
    const ZzTreeNodeSnapshot &snapshot,
    ZzNode *parent)
{
    if (snapshot.leaf) {
        QPointer<ZzTabWidget> tabs = snapshot.tabs;
        if (tabs.isNull()) {
            tabs = new ZzTabWidget(rootHost);
        }
        prepareTabs(tabs);
        auto node = std::make_unique<ZzNode>(
            ZzLeaf {snapshot.id, tabs});
        node->parent = parent;
        return node;
    }

    ZzBranch branch;
    branch.orientation = snapshot.orientation;
    auto node = std::make_unique<ZzNode>(std::move(branch));
    node->parent = parent;
    auto &children = std::get<ZzBranch>(node->value).children;
    children.reserve(snapshot.children.size());
    for (const auto &childSnapshot : snapshot.children) {
        children.push_back(buildSnapshotNode(childSnapshot, node.get()));
    }
    return node;
}

std::unique_ptr<ZzNode> ZzSplitWorkspacePrivate::buildLayoutNode(
    const ZzWorkspaceLayoutNode &layout,
    QWidget *pageParent,
    ZzNode *parent)
{
    if (pageParent == nullptr) {
        return nullptr;
    }
    if (layout.leaf) {
        auto *tabs = new ZzTabWidget(pageParent);
        prepareTabs(tabs);
        auto node = std::make_unique<ZzNode>(
            ZzLeaf {layout.id, tabs});
        node->parent = parent;
        return node;
    }

    ZzBranch branch;
    branch.orientation = layout.orientation;
    auto node = std::make_unique<ZzNode>(std::move(branch));
    node->parent = parent;
    auto &children = std::get<ZzBranch>(node->value).children;
    children.reserve(layout.children.size());
    for (const auto &childLayout : layout.children) {
        auto child = buildLayoutNode(
            childLayout, pageParent, node.get());
        if (child == nullptr) {
            return nullptr;
        }
        children.push_back(std::move(child));
    }
    return node;
}

void ZzSplitWorkspacePrivate::restoreNodeSizes(
    const ZzTreeNodeSnapshot &snapshot,
    ZzNode *node)
{
    if (snapshot.leaf || node == nullptr
        || std::holds_alternative<ZzLeaf>(node->value)) {
        return;
    }
    auto &branch = std::get<ZzBranch>(node->value);
    if (branch.splitter != nullptr
        && snapshot.sizes.size()
            == static_cast<qsizetype>(branch.children.size())) {
        branch.splitter->setSizes(snapshot.sizes);
    }
    const auto childCount = std::min(
        snapshot.children.size(), branch.children.size());
    for (std::size_t index = 0; index < childCount; ++index) {
        restoreNodeSizes(
            snapshot.children[index], branch.children[index].get());
    }
}

void ZzSplitWorkspacePrivate::prepareTabs(ZzTabWidget *tabs)
{
    if (tabs == nullptr) {
        return;
    }
    tabs->installEventFilter(q_ptr);
    tabs->fluentTabBar()->installEventFilter(q_ptr);
}

bool ZzSplitWorkspacePrivate::ensureDragToken(const QMimeData *mimeData)
{
    const QString workspaceFormat =
        QString::fromLatin1(zzWorkspaceTabMimeType);
    if (mimeData != nullptr && mimeData->hasFormat(workspaceFormat)) {
        return dragRecord(mimeData).has_value();
    }
    const auto *tabPayload = dynamic_cast<const ZzTabMimeData *>(mimeData);
    if (tabPayload == nullptr || tabPayload->source.isNull()
        || tabPayload->page.isNull()) {
        return false;
    }
    ZzNode *const sourceNode = findLeaf(tabPayload->source);
    if (sourceNode == nullptr
        || tabPayload->sourceIndex < 0
        || tabPayload->sourceIndex >= tabPayload->source->count()
        || tabPayload->source->widget(tabPayload->sourceIndex)
            != tabPayload->page) {
        return false;
    }

    const ZzTabGroupId sourceId =
        std::get<ZzLeaf>(sourceNode->value).id;
    const QString token =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream << quint32(1) << token << sourceId.value()
           << qint32(tabPayload->sourceIndex);
    dragTokens.clear();
    dragTokens.insert(
        token,
        ZzWorkspaceDragRecord {
            sourceId,
            tabPayload->sourceIndex,
            tabPayload->page,
            std::chrono::steady_clock::now()
                + zzWorkspaceDragTokenLifetime});
    const_cast<QMimeData *>(mimeData)->setData(
        QString::fromLatin1(zzWorkspaceTabMimeType), encoded);
    return true;
}

std::optional<ZzWorkspaceDragRecord> ZzSplitWorkspacePrivate::dragRecord(
    const QMimeData *mimeData)
{
    const QString format = QString::fromLatin1(zzWorkspaceTabMimeType);
    if (mimeData == nullptr || !mimeData->hasFormat(format)) {
        return std::nullopt;
    }
    const QByteArray encoded = mimeData->data(format);
    if (encoded.isEmpty() || encoded.size() > 4096) {
        return std::nullopt;
    }
    QDataStream stream(encoded);
    quint32 version = 0;
    QString token;
    QString sourceValue;
    qint32 sourceIndex = -1;
    stream >> version >> token >> sourceValue >> sourceIndex;
    if (stream.status() != QDataStream::Ok || !stream.atEnd()
        || version != 1) {
        return std::nullopt;
    }
    const auto found = dragTokens.constFind(token);
    if (found == dragTokens.cend()) {
        return std::nullopt;
    }
    if (found->sourceId.value() != sourceValue
        || found->sourceIndex != static_cast<int>(sourceIndex)
        || found->page.isNull()) {
        dragTokens.clear();
        hideDropOverlay();
        return std::nullopt;
    }
    if (std::chrono::steady_clock::now() > found->deadline) {
        dragTokens.clear();
        hideDropOverlay();
        return std::nullopt;
    }
    ZzNode *const sourceNode = findLeaf(found->sourceId);
    if (sourceNode == nullptr) {
        dragTokens.clear();
        hideDropOverlay();
        return std::nullopt;
    }
    const QPointer<ZzTabWidget> tabs =
        std::get<ZzLeaf>(sourceNode->value).tabs;
    if (tabs.isNull() || found->sourceIndex < 0
        || found->sourceIndex >= tabs->count()
        || tabs->widget(found->sourceIndex) != found->page) {
        dragTokens.clear();
        hideDropOverlay();
        return std::nullopt;
    }
    auto record = found.value();
    record.deadline = std::chrono::steady_clock::now()
        + zzWorkspaceDragTokenLifetime;
    dragTokens[token] = record;
    return record;
}

ZzTabGroupId ZzSplitWorkspacePrivate::groupAt(
    const QPoint &position) const
{
    std::vector<const ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    for (const ZzNode *leafNode : leaves) {
        const auto &leaf = std::get<ZzLeaf>(leafNode->value);
        if (leaf.tabs.isNull()) {
            continue;
        }
        const QRect geometry(
            leaf.tabs->mapTo(q_ptr, QPoint(0, 0)), leaf.tabs->size());
        if (geometry.contains(position)) {
            return leaf.id;
        }
    }
    return {};
}

ZzWorkspaceDropZone ZzSplitWorkspacePrivate::dropZoneAt(
    const ZzTabGroupId &target,
    const QPoint &position) const
{
    const QRect targetRect = dropZoneRect(
        target, ZzWorkspaceDropZone::Center)
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Left))
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Top))
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Right))
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Bottom));
    const ZzWidgetTheme theme(q_ptr);
    const int extent = std::max(
        1,
        qCeil(theme.snapshot()->metric(
            ZzMetricToken::WorkspaceDropTargetExtent)));
    const int horizontalExtent = std::min(
        extent, std::max(1, (targetRect.width() - 1) / 2));
    const int verticalExtent = std::min(
        extent, std::max(1, (targetRect.height() - 1) / 2));
    if (position.x() < targetRect.left() + horizontalExtent) {
        return ZzWorkspaceDropZone::Left;
    }
    if (position.x() > targetRect.right() - horizontalExtent) {
        return ZzWorkspaceDropZone::Right;
    }
    if (position.y() < targetRect.top() + verticalExtent) {
        return ZzWorkspaceDropZone::Top;
    }
    if (position.y() > targetRect.bottom() - verticalExtent) {
        return ZzWorkspaceDropZone::Bottom;
    }
    return ZzWorkspaceDropZone::Center;
}

QRect ZzSplitWorkspacePrivate::dropZoneRect(
    const ZzTabGroupId &target,
    ZzWorkspaceDropZone zone) const
{
    ZzNode *const targetNode = findLeaf(target);
    if (targetNode == nullptr) {
        return {};
    }
    const QPointer<ZzTabWidget> tabs =
        std::get<ZzLeaf>(targetNode->value).tabs;
    if (tabs.isNull()) {
        return {};
    }
    const QRect targetRect(tabs->mapTo(q_ptr, QPoint(0, 0)), tabs->size());
    const ZzWidgetTheme theme(q_ptr);
    const int extent = std::max(
        1,
        qCeil(theme.snapshot()->metric(
            ZzMetricToken::WorkspaceDropTargetExtent)));
    const int horizontalExtent = std::min(
        extent, std::max(1, (targetRect.width() - 1) / 2));
    const int verticalExtent = std::min(
        extent, std::max(1, (targetRect.height() - 1) / 2));
    switch (zone) {
    case ZzWorkspaceDropZone::Left:
        return QRect(
            targetRect.left(), targetRect.top(),
            horizontalExtent, targetRect.height());
    case ZzWorkspaceDropZone::Right:
        return QRect(
            targetRect.right() - horizontalExtent + 1,
            targetRect.top(), horizontalExtent, targetRect.height());
    case ZzWorkspaceDropZone::Top:
        return QRect(
            targetRect.left() + horizontalExtent,
            targetRect.top(),
            targetRect.width() - horizontalExtent * 2,
            verticalExtent);
    case ZzWorkspaceDropZone::Bottom:
        return QRect(
            targetRect.left() + horizontalExtent,
            targetRect.bottom() - verticalExtent + 1,
            targetRect.width() - horizontalExtent * 2,
            verticalExtent);
    case ZzWorkspaceDropZone::Center:
        return targetRect.adjusted(
            horizontalExtent,
            verticalExtent,
            -horizontalExtent,
            -verticalExtent);
    default:
        return {};
    }
}

void ZzSplitWorkspacePrivate::showDropOverlay(const QRect &geometry)
{
    if (geometry.isEmpty()) {
        hideDropOverlay();
        return;
    }
    QPointer<QWidget> overlay = dropOverlay;
    if (overlay.isNull()) {
        overlay = new ZzWorkspaceDropOverlay(q_ptr);
        dropOverlay = overlay;
    }
    overlay->setGeometry(geometry);
    if (overlay.isNull()) {
        return;
    }
    overlay->show();
    if (overlay.isNull()) {
        return;
    }
    overlay->raise();
}

void ZzSplitWorkspacePrivate::hideDropOverlay()
{
    if (!dropOverlay.isNull()) {
        dropOverlay->hide();
    }
}

void ZzSplitWorkspacePrivate::discardDragTokens()
{
    dragTokens.clear();
}

ZzTabGroupId ZzSplitWorkspacePrivate::createGroupId()
{
    return ZzTabGroupId(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
}

} // namespace ZzFluentUI
