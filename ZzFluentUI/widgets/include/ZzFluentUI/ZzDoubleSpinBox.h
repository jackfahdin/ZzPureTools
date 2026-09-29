#pragma once

#include <memory>

#include <QtWidgets/QDoubleSpinBox>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzSpinBoxButtonLayout.h>

namespace ZzFluentUI {

class ZzDoubleSpinBoxPrivate;

/** @brief 保留 QDoubleSpinBox 完整数值输入语义的 Fluent 浮点输入框。 */
class ZZ_FLUENT_UI_EXPORT ZzDoubleSpinBox final : public QDoubleSpinBox
{
    Q_OBJECT
    Q_PROPERTY(ZzFluentUI::ZzSpinBoxButtonLayout buttonLayout
        READ buttonLayout WRITE setButtonLayout NOTIFY buttonLayoutChanged)
    Q_DISABLE_COPY_MOVE(ZzDoubleSpinBox)

public:
    /** @brief 创建默认使用右侧横排箭头的浮点输入框。 */
    explicit ZzDoubleSpinBox(QWidget *parent = nullptr);

    /** @brief 使用 Qt 父子所有权销毁内部编辑器和动作。 */
    ~ZzDoubleSpinBox() override;

    /** @brief 返回按钮排列模式。 */
    [[nodiscard]] ZzSpinBoxButtonLayout buttonLayout() const noexcept;

    /**
     * @brief 切换布局并立即更新输入区域，不改变数值或编辑内容。
     * @param layout 新模式；无效枚举被忽略。
     *
     * 同步箭头或加减符号；已设置 NoButtons 时保持隐藏。
     * 调用者之后仍可用 Qt 的 setButtonSymbols 覆盖符号。
     */
    void setButtonLayout(ZzSpinBoxButtonLayout layout);

Q_SIGNALS:
    /** @brief 按钮排列改变时通知；重复设置相同模式不发送通知。 */
    void buttonLayoutChanged(ZzFluentUI::ZzSpinBoxButtonLayout layout);

private:
    std::unique_ptr<ZzDoubleSpinBoxPrivate> d_ptr;
};

} // namespace ZzFluentUI
