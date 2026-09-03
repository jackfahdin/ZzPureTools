#include "ZzWorkspaceCrossTransferTransactionPrivate.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QThread>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzTabWidget.h>

#include "ZzSplitWorkspacePrivate.h"
#include "ZzTabWidgetPrivate.h"

namespace ZzFluentUI {

namespace {

struct DepthGuard final {
    QPointer<ZzSplitWorkspace> workspace;
    ZzSplitWorkspacePrivate *priv = nullptr;
    DepthGuard(ZzSplitWorkspace *value, ZzSplitWorkspacePrivate *p)
        : workspace(value), priv(p) { ++priv->transactionDepth; }
    ~DepthGuard() { if (!workspace.isNull()) { --priv->transactionDepth; } }
};

struct TabsSnapshot final {
    QPointer<ZzTabWidget> tabs;
    QList<ZzTabTransferSnapshot> pages;
    int current = -1;
};

struct WorkspaceSnapshot final {
    QList<TabsSnapshot> tabs;
    ZzTabGroupId active;
    QHash<QWidget *, ZzWorkspacePageId> pageIds;
    QHash<ZzWorkspacePageId, QPointer<QWidget>> pagesById;
    std::vector<ZzWorkspacePageKey> pageKeys;
};

WorkspaceSnapshot captureWorkspace(ZzSplitWorkspacePrivate *workspace)
{
    WorkspaceSnapshot snapshot;
    snapshot.active = workspace->activeId;
    snapshot.pageIds = workspace->pageIds;
    snapshot.pagesById = workspace->pagesById;
    snapshot.pageKeys = workspace->pageKeys;
    std::vector<ZzNode *> leaves;
    ZzSplitWorkspacePrivate::collectLeaves(workspace->root.get(), leaves);
    for (auto *node : leaves) {
        auto tabs = std::get<ZzLeaf>(node->value).tabs;
        if (tabs.isNull()) continue;
        TabsSnapshot value;
        value.tabs = tabs;
        value.current = tabs->currentIndex();
        for (int i = 0; i < tabs->count(); ++i) {
            value.pages.push_back(ZzTabWidgetPrivate::snapshotFor(tabs, i));
        }
        snapshot.tabs.push_back(std::move(value));
    }
    return snapshot;
}

bool restoreWorkspace(const WorkspaceSnapshot &snapshot)
{
    for (const auto &state : snapshot.tabs) {
        if (state.tabs.isNull()) return false;
        for (const auto &page : state.pages) {
            if (page.page.isNull()) return false;
            const int index = state.tabs->indexOf(page.page);
            if (index < 0 || !ZzTabWidgetPrivate::restoreMetadata(
                                  state.tabs, index, page)) return false;
        }
        for (int desired = 0; desired < state.pages.size(); ++desired) {
            if (state.pages.at(desired).page.isNull()) return false;
            const int actual = state.tabs->indexOf(state.pages.at(desired).page);
            if (actual < 0) return false;
            if (actual != desired && !ZzTabWidgetPrivate::transferDirectFor(
                    state.tabs, state.tabs, actual, desired)) return false;
        }
        if (state.current >= 0 && state.current < state.tabs->count())
            state.tabs->setCurrentIndex(state.current);
    }
    return true;
}

void rebuildPageConnections(ZzSplitWorkspacePrivate *workspace)
{
    for (const auto &connection : workspace->pageDestroyedConnections)
        QObject::disconnect(connection);
    workspace->pageDestroyedConnections.clear();
    for (auto it = workspace->pageIds.cbegin(); it != workspace->pageIds.cend(); ++it) {
        QWidget *const page = it.key();
        workspace->pageDestroyedConnections.insert(page,
            QObject::connect(page, &QObject::destroyed, workspace->q_ptr,
                [workspace](QObject *object) {
                    auto *const widget = static_cast<QWidget *>(object);
                    const auto found = workspace->pageIds.find(widget);
                    if (found != workspace->pageIds.end()) {
                        workspace->pagesById.remove(found.value());
                        workspace->pageIds.erase(found);
                    }
                    workspace->pageDestroyedConnections.remove(widget);
                }));
    }
}

bool metadataMatches(const WorkspaceSnapshot &snapshot, QWidget *ignored,
                     ZzTabWidget *ignoredOwner, bool ignoredRemoved,
                     const ZzTabTransferSnapshot *added,
                     int addedIndex)
{
    for (const auto &state : snapshot.tabs) {
        if (state.tabs.isNull()) return false;
        QList<ZzTabTransferSnapshot> expectedPages = state.pages;
        if (added != nullptr && state.tabs == ignoredOwner) {
            const int slot = addedIndex < 0
                ? static_cast<int>(expectedPages.size())
                : std::clamp(addedIndex, 0, static_cast<int>(expectedPages.size()));
            expectedPages.insert(slot, *added);
            std::stable_partition(expectedPages.begin(), expectedPages.end(),
                                  [](const auto &page) { return page.pinned; });
        }
        if (ignoredRemoved) {
            expectedPages.erase(std::remove_if(expectedPages.begin(), expectedPages.end(),
                [ignored](const auto &p) { return p.page == ignored; }), expectedPages.end());
        }
        if (state.tabs->count() != expectedPages.size()) return false;
        for (int i = 0; i < expectedPages.size(); ++i) {
            const auto &expected = expectedPages.at(i);
            const auto current = ZzTabWidgetPrivate::snapshotFor(state.tabs, i);
            if (current.page != expected.page || current.text != expected.text
                || current.icon.cacheKey() != expected.icon.cacheKey()
                || current.toolTip != expected.toolTip || current.whatsThis != expected.whatsThis
                || current.enabled != expected.enabled || current.pinned != expected.pinned
                || current.modified != expected.modified || current.attention != expected.attention
                || current.closeEnabled != expected.closeEnabled || current.data != expected.data
                || current.textColor != expected.textColor) return false;
        }
    }
    return true;
}

/** @brief 清理被外部容器接管页面的工作区登记，不回收页面对象。 */
void clearWorkspaceRegistration(ZzSplitWorkspacePrivate *workspace, QWidget *page)
{
    if (workspace == nullptr || page == nullptr) return;
    const auto id = workspace->pageIds.take(page);
    if (id.isValid()) workspace->pagesById.remove(id);
    QObject::disconnect(workspace->pageDestroyedConnections.take(page));
    workspace->pageKeys.erase(std::remove_if(workspace->pageKeys.begin(), workspace->pageKeys.end(),
        [page](const ZzWorkspacePageKey &entry) { return entry.page == page; }),
        workspace->pageKeys.end());
}

void clearTabMetadata(ZzSplitWorkspacePrivate *workspace, QWidget *page)
{
    if (workspace == nullptr || page == nullptr) return;
    std::vector<ZzNode *> leaves;
    ZzSplitWorkspacePrivate::collectLeaves(workspace->root.get(), leaves);
    for (auto *node : leaves) {
        const auto tabs = std::get<ZzLeaf>(node->value).tabs;
        if (!tabs.isNull()) ZzTabWidgetPrivate::clearMetadataFor(tabs, page);
    }
}

bool mappingsMatch(const ZzSplitWorkspacePrivate *workspace,
                   const WorkspaceSnapshot &snapshot,
                   QWidget *movedPage,
                   const ZzWorkspacePageId &movedId,
                   const QString &movedKey,
                   bool destination)
{
    const auto owns = [workspace](QWidget *page) {
        std::vector<const ZzNode *> leaves;
        ZzSplitWorkspacePrivate::collectLeaves(workspace->root.get(), leaves);
        return std::any_of(leaves.cbegin(), leaves.cend(), [page](const ZzNode *node) {
            const auto tabs = std::get<ZzLeaf>(node->value).tabs;
            return !tabs.isNull() && tabs->indexOf(page) >= 0;
        });
    };
    QHash<QWidget *, ZzWorkspacePageId> expected = snapshot.pageIds;
    if (destination) expected.insert(movedPage, movedId);
    else expected.remove(movedPage);
    if (workspace->pageIds != expected || workspace->pagesById.size() != expected.size())
        return false;
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        if (it.key() == nullptr || !owns(it.key())
            || workspace->pagesById.value(it.value()) != it.key()) return false;
    }
    std::vector<ZzWorkspacePageKey> expectedKeys = snapshot.pageKeys;
    if (destination && !movedKey.isEmpty()) expectedKeys.push_back({movedPage, movedKey});
    if (!destination) {
        expectedKeys.erase(std::remove_if(expectedKeys.begin(), expectedKeys.end(),
            [movedPage](const auto &entry) { return entry.page == movedPage; }), expectedKeys.end());
    }
    if (workspace->pageKeys.size() != expectedKeys.size()) return false;
    for (const auto &entry : expectedKeys) {
        const auto found = std::find_if(workspace->pageKeys.cbegin(), workspace->pageKeys.cend(),
            [&entry](const auto &current) { return current.page == entry.page && current.key == entry.key; });
        if (found == workspace->pageKeys.cend()) return false;
    }
    for (auto i = workspace->pageKeys.cbegin(); i != workspace->pageKeys.cend(); ++i) {
        for (auto j = i + 1; j != workspace->pageKeys.cend(); ++j)
            if (i->key == j->key) return false;
    }
    return true;
}

[[nodiscard]] ZzCore::ZzResult<void> zzCrossTransferFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<void>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

} // namespace

