#pragma once
class QWidget;
namespace ZzExample {
/** @brief 驱动真实输入、属性、弹层和确认/取消，最后恢复示例默认值。 */
[[nodiscard]] bool zzColorPickerPageReady(const QWidget &window);
} // namespace ZzExample
