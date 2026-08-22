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
            || current > 1
            || (current == 1
                && groupsWithCurrentPage.contains(groupId.value()))) {
            return std::nullopt;
        }
        keys.insert(key);
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

[[nodiscard]] bool zzRestoreWorkspacePageMetadata(
    ZzTabWidget *tabs,
    const ZzWorkspaceLivePage &snapshot)
{
    QPointer<ZzTabWidget> guardedTabs = tabs;
    const QPointer<QWidget> page = snapshot.page;
    const auto resolveIndex = [&]() {
        return !guardedTabs.isNull() && !page.isNull()
            ? guardedTabs->indexOf(page)
            : -1;
    };
    int index = resolveIndex();
    if (index < 0) {
        return false;
    }

    guardedTabs->setTabText(index, snapshot.text);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabIcon(index, snapshot.icon);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabToolTip(index, snapshot.toolTip);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabWhatsThis(index, snapshot.whatsThis);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabEnabled(index, snapshot.enabled);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->fluentTabBar()->setTabData(index, snapshot.data);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->fluentTabBar()->setTabTextColor(
        index, snapshot.textColor);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabPinned(index, snapshot.pinned);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabModified(index, snapshot.modified);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabAttention(index, snapshot.attention);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTabs->setTabCloseEnabled(index, snapshot.closeEnabled);
    return resolveIndex() >= 0;
}

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
            [](const ZzWorkspacePageKey &entry) {
                return entry.page.isNull();
            }),
        pageKeys.end());

    ZzNode *ownerNode = nullptr;
    std::vector<ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    for (ZzNode *leafNode : leaves) {
        const auto &leaf = std::get<ZzLeaf>(leafNode->value);
        if (!leaf.tabs.isNull() && leaf.tabs->indexOf(page) >= 0) {
            ownerNode = leafNode;
            break;
        }
    }
    if (page == nullptr || ownerNode == nullptr) {
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
    return true;
}

QString ZzSplitWorkspacePrivate::pageLayoutKey(
    const QWidget *page) const
{
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
        }
    }
    if (pages.size()
        > static_cast<std::size_t>(zzWorkspaceMaximumSavedPageCount)) {
        return {};
    }
    std::stable_sort(
        pages.begin(),
        pages.end(),
        [&groupOrder](
            const ZzWorkspaceLayoutPage &left,
            const ZzWorkspaceLayoutPage &right) {
            return std::tuple(
                       groupOrder.value(left.groupId),
                       left.order,
                       left.key)
                < std::tuple(
                       groupOrder.value(right.groupId),
                       right.order,
                       right.key);
        });
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