ZzCore::ZzResult<void> ZzWorkspaceCrossTransferTransactionPrivate::run(
    ZzSplitWorkspace *source,
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *target,
    const ZzTabGroupId &targetGroup,
    int targetIndex,
    ZzWorkspaceDropZone zone,
    bool emitSignals)
{
    if (source == nullptr || target == nullptr || source == target
        || source->thread() != QThread::currentThread()
        || target->thread() != QThread::currentThread()) {
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("invalid cross-workspace transfer arguments"));
    }
    if (zone != ZzWorkspaceDropZone::Center) {
        QPointer<ZzSplitWorkspace> guardedSource = source;
        QPointer<ZzSplitWorkspace> guardedTarget = target;
        const bool horizontal = zone == ZzWorkspaceDropZone::Left || zone == ZzWorkspaceDropZone::Right;
        auto placement = (zone == ZzWorkspaceDropZone::Left || zone == ZzWorkspaceDropZone::Top)
            ? ZzSplitPlacement::Before : ZzSplitPlacement::After;
        if (horizontal && target->layoutDirection() == Qt::RightToLeft) {
            placement = placement == ZzSplitPlacement::Before
                ? ZzSplitPlacement::After : ZzSplitPlacement::Before;
        }
        const auto temp = target->d_ptr->splitGroup(targetGroup,
            horizontal ? Qt::Horizontal : Qt::Vertical, placement, {}, false);
        if (!temp.has_value()) return zzCrossTransferFailure(ZzCore::ZzErrorCode::InvalidState, QStringLiteral("target capacity exceeded"));
        auto *sourceTabs = source->tabWidget(sourceGroup);
        QWidget *const movedPage = sourceTabs != nullptr
            && sourceIndex >= 0 && sourceIndex < sourceTabs->count()
            ? sourceTabs->widget(sourceIndex) : nullptr;
        const ZzWorkspacePageId movedId = source->pageId(movedPage);
        const bool activeChanged = target->activeGroupId() != temp.value();
        const auto result = run(source, sourceGroup, sourceIndex, target, temp.value(), targetIndex, ZzWorkspaceDropZone::Center, false);
        if (!result) {
            if (!guardedTarget.isNull()) {
                guardedTarget->d_ptr->removeEmptyGroup(temp.value());
            }
            return result;
        }
        if (!guardedTarget.isNull()) {
            guardedTarget->d_ptr->rebuildView();
        }
        if (emitSignals && !guardedSource.isNull() && !guardedTarget.isNull()) {
            Q_EMIT guardedTarget->groupAdded(temp.value());
            if (guardedTarget.isNull()) return result;
            Q_EMIT guardedTarget->layoutChanged();
            if (guardedTarget.isNull()) return result;
            if (activeChanged) {
                Q_EMIT guardedTarget->activeGroupChanged(temp.value());
                if (guardedTarget.isNull()) return result;
            }
            Q_EMIT guardedTarget->tabTransferCommitted(
                guardedSource, sourceGroup, temp.value(), movedPage, movedId, zone);
        }
        return result;
    }
    auto *const sourcePrivate = source->d_ptr.get();
    auto *const targetPrivate = target->d_ptr.get();
    if (sourcePrivate->transactionDepth != 0 || targetPrivate->transactionDepth != 0) {
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace transaction is already active"));
    }
    ZzNode *const sourceNode = sourcePrivate->findLeaf(sourceGroup);
    ZzNode *const targetNode = targetPrivate->findLeaf(targetGroup);
    QPointer<ZzTabWidget> sourceTabs = sourceNode != nullptr
        ? std::get<ZzLeaf>(sourceNode->value).tabs : QPointer<ZzTabWidget> {};
    QPointer<ZzTabWidget> targetTabs = targetNode != nullptr
        ? std::get<ZzLeaf>(targetNode->value).tabs : QPointer<ZzTabWidget> {};
    if (sourceTabs.isNull() || targetTabs.isNull() || sourceIndex < 0
        || sourceIndex >= sourceTabs->count() || targetIndex > targetTabs->count()) {
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("unknown group or invalid tab index"));
    }
    QWidget *const page = sourceTabs->widget(sourceIndex);
    if (page == nullptr || targetTabs->indexOf(page) >= 0) {
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("page is not transferable"));
    }
    const QString layoutKey = sourcePrivate->pageLayoutKey(page);
    if (!layoutKey.isEmpty()) {
        const auto existing = std::find_if(
            targetPrivate->pageKeys.cbegin(), targetPrivate->pageKeys.cend(),
            [&layoutKey](const ZzWorkspacePageKey &entry) {
                return !entry.page.isNull() && entry.key == layoutKey;
            });
        if (existing != targetPrivate->pageKeys.cend()) {
            return zzCrossTransferFailure(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("target layout key already exists"));
        }
    }

    const ZzWorkspacePageId id = sourcePrivate->pageId(page);
    if (targetPrivate->pagesById.contains(id)) {
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("target page identity already exists"));
    }
    QPointer<ZzSplitWorkspace> guardedSource = source;
    QPointer<ZzSplitWorkspace> guardedTarget = target;
    QPointer<QWidget> guardedPage = page;
    const WorkspaceSnapshot sourceSnapshot = captureWorkspace(sourcePrivate);
    const WorkspaceSnapshot targetSnapshot = captureWorkspace(targetPrivate);
    const auto rollback = [&]() {
        if (guardedSource.isNull() || guardedTarget.isNull()
            || guardedPage.isNull()) return false;
        bool movedBack = true;
        if (!sourceTabs.isNull() && !targetTabs.isNull()
            && targetTabs->indexOf(guardedPage) >= 0
            && sourceTabs->indexOf(guardedPage) < 0) {
            movedBack = ZzTabWidgetPrivate::transferDirectFor(
                targetTabs, sourceTabs, targetTabs->indexOf(guardedPage), sourceIndex);
        }
        if (!movedBack) return false;
        if (sourceTabs.isNull() || targetTabs.isNull()
            || sourceTabs->indexOf(guardedPage) < 0) return false;
        for (const auto &connection : sourcePrivate->pageDestroyedConnections)
            QObject::disconnect(connection);
        for (const auto &connection : targetPrivate->pageDestroyedConnections)
            QObject::disconnect(connection);
        sourcePrivate->pageDestroyedConnections.clear();
        targetPrivate->pageDestroyedConnections.clear();
        sourcePrivate->pageIds = sourceSnapshot.pageIds;
        sourcePrivate->pagesById = sourceSnapshot.pagesById;
        sourcePrivate->pageKeys = sourceSnapshot.pageKeys;
        sourcePrivate->activeId = sourceSnapshot.active;
        targetPrivate->pageIds = targetSnapshot.pageIds;
        targetPrivate->pagesById = targetSnapshot.pagesById;
        targetPrivate->pageKeys = targetSnapshot.pageKeys;
        targetPrivate->activeId = targetSnapshot.active;
        rebuildPageConnections(sourcePrivate);
        rebuildPageConnections(targetPrivate);
        const bool sourceRestored = restoreWorkspace(sourceSnapshot);
        const bool targetRestored = restoreWorkspace(targetSnapshot);
        return sourceRestored && targetRestored;
    };
    DepthGuard sourceGuard(source, sourcePrivate);
    DepthGuard targetGuard(target, targetPrivate);
    const bool transferred = sourceTabs->d_ptr->transferToDirect(
        targetTabs, sourceIndex, targetIndex, false);

    if (!transferred || guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull()) {
        const bool restored = rollback();
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidState,
            restored ? QStringLiteral("cross-workspace transfer did not commit")
                     : QStringLiteral("cross-workspace rollback failed"));
    }

    if (sourceTabs.isNull() || targetTabs.isNull()
        || targetTabs->indexOf(guardedPage) < 0
        || sourceTabs->indexOf(guardedPage) >= 0) {
        // 第三方可能已接管页面；不抢回，但清理工作区内部残留登记。
        if (!guardedPage.isNull()) {
            clearWorkspaceRegistration(sourcePrivate, guardedPage);
            clearWorkspaceRegistration(targetPrivate, guardedPage);
            clearTabMetadata(sourcePrivate, guardedPage);
            clearTabMetadata(targetPrivate, guardedPage);
        }
        return zzCrossTransferFailure(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("cross-workspace ownership audit failed"));
    }

    const auto sourceIdIt = sourcePrivate->pageIds.find(guardedPage);
    if (sourceIdIt != sourcePrivate->pageIds.end()) {
        sourcePrivate->pagesById.remove(sourceIdIt.value());
        QObject::disconnect(sourcePrivate->pageDestroyedConnections.take(guardedPage));
        sourcePrivate->pageIds.erase(sourceIdIt);
    }
    targetPrivate->pageIds.insert(guardedPage, id);
    targetPrivate->pagesById.insert(id, guardedPage);
    targetPrivate->pageDestroyedConnections.insert(
        guardedPage,
        QObject::connect(guardedPage, &QObject::destroyed, guardedTarget,
            [targetPrivate](QObject *object) {
                auto *const widget = static_cast<QWidget *>(object);
                const auto it = targetPrivate->pageIds.find(widget);
                if (it != targetPrivate->pageIds.end()) {
                    targetPrivate->pagesById.remove(it.value());
                    targetPrivate->pageIds.erase(it);
                }
                targetPrivate->pageDestroyedConnections.remove(widget);
            }));
    if (!layoutKey.isEmpty()) {
        sourcePrivate->pageKeys.erase(std::remove_if(
            sourcePrivate->pageKeys.begin(), sourcePrivate->pageKeys.end(),
            [guardedPage](const ZzWorkspacePageKey &entry) {
                return entry.page == guardedPage;
            }), sourcePrivate->pageKeys.end());
        targetPrivate->pageKeys.push_back({guardedPage, layoutKey});
    }
    const bool activeChanged = targetPrivate->activeId != targetGroup;
    targetPrivate->activeId = targetGroup;
    if (guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull() || sourceTabs.isNull() || targetTabs.isNull()
        || targetTabs->indexOf(guardedPage) < 0
        || sourceTabs->indexOf(guardedPage) >= 0) {
        const bool restored = rollback();
        return zzCrossTransferFailure(ZzCore::ZzErrorCode::InvalidState,
            restored ? QStringLiteral("cross-workspace commit audit failed")
                     : QStringLiteral("cross-workspace rollback failed"));
    }
    const ZzTabTransferSnapshot moved = [&]() {
        for (const auto &state : sourceSnapshot.tabs)
            for (const auto &entry : state.pages)
                if (entry.page == guardedPage) return entry;
        return ZzTabTransferSnapshot {};
    }();
    if (!metadataMatches(sourceSnapshot, guardedPage, sourceTabs, true, nullptr, -1)
        || !metadataMatches(targetSnapshot, guardedPage, targetTabs, false, &moved,
                             targetIndex)
        || !mappingsMatch(sourcePrivate, sourceSnapshot, guardedPage, id, layoutKey, false)
        || !mappingsMatch(targetPrivate, targetSnapshot, guardedPage, id, layoutKey, true)) {
        const bool restored = rollback();
        return zzCrossTransferFailure(ZzCore::ZzErrorCode::InvalidState,
            restored ? QStringLiteral("workspace metadata audit failed")
                     : QStringLiteral("cross-workspace rollback failed"));
    }
    // The notification is a commit point only after both sides still match.
    if (emitSignals && activeChanged && !guardedTarget.isNull()) {
        Q_EMIT guardedTarget->activeGroupChanged(targetGroup);
    }
    if (emitSignals && !guardedTarget.isNull()) {
        Q_EMIT guardedTarget->tabTransferCommitted(
            guardedSource, sourceGroup, targetGroup, guardedPage, id, zone);
    }
    if (guardedSource.isNull() || guardedTarget.isNull()
        || !mappingsMatch(sourcePrivate, sourceSnapshot, guardedPage, id, layoutKey, false)
        || !mappingsMatch(targetPrivate, targetSnapshot, guardedPage, id, layoutKey, true)
        || !metadataMatches(sourceSnapshot, guardedPage, sourceTabs, true, nullptr, -1)
        || !metadataMatches(targetSnapshot, guardedPage, targetTabs, false, &moved,
                             targetIndex)) {
        const bool restored = rollback();
        return zzCrossTransferFailure(ZzCore::ZzErrorCode::InvalidState,
            restored ? QStringLiteral("workspace changed during notification")
                     : QStringLiteral("cross-workspace rollback failed"));
    }
    return ZzCore::ZzResult<void>::success();
}

} // namespace ZzFluentUI
