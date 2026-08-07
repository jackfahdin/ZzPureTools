#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QtTypes>

namespace ZzFluentUI {

/** @brief 标识随 ZzFluentUI 发布的内嵌 SVG 图标。 */
enum class ZzBundledSvgIcon : quint8
{
    Close,
    ComputerSystem,
    FullScreen,
    Maximize,
    Minimize,
    Moon,
    MoreLine,
    Pin,
    PinFill,
    Restore,
    Sun,
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzBundledSvgIcon)
