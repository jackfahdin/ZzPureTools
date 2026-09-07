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
#include "private/ZzWorkspacePageIdentityPrivate.h"
#include "private/ZzWorkspaceCrossTransferTransactionPrivate.h"

namespace ZzFluentUI {

QList<ZzTabGroupId> ZzSplitWorkspace::attentionGroupIds() const
{
    return d_ptr->attentionGroupIds();
}

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
    int tabBarCount = 0;
    std::vector<ZzTabTransferSnapshot> tabBarEntries;
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
    qRegisterMetaType<ZzTabGroupId>();
    qRegisterMetaType<ZzWorkspacePageId>();
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

void ZzSplitWorkspace::setEmptyGroupPolicy(ZzEmptyGroupPolicy policy) noexcept
{
    d_ptr->emptyGroupPolicy = policy;
}

ZzEmptyGroupPolicy ZzSplitWorkspace::emptyGroupPolicy() const noexcept
{
    return d_ptr->emptyGroupPolicy;
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
        if (guardedWorkspace.isNull()) {
            return true;
        }
        d_ptr->publishActivePage();
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
        d_ptr->publishActivePage();
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
    if (d_ptr->publicTransferBlockDepth != 0) return false;
    QPointer<ZzSplitWorkspace> guardedWorkspace(this);
    ++d_ptr->transactionDepth;
    const bool transferred = d_ptr->transferTab(
        source, sourceIndex, target, targetIndex);
    if (guardedWorkspace.isNull()) return transferred;
    --d_ptr->transactionDepth;
    d_ptr->processPendingEmptyGroups();
    if (guardedWorkspace.isNull()) return transferred;
    if (d_ptr->activePagePublishPending) {
        d_ptr->activePagePublishPending = false;
        d_ptr->publishActivePage();
    }
    return transferred;
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
    auto result = ZzWorkspaceCrossTransferTransactionPrivate::run(
        this,
        sourceGroup,
        sourceIndex,
        targetWorkspace,
        targetGroup,
        targetIndex,
        zone);
    if (result.hasValue()) {
        d_ptr->refreshAttentionGroups();
        if (targetWorkspace != nullptr && targetWorkspace != this) {
            targetWorkspace->d_ptr->refreshAttentionGroups();
        }
    }
    return result;
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

void ZzSplitWorkspace::beginInternalTransaction(bool blockPublicTransfers)
{
    ++d_ptr->transactionDepth;
    if (blockPublicTransfers) ++d_ptr->publicTransferBlockDepth;
    if (d_ptr->transactionDepth == 1 || blockPublicTransfers) {
        for (const auto &group : groupIds()) {
            if (auto *tabs = tabWidget(group); tabs != nullptr)
                tabs->beginCoordinatorTransaction(blockPublicTransfers);
        }
    }
}

void ZzSplitWorkspace::endInternalTransaction(bool blockPublicTransfers)
{
    if (d_ptr->transactionDepth <= 0) return;
    --d_ptr->transactionDepth;
    if (blockPublicTransfers && d_ptr->publicTransferBlockDepth > 0)
        --d_ptr->publicTransferBlockDepth;
    if (d_ptr->transactionDepth == 0) {
        for (const auto &group : groupIds()) {
            if (auto *tabs = tabWidget(group); tabs != nullptr)
                tabs->endCoordinatorTransaction(blockPublicTransfers);
        }
        d_ptr->processPendingEmptyGroups();
        if (d_ptr->activePagePublishPending) {
            d_ptr->activePagePublishPending = false;
            d_ptr->publishActivePage();
        }
    } else if (blockPublicTransfers) {
        for (const auto &group : groupIds()) {
            if (auto *tabs = tabWidget(group); tabs != nullptr)
                tabs->endCoordinatorTransaction(true);
        }
    }
}

void ZzSplitWorkspace::beginCloseNotificationTransaction()
{
    beginInternalTransaction(true);
}

void ZzSplitWorkspace::endCloseNotificationTransaction()
{
    endInternalTransaction(true);
}

std::shared_ptr<void> ZzSplitWorkspace::captureInternalSnapshot() const
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
            auto *const bar = tabs->fluentTabBar();
            groupSnapshot.tabBarCount = bar != nullptr ? bar->count() : 0;
            if (bar != nullptr) {
                groupSnapshot.tabBarEntries.reserve(
                    static_cast<std::size_t>(bar->count()));
                for (int index = 0; index < bar->count(); ++index) {
                    auto entry = ZzTabWidgetPrivate::snapshotFor(tabs, index);
                    if (index >= tabs->count()) {
                        entry.page = nullptr;
                        entry.text = bar->tabText(index);
                        entry.icon = bar->tabIcon(index);
                        entry.toolTip = bar->tabToolTip(index);
                        entry.whatsThis = bar->tabWhatsThis(index);
                        entry.data = bar->tabData(index);
                        entry.textColor = bar->tabTextColor(index);
                        entry.enabled = bar->isTabEnabled(index);
                        entry.visible = bar->isTabVisible(index);
                    }
                    groupSnapshot.tabBarEntries.push_back(std::move(entry));
                }
            }
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

bool ZzSplitWorkspace::internalSnapshotMatches(
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
            || tabs->fluentTabBar() == nullptr
            || tabs->fluentTabBar()->count() != group.tabBarCount
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
                || actual.visible != expected.tab.visible
                || actual.pinned != expected.tab.pinned
                || actual.modified != expected.tab.modified
                || actual.attention != expected.tab.attention
                || actual.closeEnabled != expected.tab.closeEnabled
                || expected.tab.page->windowTitle() != expected.pageTitle
                || pageLayoutKey(actual.page) != expected.layoutKey) {
                return false;
            }
        }
        auto *const bar = tabs->fluentTabBar();
        for (std::size_t index = 0; index < group.tabBarEntries.size(); ++index) {
            const auto &expected = group.tabBarEntries[index];
            if (bar->tabText(static_cast<int>(index)) != expected.text
                || !zzCoordinatorIconMatches(
                    bar->tabIcon(static_cast<int>(index)), expected.icon)
                || bar->tabToolTip(static_cast<int>(index)) != expected.toolTip
                || bar->tabWhatsThis(static_cast<int>(index)) != expected.whatsThis
                || bar->tabData(static_cast<int>(index)) != expected.data
                || bar->tabTextColor(static_cast<int>(index)) != expected.textColor
                || bar->isTabEnabled(static_cast<int>(index)) != expected.enabled
                || bar->isTabVisible(static_cast<int>(index)) != expected.visible) {
                return false;
            }
        }
    }
    return true;
}

