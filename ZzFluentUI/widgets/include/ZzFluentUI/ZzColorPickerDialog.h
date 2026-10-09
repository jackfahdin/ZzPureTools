#pragma once

#include <memory>
#include <QtGui/QColor>
#include <QtWidgets/QDialog>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {
class ZzColorPicker;
class ZzColorPickerDialogPrivate;

/** @brief 提供即时预览、确定提交和取消回滚的 Fluent 颜色对话框。 */
class ZZ_FLUENT_UI_EXPORT ZzColorPickerDialog final : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor NOTIFY currentColorChanged)
    Q_DISABLE_COPY_MOVE(ZzColorPickerDialog)
public:
    /** @brief 创建父级拥有、默认启用 alpha 的 Fluent 颜色对话框。 */
    explicit ZzColorPickerDialog(QWidget *parent = nullptr);
    /** @brief 释放固定选择器与对话框装配。 */
    ~ZzColorPickerDialog() override;
    /** @brief 返回与窗口标题同步的正文标题。 */
    [[nodiscard]] QString title() const;
    /** @brief 设置窗口和正文共享的纯文本标题。 */
    void setTitle(const QString &title);
    /** @brief 返回唯一选择器的当前颜色。 */
    [[nodiscard]] QColor currentColor() const;
    /** @brief 设置有效当前颜色，隐藏时也更新下次会话的初值。 */
    void setCurrentColor(QColor color);
    /** @brief 返回稳定、非拥有的 Fluent 选择器；默认启用 alpha。 */
    [[nodiscard]] ZzColorPicker *colorPicker() const noexcept;
    /** @brief 返回适合完整 Fluent 装配的首选尺寸，宽度至少 480。 */
    [[nodiscard]] QSize sizeHint() const override;
    /** @brief 每次重新显示时保存本次会话的颜色初值。 */
    void setVisible(bool visible) override;
public Q_SLOTS:
    /** @brief 确定前提交待编辑 HEX；其他结果恢复本次打开的初值。 */
    void done(int result) override;
Q_SIGNALS:
    /** @brief 即时颜色变化通知；取消时通知已发布预览的回滚结果。 */
    void currentColorChanged(const QColor &color);
    /** @brief 仅确定发出，位于最终预览通知之后；回调可同步销毁对话框。 */
    void colorSelected(const QColor &color);
protected:
    /** @brief 同步语言及主题变化，保留调用者设置的自定义标题。 */
    void changeEvent(QEvent *event) override;
    /** @brief 使用当前主题绘制正文与底部区域。 */
    void paintEvent(QPaintEvent *event) override;
private:
    std::unique_ptr<ZzColorPickerDialogPrivate> d_ptr;
};
} // namespace ZzFluentUI
