#pragma once

#include <cstdint>
#include <optional>

#include <QtCore/QMetaType>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QIcon>

#include <ZzPureTools/ZzWorkspaceTitleMode.h>

namespace ZzPureTools {

/** @brief 指定工作区窗口接收关闭请求时的策略。 */
enum class ZzWindowClosePolicy : std::uint8_t
{
    /** @brief 允许窗口按默认流程关闭。 */
    Allow,
    /** @brief 拒绝本次关闭请求。 */
    Deny,
    /** @brief 由上层协调器决定本次关闭结果。 */
    Delegate
};

/** @brief 描述一个工作区窗口的完整展示和尺寸配置。 */
struct ZzWorkspaceWindowConfiguration final
{
    /** @brief 窗口标题。 */
    QString title;
    /** @brief 窗口图标。 */
    QIcon icon;
    /** @brief 工作区标题组合策略。 */
    ZzWorkspaceTitleMode titleMode = ZzWorkspaceTitleMode::Application;
    /** @brief 窗口关闭策略。 */
    ZzWindowClosePolicy closePolicy = ZzWindowClosePolicy::Allow;
    /** @brief 是否始终置顶。 */
    bool alwaysOnTop = false;
    /** @brief 窗口最小尺寸。 */
    QSize minimumSize;
    /** @brief 窗口最大尺寸。 */
    QSize maximumSize;
    /** @brief 窗口首次显示时的几何区域。 */
    QRect initialGeometry;
};

/** @brief 描述工作区窗口配置中可选择覆盖的字段。 */
struct ZzWorkspaceWindowConfigurationPatch final
{
    /** @brief 可选窗口标题覆盖值。 */
    std::optional<QString> title;
    /** @brief 可选窗口图标覆盖值。 */
    std::optional<QIcon> icon;
    /** @brief 可选标题组合策略覆盖值。 */
    std::optional<ZzWorkspaceTitleMode> titleMode;
    /** @brief 可选关闭策略覆盖值。 */
    std::optional<ZzWindowClosePolicy> closePolicy;
    /** @brief 可选置顶状态覆盖值。 */
    std::optional<bool> alwaysOnTop;
    /** @brief 可选最小尺寸覆盖值。 */
    std::optional<QSize> minimumSize;
    /** @brief 可选最大尺寸覆盖值。 */
    std::optional<QSize> maximumSize;
    /** @brief 可选初始几何区域覆盖值。 */
    std::optional<QRect> initialGeometry;
};

} // namespace ZzPureTools

Q_DECLARE_METATYPE(ZzPureTools::ZzWindowClosePolicy)
Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceWindowConfiguration)
Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceWindowConfigurationPatch)
