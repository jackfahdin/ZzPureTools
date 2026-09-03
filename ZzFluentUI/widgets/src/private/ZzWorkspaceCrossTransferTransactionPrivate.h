#pragma once

#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>

namespace ZzFluentUI {

class ZzWorkspaceCrossTransferTransactionPrivate final
{
public:
    static ZzCore::ZzResult<void> run(
        ZzSplitWorkspace *source,
        const ZzTabGroupId &sourceGroup,
        int sourceIndex,
        ZzSplitWorkspace *target,
        const ZzTabGroupId &targetGroup,
        int targetIndex,
        ZzWorkspaceDropZone zone,
        bool emitSignals = true);

};

} // namespace ZzFluentUI
