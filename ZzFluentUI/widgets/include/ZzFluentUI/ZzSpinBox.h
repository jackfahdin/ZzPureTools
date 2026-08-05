#pragma once

#include <QtWidgets/QSpinBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/** @brief 保留 QSpinBox 完整数值输入语义的 Fluent 整数输入框。 */
class ZZ_FLUENT_UI_EXPORT ZzSpinBox final : public QSpinBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSpinBox)

public:
    /** @brief 创建默认使用加减按钮符号的整数输入框。 */
    explicit ZzSpinBox(QWidget *parent = nullptr);

    /** @brief 使用 Qt 父子所有权销毁内部编辑器和动作。 */
    ~ZzSpinBox() override;
};

} // namespace ZzFluentUI
