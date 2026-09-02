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
    ZzWorkspaceDropZone zone)
{
    if (source == nullptr || target == nullptr || source == target
        || zone != ZzWorkspaceDropZone::Center || source->thread() != QThread::currentThread()
        || target->thread() != QThread::currentThread()) {
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("invalid cross-workspace transfer arguments"));
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
    DepthGuard sourceGuard(source, sourcePrivate);
    DepthGuard targetGuard(target, targetPrivate);
    const bool transferred = sourceTabs->d_ptr->transferToDirect(
        targetTabs, sourceIndex, targetIndex);

    if (!transferred || guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull()) {
        return zzCrossTransferFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("cross-workspace transfer did not commit"));
    }

    if (sourceTabs.isNull() || targetTabs.isNull()
        || targetTabs->indexOf(guardedPage) < 0
        || sourceTabs->indexOf(guardedPage) >= 0) {
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
    Q_EMIT guardedTarget->tabTransferCommitted(
        guardedSource, sourceGroup, targetGroup, guardedPage, id, zone);
    if (activeChanged && !guardedTarget.isNull()) {
        Q_EMIT guardedTarget->activeGroupChanged(targetGroup);
    }
    return ZzCore::ZzResult<void>::success();
}

} // namespace ZzFluentUI
