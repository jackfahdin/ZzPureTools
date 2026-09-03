#include "ZzSplitWorkspaceTransactionPrivate.h"

#include <ZzFluentUI/ZzSplitWorkspace.h>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

namespace ZzFluentUI {

void ZzSplitWorkspaceTransactionPrivate::begin(ZzSplitWorkspace *workspace)
{
    if (workspace != nullptr) workspace->beginInternalTransaction();
}

void ZzSplitWorkspaceTransactionPrivate::end(ZzSplitWorkspace *workspace)
{
    if (workspace != nullptr) workspace->endInternalTransaction();
}

std::shared_ptr<void> ZzSplitWorkspaceTransactionPrivate::capture(
    const ZzSplitWorkspace *workspace)
{
    return workspace != nullptr ? workspace->captureInternalSnapshot() : nullptr;
}

bool ZzSplitWorkspaceTransactionPrivate::matches(
    const ZzSplitWorkspace *workspace, const std::shared_ptr<void> &snapshot)
{
    return workspace != nullptr && workspace->internalSnapshotMatches(snapshot);
}

bool ZzSplitWorkspaceTransactionPrivate::restore(
    ZzSplitWorkspace *workspace, const std::shared_ptr<void> &snapshot)
{
    return workspace != nullptr && workspace->restoreInternalSnapshot(snapshot);
}

ZzCore::ZzResult<void> ZzSplitWorkspaceTransactionPrivate::transferSilently(
    ZzSplitWorkspace *source,
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *target,
    const ZzTabGroupId &targetGroup,
    int targetIndex)
{
    return source != nullptr ? source->transferTabToWorkspaceSilently(
        sourceGroup, sourceIndex, target, targetGroup, targetIndex)
        : ZzCore::ZzResult<void>::failure(
            ZzCore::ZzError(ZzCore::ZzErrorCode::InvalidArgument, {}));
}

bool ZzSplitWorkspaceTransactionPrivate::restoreGroupOrder(
    ZzSplitWorkspace *workspace,
    const ZzTabGroupId &group,
    const QList<QWidget *> &pages)
{
    return workspace != nullptr
        && workspace->restoreGroupOrderSilently(group, pages);
}

} // namespace ZzFluentUI
