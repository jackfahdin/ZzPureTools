#pragma once

#include <QtWidgets/QDoubleSpinBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/** @brief 保留 QDoubleSpinBox 完整数值输入语义的 Fluent 浮点输入框。 */
class ZZ_FLUENT_UI_EXPORT ZzDoubleSpinBox final : public QDoubleSpinBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzDoubleSpinBox)

public:
    /** @brief 创建默认使用加减按钮符号的浮点输入框。 */
    explicit ZzDoubleSpinBox(QWidget *parent = nullptr);

    /** @brief 使用 Qt 父子所有权销毁内部编辑器和动作。 */
    ~ZzDoubleSpinBox() override;
};

} // namespace ZzFluentUI