bool ZzSplitWorkspace::restoreInternalSnapshot(
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
        auto *const bar = tabs->fluentTabBar();
        if (bar == nullptr) return false;
        const auto barEntryMatches = [bar](int index,
                                       const ZzTabTransferSnapshot &expected) {
            return bar->tabText(index) == expected.text
                && zzCoordinatorIconMatches(bar->tabIcon(index), expected.icon)
                && bar->tabToolTip(index) == expected.toolTip
                && bar->tabWhatsThis(index) == expected.whatsThis
                && bar->tabData(index) == expected.data
                && bar->tabTextColor(index) == expected.textColor
                && bar->isTabEnabled(index) == expected.enabled
                && bar->isTabVisible(index) == expected.visible;
        };
        int expectedIndex = 0;
        for (int index = 0; index < bar->count();) {
            if (expectedIndex < group.tabBarCount
                && barEntryMatches(index,
                    group.tabBarEntries[static_cast<std::size_t>(expectedIndex)])) {
                ++index;
                ++expectedIndex;
            } else {
                static_cast<QTabBar *>(bar)->removeTab(index);
            }
        }
        while (bar->count() < group.tabBarCount) {
            const auto &entry = group.tabBarEntries[static_cast<std::size_t>(bar->count())];
            static_cast<QTabBar *>(bar)->addTab(entry.icon, entry.text);
        }
        for (int index = 0; index < group.tabBarCount; ++index) {
            const auto &entry = group.tabBarEntries[static_cast<std::size_t>(index)];
            static_cast<QTabBar *>(bar)->setTabText(index, entry.text);
            static_cast<QTabBar *>(bar)->setTabIcon(index, entry.icon);
            static_cast<QTabBar *>(bar)->setTabToolTip(index, entry.toolTip);
            static_cast<QTabBar *>(bar)->setTabWhatsThis(index, entry.whatsThis);
            static_cast<QTabBar *>(bar)->setTabData(index, entry.data);
            static_cast<QTabBar *>(bar)->setTabTextColor(index, entry.textColor);
            static_cast<QTabBar *>(bar)->setTabEnabled(index, entry.enabled);
            static_cast<QTabBar *>(bar)->setTabVisible(index, entry.visible);
        }
        tabs->setCurrentWidget(group.current);
    }
    for (const auto &connection : d_ptr->pageDestroyedConnections) {
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
    return internalSnapshotMatches(opaque);
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
    d_ptr->publishActivePage();
    if (guardedWorkspace.isNull()) {
        return true;
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
        d_ptr->publishActivePage();
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
