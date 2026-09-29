#pragma once

namespace ZzFluentUI {

/** @brief 线性进度条外观；仅改变绘制粗细，不改变进度值及布局尺寸。 */
enum class ZzProgressBarAppearance
{
    Thin, /**< 细线：1px 中性轨道与 3px 强调色填充，默认外观。 */
    Thick /**< 粗线：4px 等厚圆角轨道与强调色填充。 */
};

} // namespace ZzFluentUI
