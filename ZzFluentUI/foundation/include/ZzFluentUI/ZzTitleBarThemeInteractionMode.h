#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定标题栏主题按钮的交互方式。 */
enum class ZzTitleBarThemeInteractionMode : std::uint8_t
{
    /** @brief 点击按钮打开主题模式菜单。 */
    Menu,

    /** @brief 点击按钮请求在浅色与深色之间切换。 */
    Toggle
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzTitleBarThemeInteractionMode)
