#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 标识标题栏中可独立控制展示的自绘按钮。 */
enum class ZzTitleBarButton : std::uint8_t
{
    /** @brief 最小化按钮。 */
    Minimize,
    /** @brief 最大化或还原按钮。 */
    Maximize,
    /** @brief 关闭按钮。 */
    Close,
    /** @brief 窗口置顶按钮。 */
    AlwaysOnTop,
    /** @brief 主题切换按钮。 */
    Theme
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzTitleBarButton)
