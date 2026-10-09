#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QPointer>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzWorkspaceShell.h>

namespace ZzPureTools {

/** @brief 非拥有地观察同一工作区中的应用窗口和 Shell。 */
struct ZzWorkspaceWindowHandle final
{
    /** @brief 应用拥有的窗口观察值。 */
    QPointer<ZzApplicationWindow> window;
    /** @brief 窗口工作区 Shell 的观察值。 */
    QPointer<ZzWorkspaceShell> shell;

    /** @brief 当窗口和 Shell 均仍存在时返回 true。 */
    [[nodiscard]] bool isValid() const noexcept
    {
        return window && shell;
    }
};

} // namespace ZzPureTools

Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceWindowHandle)
