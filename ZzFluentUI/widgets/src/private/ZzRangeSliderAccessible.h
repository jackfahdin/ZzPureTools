#pragma once
#include <ZzFluentUI/ZzRangeSlider.h>

namespace ZzFluentUI {
/** @brief 注册范围滑块及两个端点的公开 Qt 无障碍接口工厂。 */
void zzInstallRangeSliderAccessibility();
/** @brief 通知指定端点已提交的值发生变化。 */
void zzRangeSliderAccessibleValueChanged(ZzRangeSlider *slider, ZzRangeSlider::Handle handle, int value);
/** @brief 通知活动端点获得键盘焦点。 */
void zzRangeSliderAccessibleFocusChanged(ZzRangeSlider *slider);
} // namespace ZzFluentUI
