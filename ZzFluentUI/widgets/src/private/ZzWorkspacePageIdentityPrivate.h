#pragma once

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzWorkspacePageId.h>

class QWidget;

namespace ZzFluentUI {

class ZzSplitWorkspace;

/** @brief 为恢复协调器提供不安装的页面身份接管桥。 */
class ZZ_FLUENT_UI_EXPORT ZzWorkspacePageIdentityPrivate final
{
public:
    /**
     * @brief 在恢复事务中安全接管页面的持久身份。
     * @param workspace 目标工作区。
     * @param page 待接管的页面。
     * @param id 页面稳定身份。
     * @return 接管成功时返回 true。
     */
    static bool adoptPageId(
        ZzSplitWorkspace *workspace,
        QWidget *page,
        const ZzWorkspacePageId &id);
};

} // namespace ZzFluentUI