bool ZzSplitWorkspacePrivate::restoreLayout(const QByteArray &encoded)
{
    const auto decoded = zzDecodeWorkspaceLayout(encoded);
    if (!decoded.has_value()) {
        return false;
    }
    const ZzWorkspaceLayoutState state = decoded.value();
    QPointer<ZzSplitWorkspace> guardedWorkspace = q_ptr;
    const ZzTreeSnapshot treeSnapshot = captureTreeSnapshot();
    const QList<ZzTabGroupId> originalIds = groupIds();
    const auto originalSavedPages = savedPages;

    std::vector<ZzWorkspaceLiveGroup> originalGroups;
    originalGroups.reserve(static_cast<std::size_t>(originalIds.size()));
    for (const ZzTabGroupId &id : originalIds) {
        ZzNode *const leafNode = findLeaf(id);
        QPointer<ZzTabWidget> tabs = leafNode != nullptr
            ? std::get<ZzLeaf>(leafNode->value).tabs
            : QPointer<ZzTabWidget> {};
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
                {tabs->widget(index),
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
                 tabs->isTabCloseEnabled(index)});
        }
        originalGroups.push_back(std::move(group));
    }

    auto *stagingHostObject = new QWidget(q_ptr);
    QPointer<QWidget> stagingHost = stagingHostObject;
    QObject::connect(
        q_ptr,
        &QObject::destroyed,
        stagingHostObject,
        [stagingHost]() {
            delete stagingHost.data();
        });
    const auto discardStaging = [&stagingHost]() {
        if (!stagingHost.isNull()) {
            stagingHost->setParent(nullptr);
            stagingHost->deleteLater();
        }
    };
    stagingHostObject->hide();
    auto *stagingLayout = new QVBoxLayout(stagingHostObject);
    stagingLayout->setContentsMargins(0, 0, 0, 0);
    stagingLayout->setSpacing(0);
    auto stagedRoot = buildLayoutNode(
        state.root, stagingHostObject, nullptr);
    if (stagedRoot == nullptr) {
        discardStaging();
        return false;
    }
    stagedRoot = normalize(std::move(stagedRoot), nullptr);
    QPointer<QWidget> stagedRootWidget = buildNodeWidget(
        stagedRoot.get(), stagingHostObject);
    stagingLayout->addWidget(stagedRootWidget);
    zzRestoreWorkspaceLayoutSizes(state.root, stagedRoot.get());

    QHash<ZzTabGroupId, QPointer<ZzTabWidget>> stagedTabs;
    QList<ZzTabGroupId> restoredIds;
    std::vector<QPointer<ZzTabWidget>> stagedTabList;
    std::vector<ZzNode *> stagedLeaves;
    collectLeaves(stagedRoot.get(), stagedLeaves);
    for (ZzNode *leafNode : stagedLeaves) {
        const auto &leaf = std::get<ZzLeaf>(leafNode->value);
        if (leaf.tabs.isNull()) {
            discardStaging();
            return false;
        }
        stagedTabs.insert(leaf.id, leaf.tabs);
        restoredIds.push_back(leaf.id);
        stagedTabList.push_back(leaf.tabs);
    }

    QHash<QString, ZzWorkspaceLayoutPage> savedByKey;
    QSet<QString> restoredGroupValues;
    for (const ZzTabGroupId &id : stagedTabs.keys()) {
        restoredGroupValues.insert(id.value());
    }
    for (const auto &page : state.pages) {
        savedByKey.insert(page.key, page);
    }

    QHash<ZzTabGroupId, std::vector<ZzWorkspaceDesiredPage>> desiredPages;
    QHash<QWidget *, ZzTabGroupId> desiredTargets;
    QHash<QString, QPointer<QWidget>> livePagesByKey;
    int sequence = 0;
    for (const auto &group : originalGroups) {
        for (std::size_t index = 0; index < group.pages.size(); ++index) {
            const QPointer<QWidget> page = group.pages[index].page;
            if (page.isNull()) {
                discardStaging();
                return false;
            }
            const QString key = pageLayoutKey(page);
            ZzWorkspaceDesiredPage desired;
            desired.page = page;
            desired.originalTabs = group.tabs;
            desired.sequence = sequence++;
            const auto saved = savedByKey.constFind(key);
            if (!key.isEmpty() && saved != savedByKey.cend()) {
                desired.targetId = saved->groupId;
                desired.desiredOrder = saved->order;
                desired.savedOrder = true;
                livePagesByKey.insert(key, page);
            } else {
                desired.targetId = restoredGroupValues.contains(
                    group.id.value())
                    ? group.id
                    : state.activeId;
                desired.desiredOrder = static_cast<int>(index);
            }
            desiredTargets.insert(page, desired.targetId);
            desiredPages[desired.targetId].push_back(std::move(desired));
        }
    }
    for (auto pageIt = desiredPages.begin();
         pageIt != desiredPages.end();
         ++pageIt) {
        std::stable_sort(
            pageIt->begin(),
            pageIt->end(),
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

    const auto rollback = [&]() {
        if (guardedWorkspace.isNull()) {
            return false;
        }
        for (const auto &group : originalGroups) {
            ZzNode *const originalNode = findLeaf(group.id);
            const QPointer<ZzTabWidget> originalTabs =
                originalNode != nullptr
                ? std::get<ZzLeaf>(originalNode->value).tabs
                : QPointer<ZzTabWidget> {};
            if (originalTabs.isNull()
                || originalTabs.data() != group.identity) {
                continue;
            }
            for (std::size_t index = 0;
                 index < group.pages.size();
                 ++index) {
                const auto &snapshot = group.pages[index];
                const QPointer<QWidget> page = snapshot.page;
                if (page.isNull()) {
                    continue;
                }
                QPointer<ZzTabWidget> owner = zzOwningWorkspaceTabs(page);
                const bool parentedByStaging = std::any_of(
                    stagedTabList.cbegin(),
                    stagedTabList.cend(),
                    [owner](const QPointer<ZzTabWidget> &tabs) {
                        return !tabs.isNull() && tabs == owner;
                    });
                if (!parentedByStaging || owner.isNull()) {
                    continue;
                }
                int currentIndex = owner->indexOf(page);
                bool rebuiltStagingTab = false;
                if (currentIndex < 0) {
                    const QPointer<ZzTabWidget> stagingOwner = owner;
                    owner->insertTab(
                        owner->count(), page, snapshot.icon, snapshot.text);
                    if (guardedWorkspace.isNull()) {
                        return false;
                    }
                    if (stagingOwner.isNull() || page.isNull()) {
                        continue;
                    }
                    owner = stagingOwner;
                    currentIndex = owner->indexOf(page);
                    if (currentIndex < 0) {
                        continue;
                    }
                    rebuiltStagingTab = true;
                }
                if (currentIndex >= 0) {
                    owner->transferTabTo(
                        originalTabs,
                        currentIndex,
                        static_cast<int>(index));
                    if (guardedWorkspace.isNull()) {
                        return false;
                    }
                    if (rebuiltStagingTab && !page.isNull()
                        && !originalTabs.isNull()
                        && originalTabs->indexOf(page) >= 0) {
                        const bool metadataRestored =
                            zzRestoreWorkspacePageMetadata(
                            originalTabs, snapshot);
                        if (guardedWorkspace.isNull()) {
                            return false;
                        }
                        if (!metadataRestored) {
                            continue;
                        }
                    }
                }
            }
        }
        for (const auto &group : originalGroups) {
            ZzNode *const originalNode = findLeaf(group.id);
            QPointer<ZzTabWidget> tabs = originalNode != nullptr
                ? std::get<ZzLeaf>(originalNode->value).tabs
                : QPointer<ZzTabWidget> {};
            if (tabs.isNull() || tabs.data() != group.identity) {
                continue;
            }
            for (std::size_t index = 0;
                 index < group.pages.size();
                 ++index) {
                const QPointer<QWidget> page = group.pages[index].page;
                if (page.isNull()) {
                    continue;
                }
                const int currentIndex = tabs->indexOf(page);
                const int wantedIndex = static_cast<int>(index);
                if (currentIndex >= 0 && currentIndex != wantedIndex) {
                    const int insertionSlot = currentIndex < wantedIndex
                        ? wantedIndex + 1
                        : wantedIndex;
                    tabs->transferTabTo(
                        tabs, currentIndex, insertionSlot);
                    if (guardedWorkspace.isNull()) {
                        return false;
                    }
                }
            }
            if (!group.currentPage.isNull()
                && tabs->indexOf(group.currentPage) >= 0) {
                tabs->setCurrentWidget(group.currentPage);
                if (guardedWorkspace.isNull()) {
                    return false;
                }
            }
        }

        bool originalIdentitiesPresent = true;
        for (const auto &group : originalGroups) {
            ZzNode *const node = findLeaf(group.id);
            const QPointer<ZzTabWidget> tabs = node != nullptr
                ? std::get<ZzLeaf>(node->value).tabs
                : QPointer<ZzTabWidget> {};
            if (tabs.isNull() || tabs.data() != group.identity) {
                originalIdentitiesPresent = false;
                break;
            }
        }
        if (originalIdentitiesPresent && groupIds() == originalIds) {
            activeId = treeSnapshot.activeId;
            restoreNodeSizes(treeSnapshot.root, root.get());
        }
        savedPages = originalSavedPages;
        discardStaging();
        return false;
    };

    for (const ZzTabGroupId &targetId : restoredIds) {
        QPointer<ZzTabWidget> target = stagedTabs.value(targetId);
        auto &pages = desiredPages[targetId];
        if (target.isNull()) {
            return rollback();
        }
        for (const auto &desired : pages) {
            if (desired.page.isNull() || desired.originalTabs.isNull()
                || desired.originalTabs->indexOf(desired.page) < 0) {
                return rollback();
            }
            const int sourceIndex = desired.originalTabs->indexOf(
                desired.page);
            if (!desired.originalTabs->transferTabTo(
                    target, sourceIndex, -1)) {
                return rollback();
            }
            if (guardedWorkspace.isNull()) {
                return false;
            }
            target = stagedTabs.value(targetId);
            if (desired.page.isNull() || target.isNull()
                || zzOwningWorkspaceTabs(desired.page) != target) {
                return rollback();
            }
        }
        if (target->count() != static_cast<int>(pages.size())) {
            return rollback();
        }
        for (std::size_t index = 0; index < pages.size(); ++index) {
            if (pages[index].page.isNull()
                || target->widget(static_cast<int>(index))
                    != pages[index].page) {
                return rollback();
            }
        }
    }

    QHash<ZzTabGroupId, QPointer<QWidget>> desiredCurrentPages;
    for (const auto &saved : state.pages) {
        if (!saved.current) {
            continue;
        }
        const QPointer<QWidget> page = livePagesByKey.value(saved.key);
        if (!page.isNull()
            && desiredTargets.value(page) == saved.groupId) {
            desiredCurrentPages.insert(saved.groupId, page);
        }
    }
    for (const auto &group : originalGroups) {
        if (!group.currentPage.isNull()) {
            const ZzTabGroupId targetId = desiredTargets.value(
                group.currentPage);
            if (targetId.isValid()
                && !desiredCurrentPages.contains(targetId)) {
                desiredCurrentPages.insert(targetId, group.currentPage);
            }
        }
    }
    for (auto currentIt = desiredCurrentPages.cbegin();
         currentIt != desiredCurrentPages.cend();
         ++currentIt) {
        QPointer<ZzTabWidget> target = stagedTabs.value(currentIt.key());
        const QPointer<QWidget> page = currentIt.value();
        if (target.isNull() || page.isNull()
            || target->indexOf(page) < 0) {
            return rollback();
        }
        target->setCurrentWidget(page);
        if (guardedWorkspace.isNull()) {
            return false;
        }
        target = stagedTabs.value(currentIt.key());
        if (target.isNull() || page.isNull()
            || target->currentWidget() != page) {
            return rollback();
        }
    }

    for (const ZzTabGroupId &targetId : restoredIds) {
        const QPointer<ZzTabWidget> target = stagedTabs.value(targetId);
        const auto &pages = desiredPages[targetId];
        if (target.isNull()
            || target->count() != static_cast<int>(pages.size())) {
            return rollback();
        }
        for (std::size_t index = 0; index < pages.size(); ++index) {
            if (pages[index].page.isNull()
                || target->widget(static_cast<int>(index))
                    != pages[index].page
                || zzOwningWorkspaceTabs(pages[index].page) != target) {
                return rollback();
            }
        }
    }
    if (groupIds() != originalIds) {
        return rollback();
    }
    for (const auto &group : originalGroups) {
        ZzNode *const node = findLeaf(group.id);
        const QPointer<ZzTabWidget> tabs = node != nullptr
            ? std::get<ZzLeaf>(node->value).tabs
            : QPointer<ZzTabWidget> {};
        if (tabs.isNull() || tabs.data() != group.identity) {
            return rollback();
        }
    }

    if (stagingHost.isNull() || stagedRootWidget.isNull()
        || rootLayout->count() != 1) {
        return rollback();
    }
    QPointer<QWidget> previousRootWidget =
        rootLayout->itemAt(0)->widget();
    if (previousRootWidget.isNull()) {
        return rollback();
    }
    rootLayout->removeWidget(previousRootWidget);
    previousRootWidget->setParent(stagingHost);
    if (guardedWorkspace.isNull()) {
        return false;
    }

    auto previousRoot = std::move(root);
    root = std::move(stagedRoot);
    activeId = state.activeId;
    savedPages = state.pages;
    stagingLayout->removeWidget(stagedRootWidget);
    stagedRootWidget->setParent(rootHost);
    rootLayout->addWidget(stagedRootWidget);
    stagedRootWidget->show();
    zzRestoreWorkspaceLayoutSizes(state.root, root.get());
    discardStaging();
    return true;
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
