#pragma once
#include <ZzFluentUI/ZzRangeSlider.h>

namespace ZzFluentUI {

/** @brief 注册范围滑块及两个端点的公开 Qt 无障碍接口工厂。 */
class ZzRangeSliderAccessible final
{
public:
    /** @brief 注册范围滑块及两个端点的公开 Qt 无障碍接口工厂。 */
    static void install();
    /** @brief 通知指定端点已提交的值发生变化。 */
    static void notifyValueChanged(ZzRangeSlider *slider, ZzRangeSlider::ZzSliderHandle handle, int value);
    /** @brief 通知活动端点获得键盘焦点。 */
    static void notifyFocusChanged(ZzRangeSlider *slider);
};

} // namespace ZzFluentUI
