#pragma once

#include <QtCore/QString>

#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>

namespace ZzPureTools {

class ZzWorkspaceShellPrivate;

/** @brief 事务搬运 ApplicationWindow 导航表面并审计对象身份。 */
class ZzWorkspaceNavigationIntegrationTransactionPrivate final
{
public:
    /** @brief 执行单次导航表面集成，失败时恢复已完成阶段。 */
    [[nodiscard]] static ZzCore::ZzResult<void> execute(
        ZzWorkspaceShellPrivate &shell,
        const ZzWorkspacePanelId &panelId,
        const QString &panelTitle,
        ZzFluentUI::ZzIconDescriptor icon,
        ZzFluentUI::ZzActivityArea area,
        const QString &centralTabTitle);
};

} // namespace ZzPureTools
