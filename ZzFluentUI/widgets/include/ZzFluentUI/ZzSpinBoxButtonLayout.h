#pragma once

#include <cstdint>

namespace ZzFluentUI {

/**
 * @brief 数值输入框按钮的排列模式。
 *
 * 以下左右位置以从左到右排版描述；RTL 时整体镜像，增减语义不变。
 */
enum class ZzSpinBoxButtonLayout : std::uint8_t
{
    /** @brief 右侧竖排箭头：上方增大，下方减小。 */
    Vertical,
    /** @brief 两侧箭头：左侧向下箭头减小，中间数值，右侧向上箭头增大。 */
    HorizontalSides,
    /** @brief 右侧横排箭头：数值在左，右侧先减小后增大；默认模式。 */
    HorizontalRight,
    /** @brief 两侧加减：左侧减号减小，中间数值，右侧加号增大。 */
    PlusMinusHorizontalSides
};

} // namespace ZzFluentUI
