#pragma once

#include <ZzFluentUI/ZzSpinBoxButtonLayout.h>

namespace ZzFluentUI {

class ZzDoubleSpinBox;

/** @brief 隔离数值控件布局状态，不把实现数据暴露到公开 ABI。 */
class ZzDoubleSpinBoxPrivate final
{
public:
    /** @brief 验证并更新布局和原生编辑器；返回是否发生有效变化。 */
    bool setButtonLayout(ZzDoubleSpinBox *spinBox, ZzSpinBoxButtonLayout layout);

    ZzSpinBoxButtonLayout buttonLayout = ZzSpinBoxButtonLayout::HorizontalRight;
};

} // namespace ZzFluentUI
