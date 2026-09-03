#include <ZzFluentUI/ZzSplitWorkspace.h>

#include <QtCore/QPointer>
#include <QtCore/QEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QObject>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTabBar.h>

#include <QtWidgets/QStackedWidget>

#include "private/ZzSplitWorkspacePrivate.h"
#include "private/ZzTabWidgetPrivate.h"
#include "private/ZzWorkspaceCrossTransferTransactionPrivate.h"

namespace ZzFluentUI {

namespace {

struct ZzCoordinatorTabSnapshot final
{
    ZzTabTransferSnapshot tab;
    QString pageTitle;
    QString layoutKey;
};

struct ZzCoordinatorGroupSnapshot final
{
    ZzTabGroupId id;
    QPointer<ZzTabWidget> tabs;
    std::vector<ZzCoordinatorTabSnapshot> pages;
    QPointer<QWidget> current;
};

struct ZzCoordinatorPageSnapshot final
{
    QPointer<QWidget> page;
    ZzWorkspacePageId id;
};

struct ZzCoordinatorWorkspaceSnapshot final
{
    QPointer<ZzSplitWorkspace> workspace;
    QList<ZzTabGroupId> groups;
    ZzTabGroupId active;
    ZzTreeSnapshot tree;
    std::vector<ZzCoordinatorGroupSnapshot> groupStates;
    std::vector<ZzCoordinatorPageSnapshot> pageIds;
    std::vector<ZzWorkspacePageKey> pageKeys;
    std::vector<ZzWorkspaceLayoutPage> savedPages;
};

[[nodiscard]] bool zzCoordinatorIconMatches(
    const QIcon &left, const QIcon &right)
{
    if (left.isNull() || right.isNull()) return left.isNull() == right.isNull();
    if (left.availableSizes() != right.availableSizes()) return false;
    for (const QSize &size : left.availableSizes()) {
        if (left.pixmap(size).toImage() != right.pixmap(size).toImage()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool zzCoordinatorTreeMatches(
    const ZzTreeNodeSnapshot &left,
    const ZzTreeNodeSnapshot &right)
{
    if (left.leaf != right.leaf || left.id != right.id
        || left.tabs != right.tabs || left.orientation != right.orientation
        || left.sizes != right.sizes
        || left.children.size() != right.children.size()) return false;
    for (std::size_t index = 0; index < left.children.size(); ++index) {
        if (!zzCoordinatorTreeMatches(left.children[index], right.children[index])) {
            return false;
        }
    }
    return true;
}

class ZzScopedSignalBlock final
{
public:
    explicit ZzScopedSignalBlock(QObject *object)
        : object_(object)
        , previouslyBlocked_(object != nullptr && object->signalsBlocked())
    {
        if (!object_.isNull()) object_->blockSignals(true);
    }

    ~ZzScopedSignalBlock()
    {
        if (!object_.isNull()) object_->blockSignals(previouslyBlocked_);
    }

    Q_DISABLE_COPY_MOVE(ZzScopedSignalBlock)

private:
    QPointer<QObject> object_;
    bool previouslyBlocked_ = false;
};

void zzSyncCurrentPage(ZzTabWidget *tabs)
{
    if (tabs == nullptr || tabs->fluentTabBar() == nullptr) return;
    auto *const stack = tabs->findChild<QStackedWidget *>();
    if (stack == nullptr) return;
    ZzScopedSignalBlock stackSignals(stack);
    stack->setCurrentIndex(tabs->fluentTabBar()->currentIndex());
}

} // namespace

ZzSplitWorkspace::ZzSplitWorkspace(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzSplitWorkspacePrivate>(this))
{
}

ZzSplitWorkspace::~ZzSplitWorkspace() = default;

QList<ZzTabGroupId> ZzSplitWorkspace::groupIds() const
{
    return d_ptr->groupIds();
}

ZzTabGroupId ZzSplitWorkspace::activeGroupId() const
{
    return d_ptr->activeId;
}

bool ZzSplitWorkspace::setActiveGroup(const ZzTabGroupId &id)
{
    ZzNode *const node = d_ptr->findLeaf(id);
    if (node == nullptr) {
        return false;
    }

    const bool changed = d_ptr->activeId != id;
    d_ptr->activeId = id;
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    QPointer<ZzTabWidget> guardedTabs = std::get<ZzLeaf>(node->value).tabs;
    if (!guardedTabs.isNull()) {
        guardedTabs->setFocus(Qt::OtherFocusReason);
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (changed) {
        Q_EMIT activeGroupChanged(id);
    }
    return true;
}

ZzTabWidget *ZzSplitWorkspace::tabWidget(
    const ZzTabGroupId &id) const noexcept
{
    ZzNode *const node = d_ptr->findLeaf(id);
    return node != nullptr
        ? std::get<ZzLeaf>(node->value).tabs.data()
        : nullptr;
}

ZzTabGroupId ZzSplitWorkspace::groupId(const ZzTabWidget *tabs) const
{
    ZzNode *const node = d_ptr->findLeaf(tabs);
    return node != nullptr ? std::get<ZzLeaf>(node->value).id
                           : ZzTabGroupId {};
}

std::optional<ZzTabGroupId> ZzSplitWorkspace::splitGroup(
    const ZzTabGroupId &source,
    Qt::Orientation orientation,
    ZzSplitPlacement placement,
    const ZzTabGroupId &requestedId)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    const auto result = d_ptr->splitGroup(
        source, orientation, placement, requestedId);
    if (!result.has_value() || guardedWorkspace.isNull()) {
        return result;
    }

    Q_EMIT groupAdded(result.value());
    if (guardedWorkspace.isNull()) {
        return result;
    }
    Q_EMIT layoutChanged();
    return result;
}

bool ZzSplitWorkspace::removeEmptyGroup(const ZzTabGroupId &id)
{
    const bool removedActive = d_ptr->activeId == id;
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    if (!d_ptr->removeEmptyGroup(id)) {
        return false;
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }

    Q_EMIT groupAboutToBeRemoved(id);
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (removedActive) {
        Q_EMIT activeGroupChanged(d_ptr->activeId);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    Q_EMIT layoutChanged();
    return true;
}

bool ZzSplitWorkspace::focusAdjacentGroup(Qt::Edge direction)
{
    const ZzTabGroupId adjacent = d_ptr->adjacentGroup(direction);
    return adjacent.isValid() && setActiveGroup(adjacent);
}

bool ZzSplitWorkspace::transferTab(
    const ZzTabGroupId &source,
    int sourceIndex,
    const ZzTabGroupId &target,
    int targetIndex)
{
    return d_ptr->transferTab(source, sourceIndex, target, targetIndex);
}

bool ZzSplitWorkspace::setPageLayoutKey(
    QWidget *page,
    const QString &key)
{
    return d_ptr->setPageLayoutKey(page, key);
}

QString ZzSplitWorkspace::pageLayoutKey(const QWidget *page) const
{
    return d_ptr->pageLayoutKey(page);
}

ZzWorkspacePageId ZzSplitWorkspace::pageId(const QWidget *page) const
{
    return d_ptr->pageId(page);
}

QWidget *ZzSplitWorkspace::pageForId(const ZzWorkspacePageId &id) const
{
    return d_ptr->pageForId(id);
}

ZzCore::ZzResult<void> ZzSplitWorkspace::transferTabToWorkspace(
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *targetWorkspace,
    const ZzTabGroupId &targetGroup,
    int targetIndex,
    ZzWorkspaceDropZone zone)
{
    return ZzWorkspaceCrossTransferTransactionPrivate::run(
        this,
        sourceGroup,
        sourceIndex,
        targetWorkspace,
        targetGroup,
        targetIndex,
        zone);
}

ZzCore::ZzResult<void> ZzSplitWorkspace::transferTabToWorkspaceSilently(
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *targetWorkspace,
    const ZzTabGroupId &targetGroup,
    int targetIndex)
{
    auto *const sourceTabs = tabWidget(sourceGroup);
    auto *const targetTabs = targetWorkspace != nullptr
        ? targetWorkspace->tabWidget(targetGroup)
        : nullptr;
    if (sourceTabs == nullptr || targetTabs == nullptr) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("unknown workspace tab group")));
    }
    QPointer<ZzTabWidget> guardedSourceTabs(sourceTabs);
    QPointer<ZzTabWidget> guardedTargetTabs(targetTabs);
    ZzScopedSignalBlock sourceBarSignals(sourceTabs->fluentTabBar());
    ZzScopedSignalBlock targetBarSignals(targetTabs->fluentTabBar());
    const bool sourceSilent = sourceTabs->silentTransfer();
    const bool targetSilent = targetTabs->silentTransfer();
    sourceTabs->setSilentTransfer(true);
    targetTabs->setSilentTransfer(true);
    int effectiveTargetIndex = targetIndex;
    if (effectiveTargetIndex < 0 && sourceTabs->isTabPinned(sourceIndex)) {
        effectiveTargetIndex = 0;
        while (effectiveTargetIndex < targetTabs->count()
            && targetTabs->isTabPinned(effectiveTargetIndex)) {
            ++effectiveTargetIndex;
        }
    }
    const auto result = ZzWorkspaceCrossTransferTransactionPrivate::run(
        this,
        sourceGroup,
        sourceIndex,
        targetWorkspace,
        targetGroup,
        effectiveTargetIndex,
        ZzWorkspaceDropZone::Center,
        false);
    if (!guardedSourceTabs.isNull()) {
        zzSyncCurrentPage(guardedSourceTabs);
        guardedSourceTabs->setSilentTransfer(sourceSilent);
    }
    if (!guardedTargetTabs.isNull()) {
        zzSyncCurrentPage(guardedTargetTabs);
        guardedTargetTabs->setSilentTransfer(targetSilent);
    }
    return result;
}

bool ZzSplitWorkspace::restoreGroupOrderSilently(
    const ZzTabGroupId &group,
    const QList<QWidget *> &pages)
{
    QPointer<ZzTabWidget> tabs = tabWidget(group);
    if (tabs.isNull() || tabs->count() != pages.size()) {
        return false;
    }
    QPointer<ZzTabBar> bar = tabs->fluentTabBar();
    if (bar.isNull()) return false;
    ZzScopedSignalBlock barSignals(bar);
    QPointer<QWidget> currentPage = tabs->currentWidget();
    const bool previouslySilent = tabs->silentTransfer();
    tabs->setSilentTransfer(true);
    const bool previouslyBlocked = tabs->blockSignals(true);
    const auto restoreSignals = [&tabs, previouslyBlocked, previouslySilent] {
        tabs->setSilentTransfer(previouslySilent);
        if (!tabs.isNull()) tabs->blockSignals(previouslyBlocked);
    };
    for (int desired = 0; desired < pages.size(); ++desired) {
        QPointer<QWidget> page = pages.at(desired);
        if (tabs.isNull() || bar.isNull() || page.isNull()) {
            restoreSignals();
            return false;
        }
        const int actual = tabs->indexOf(page);
        if (actual < 0) {
            restoreSignals();
            return false;
        }
        if (actual != desired) {
            const auto snapshot = ZzFluentUI::ZzTabWidgetPrivate::snapshotFor(
                tabs, actual);
            tabs->d_ptr->removalNotified = true;
            tabs->QTabWidget::removeTab(actual);
            const int inserted = tabs->QTabWidget::insertTab(
                desired, page, snapshot.icon, snapshot.text);
            if (inserted < 0) {
                restoreSignals();
                return false;
            }
            if (!ZzTabWidgetPrivate::restoreMetadata(
                    tabs, inserted, snapshot)) {
                restoreSignals();
                return false;
            }
            if (tabs.isNull() || bar.isNull() || page.isNull()) {
                restoreSignals();
                return false;
            }
        }
    }
    for (int index = 0; index < pages.size(); ++index) {
        if (tabs.isNull() || tabs->widget(index) != pages.at(index)) {
            restoreSignals();
            return false;
        }
    }
    if (!currentPage.isNull() && tabs->indexOf(currentPage) >= 0) {
        bar->setCurrentIndex(tabs->indexOf(currentPage));
        zzSyncCurrentPage(tabs);
    }
    restoreSignals();
    return true;
}

void ZzSplitWorkspace::beginCoordinatorTransaction()
{
    ++d_ptr->transactionDepth;
    if (d_ptr->transactionDepth == 1) {
        for (const auto &group : groupIds()) {
            if (auto *tabs = tabWidget(group); tabs != nullptr)
                tabs->beginCoordinatorTransaction();
        }
    }
}

void ZzSplitWorkspace::endCoordinatorTransaction()
{
    if (d_ptr->transactionDepth <= 0) return;
    --d_ptr->transactionDepth;
    if (d_ptr->transactionDepth == 0) {
        for (const auto &group : groupIds()) {
            if (auto *tabs = tabWidget(group); tabs != nullptr)
                tabs->endCoordinatorTransaction();
        }
    }
}

std::shared_ptr<void> ZzSplitWorkspace::captureCoordinatorSnapshot() const
{
    auto snapshot = std::make_shared<ZzCoordinatorWorkspaceSnapshot>();
    snapshot->workspace = const_cast<ZzSplitWorkspace *>(this);
    snapshot->groups = d_ptr->groupIds();
    snapshot->active = d_ptr->activeId;
    snapshot->tree = d_ptr->captureTreeSnapshot();
    for (const auto &group : snapshot->groups) {
        auto *const tabs = tabWidget(group);
        ZzCoordinatorGroupSnapshot groupSnapshot;
        groupSnapshot.id = group;
        groupSnapshot.tabs = tabs;
        if (tabs != nullptr) {
            groupSnapshot.current = tabs->currentWidget();
            groupSnapshot.pages.reserve(static_cast<std::size_t>(tabs->count()));
            for (int index = 0; index < tabs->count(); ++index) {
                const auto tab = ZzTabWidgetPrivate::snapshotFor(tabs, index);
                groupSnapshot.pages.push_back(
                    {tab,
                     tab.page != nullptr ? tab.page->windowTitle() : QString {},
                     pageLayoutKey(tab.page)});
                static_cast<void>(const_cast<ZzSplitWorkspace *>(this)->pageId(tab.page));
            }
        }
        snapshot->groupStates.push_back(std::move(groupSnapshot));
    }
    for (auto it = d_ptr->pageIds.cbegin(); it != d_ptr->pageIds.cend(); ++it) {
        snapshot->pageIds.push_back({it.key(), it.value()});
    }
    snapshot->pageKeys = d_ptr->pageKeys;
    snapshot->savedPages = d_ptr->savedPages;
    return snapshot;
}

bool ZzSplitWorkspace::coordinatorSnapshotMatches(
    const std::shared_ptr<void> &opaque) const
{
    const auto snapshot = std::static_pointer_cast<
        const ZzCoordinatorWorkspaceSnapshot>(opaque);
    if (!snapshot || snapshot->workspace != this
        || d_ptr->groupIds() != snapshot->groups
        || d_ptr->activeId != snapshot->active
        || !zzCoordinatorTreeMatches(
            d_ptr->captureTreeSnapshot().root, snapshot->tree.root)
        || d_ptr->pageIds.size() != static_cast<qsizetype>(snapshot->pageIds.size())
        || d_ptr->pageKeys.size() != snapshot->pageKeys.size()) {
        return false;
    }
    for (const auto &entry : snapshot->pageIds) {
        if (entry.page.isNull() || d_ptr->pageIds.value(entry.page.data()) != entry.id
            || d_ptr->pagesById.value(entry.id) != entry.page) return false;
    }
    for (const auto &entry : snapshot->pageKeys) {
        const auto found = std::find_if(d_ptr->pageKeys.cbegin(), d_ptr->pageKeys.cend(),
            [&entry](const ZzWorkspacePageKey &value) {
                return value.page == entry.page && value.key == entry.key;
            });
        if (found == d_ptr->pageKeys.cend()) return false;
    }
    for (const auto &group : snapshot->groupStates) {
        auto *const tabs = tabWidget(group.id);
        if (tabs == nullptr || tabs != group.tabs
            || tabs->count() != static_cast<int>(group.pages.size())
            || tabs->currentWidget() != group.current) return false;
        for (std::size_t index = 0; index < group.pages.size(); ++index) {
            const auto &expected = group.pages[index];
            const auto actual = ZzTabWidgetPrivate::snapshotFor(
                tabs, static_cast<int>(index));
            if (actual.page != expected.tab.page
                || actual.text != expected.tab.text
                || !zzCoordinatorIconMatches(actual.icon, expected.tab.icon)
                || actual.toolTip != expected.tab.toolTip
                || actual.whatsThis != expected.tab.whatsThis
                || actual.data != expected.tab.data
                || actual.textColor != expected.tab.textColor
                || actual.enabled != expected.tab.enabled
                || actual.pinned != expected.tab.pinned
                || actual.modified != expected.tab.modified
                || actual.attention != expected.tab.attention
                || actual.closeEnabled != expected.tab.closeEnabled
                || expected.tab.page->windowTitle() != expected.pageTitle
                || pageLayoutKey(actual.page) != expected.layoutKey) {
                return false;
            }
        }
    }
    return true;
}

bool ZzSplitWorkspace::restoreCoordinatorSnapshot(
    const std::shared_ptr<void> &opaque)
{
    const auto snapshot = std::static_pointer_cast<
        const ZzCoordinatorWorkspaceSnapshot>(opaque);
    if (!snapshot || snapshot->workspace != this) return false;
    for (const auto &entry : snapshot->pageIds) {
        if (entry.page.isNull()) return false;
    }
    for (const auto &group : snapshot->groupStates) {
        if (group.tabs.isNull()) return false;
    }
    if (d_ptr->groupIds() != snapshot->groups) {
        if (!d_ptr->restoreTreeSnapshot(snapshot->tree)) return false;
    }
    const auto expectedOwner = [snapshot](QWidget *page) -> ZzTabWidget * {
        for (const auto &group : snapshot->groupStates) {
            for (const auto &state : group.pages) {
                if (state.tab.page == page) return group.tabs.data();
            }
        }
        return nullptr;
    };
    for (const auto &group : snapshot->groupStates) {
        auto *const tabs = group.tabs.data();
        for (int index = tabs->count() - 1; index >= 0; --index) {
            if (expectedOwner(tabs->widget(index)) != tabs) {
                static_cast<QTabWidget *>(tabs)->removeTab(index);
            }
        }
    }
    for (const auto &group : snapshot->groupStates) {
        auto *const tabs = group.tabs.data();
        for (std::size_t wanted = 0; wanted < group.pages.size(); ++wanted) {
            const auto &expected = group.pages[wanted];
            auto *owner = expectedOwner(expected.tab.page);
            if (owner == nullptr || expected.tab.page.isNull()) return false;
            const int ownerIndex = owner->indexOf(expected.tab.page);
            if (owner != tabs && ownerIndex >= 0) {
                static_cast<QTabWidget *>(owner)->removeTab(ownerIndex);
            }
            int actual = tabs->indexOf(expected.tab.page);
            if (actual < 0) {
                actual = static_cast<QTabWidget *>(tabs)->insertTab(
                    static_cast<int>(wanted), expected.tab.page,
                    expected.tab.icon, expected.tab.text);
            } else if (actual != static_cast<int>(wanted)) {
                static_cast<QTabWidget *>(tabs)->removeTab(actual);
                actual = static_cast<QTabWidget *>(tabs)->insertTab(
                    static_cast<int>(wanted), expected.tab.page,
                    expected.tab.icon, expected.tab.text);
            }
            if (actual < 0 || !ZzTabWidgetPrivate::restoreMetadata(
                    tabs, actual, expected.tab)) return false;
            expected.tab.page->setWindowTitle(expected.pageTitle);
        }
        while (tabs->count() > static_cast<int>(group.pages.size())) {
            static_cast<QTabWidget *>(tabs)->removeTab(tabs->count() - 1);
        }
        tabs->setCurrentWidget(group.current);
    }
    for (auto connection : d_ptr->pageDestroyedConnections) {
        QObject::disconnect(connection);
    }
    d_ptr->pageDestroyedConnections.clear();
    d_ptr->pageIds.clear();
    d_ptr->pagesById.clear();
    d_ptr->pageKeys = snapshot->pageKeys;
    d_ptr->savedPages = snapshot->savedPages;
    for (const auto &entry : snapshot->pageIds) {
        d_ptr->pageIds.insert(entry.page.data(), entry.id);
        d_ptr->pagesById.insert(entry.id, entry.page);
        d_ptr->pageDestroyedConnections.insert(
            entry.page.data(),
            QObject::connect(entry.page.data(), &QObject::destroyed, this,
                [this](QObject *object) {
                    auto *const page = static_cast<QWidget *>(object);
                    const auto it = d_ptr->pageIds.find(page);
                    if (it != d_ptr->pageIds.end()) {
                        d_ptr->pagesById.remove(it.value());
                        d_ptr->pageIds.erase(it);
                    }
                    d_ptr->pageDestroyedConnections.remove(page);
                }));
    }
    d_ptr->activeId = snapshot->active;
    for (const auto &group : snapshot->groupStates) zzSyncCurrentPage(group.tabs);
    return coordinatorSnapshotMatches(opaque);
}

QByteArray ZzSplitWorkspace::saveLayout() const
{
    return d_ptr->saveLayout();
}

bool ZzSplitWorkspace::restoreLayout(const QByteArray &state)
{
    const ZzTabGroupId previousActive = d_ptr->activeId;
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    if (!d_ptr->restoreLayout(state)) {
        return false;
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (previousActive != d_ptr->activeId) {
        Q_EMIT activeGroupChanged(d_ptr->activeId);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    Q_EMIT layoutChanged();
    return true;
}

ZzTabGroupId ZzSplitWorkspace::savedGroupForPageKey(
    const QString &key) const
{
    return d_ptr->savedGroupForPageKey(key);
}

bool ZzSplitWorkspace::moveTabToDropZone(
    const ZzTabGroupId &source,
    int sourceIndex,
    const ZzTabGroupId &target,
    ZzWorkspaceDropZone zone)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    const ZzWorkspaceTransferResult result = d_ptr->moveTabToDropZone(
        source, sourceIndex, target, zone);
    if (!result.committed || guardedWorkspace.isNull()) {
        return result.committed;
    }
    if (result.groupAdded) {
        Q_EMIT groupAdded(result.destinationId);
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (result.sourceRemoved) {
        Q_EMIT groupAboutToBeRemoved(result.sourceId);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    if (result.layoutChanged) {
        Q_EMIT layoutChanged();
    }
    if (result.activeChanged) {
        Q_EMIT activeGroupChanged(result.destinationId);
        if (guardedWorkspace.isNull()) return true;
    }
    Q_EMIT tabDropCommitted(
        result.sourceId,
        result.destinationId,
        result.zone,
        result.page.data());
    if (guardedWorkspace.isNull()) {
        return true;
    }
    return true;
}

bool ZzSplitWorkspace::eventFilter(QObject *watched, QEvent *event)
{
    if (d_ptr->eventFilter(watched, event)) {
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ZzSplitWorkspace::dragEnterEvent(QDragEnterEvent *event)
{
    if (!d_ptr->handleDragEnter(this, event)) {
        QWidget::dragEnterEvent(event);
    }
}

void ZzSplitWorkspace::dragMoveEvent(QDragMoveEvent *event)
{
    if (!d_ptr->handleDragMove(this, event)) {
        QWidget::dragMoveEvent(event);
    }
}

void ZzSplitWorkspace::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (!d_ptr->handleDragLeave(event)) {
        QWidget::dragLeaveEvent(event);
    }
}

void ZzSplitWorkspace::dropEvent(QDropEvent *event)
{
    if (!d_ptr->handleDrop(this, event)) {
        QWidget::dropEvent(event);
    }
}

} // namespace ZzFluentUI
