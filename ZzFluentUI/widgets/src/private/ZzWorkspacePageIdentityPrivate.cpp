#include "ZzWorkspacePageIdentityPrivate.h"

#include <ZzFluentUI/ZzSplitWorkspace.h>

#include "ZzSplitWorkspacePrivate.h"

namespace ZzFluentUI {

bool ZzWorkspacePageIdentityPrivate::adoptPageId(
    ZzSplitWorkspace *workspace,
    QWidget *page,
    const ZzWorkspacePageId &id)
{
    return workspace != nullptr
        && workspace->d_ptr->adoptPageId(page, id);
}

} // namespace ZzFluentUI
