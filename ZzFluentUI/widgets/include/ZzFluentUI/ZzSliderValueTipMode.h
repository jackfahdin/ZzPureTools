#pragma once

#include <cstdint>
#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 滑块调节提示的显示方式，不改变滑块本身的取值范围。 */
enum class ZzSliderValueTipMode : std::uint8_t
{
    Disabled,   ///< 关闭调节提示。
    Value,      ///< 显示原始整数值。
    Percentage  ///< 将当前范围归一化为 0%～100%，四舍五入为整数。
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzSliderValueTipMode)
