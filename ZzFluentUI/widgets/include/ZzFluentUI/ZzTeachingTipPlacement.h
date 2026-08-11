#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定教学提示相对目标控件的首选方向。 */
enum class ZzTeachingTipPlacement : std::uint8_t
{
    Auto,
    Top,
    Bottom,
    Left,
    Right
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzTeachingTipPlacement)
