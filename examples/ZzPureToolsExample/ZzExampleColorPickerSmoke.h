#pragma once
class QWidget;
namespace ZzExample {

/** @brief 驱动真实输入、属性、弹层和确认/取消，最后恢复示例默认值。 */
class ZzExampleColorPickerSmoke final
{
public:
    /** @brief 驱动真实输入、属性、弹层和确认/取消，最后恢复示例默认值。 */
    [[nodiscard]] static bool isPageReady(const QWidget &window);
};

} // namespace ZzExample
