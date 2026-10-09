#pragma once

#include <memory>
#include <QtGui/QColor>
#include <QtWidgets/QToolButton>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {
class ZzColorPicker;
class ZzColorPickerButtonPrivate;

/** @brief 即时编辑颜色的 Fluent 色块按钮，固定复用由 parent 拥有的弹层。 */
class ZZ_FLUENT_UI_EXPORT ZzColorPickerButton final : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(QColor selectedColor READ selectedColor WRITE setSelectedColor NOTIFY selectedColorChanged)
    Q_DISABLE_COPY_MOVE(ZzColorPickerButton)
public:
    /** @brief 创建父级拥有的按钮、弹层与选择器装配。 */
    explicit ZzColorPickerButton(QWidget *parent = nullptr);
    /** @brief 释放私有状态和固定弹层装配。 */
    ~ZzColorPickerButton() override;
    /** @brief 返回唯一选择器的当前颜色。 */
    [[nodiscard]] QColor selectedColor() const;
    /** @brief 设置有效颜色；同值不通知，回调可同步删除按钮。 */
    void setSelectedColor(QColor color);
    /** @brief 返回稳定的、非拥有的 Fluent 选择器，默认启用 alpha。 */
    [[nodiscard]] ZzColorPicker *colorPicker() const noexcept;
Q_SIGNALS:
    /** @brief 当前颜色实际变化时通知一次，包括弹层内的即时编辑。 */
    void selectedColorChanged(const QColor &color);
protected:
    /** @brief 按当前样式绘制色块、透明度棋盘和 TTF 箭头。 */
    void paintEvent(QPaintEvent *event) override;
    /** @brief 同步语言、布局方向与主题环境变化。 */
    void changeEvent(QEvent *event) override;
private:
    friend class ZzColorPickerButtonPrivate;
    std::unique_ptr<ZzColorPickerButtonPrivate> d_ptr;
};
} // namespace ZzFluentUI
