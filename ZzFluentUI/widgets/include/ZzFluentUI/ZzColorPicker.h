#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;

namespace ZzFluentUI {

class ZzColorPickerPrivate;

/**
 * @brief 提供可组合的色板、RGB(A)、十六进制和透明度颜色编辑。
 *
 * currentColor 是唯一当前值，palette model 是唯一色板集合。组件只维护
 * 颜色输入和展示状态，不创建窗口、不持久化颜色，也不访问业务服务。
 */
class ZZ_FLUENT_UI_EXPORT ZzColorPicker final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(ZzColorPickerAppearance appearance READ appearance WRITE setAppearance NOTIFY appearanceChanged)
    Q_PROPERTY(ZzColorRepresentation colorRepresentation READ colorRepresentation WRITE setColorRepresentation NOTIFY colorRepresentationChanged)
    Q_PROPERTY(ZzColorSpectrumShape colorSpectrumShape READ colorSpectrumShape WRITE setColorSpectrumShape NOTIFY colorSpectrumShapeChanged)
    Q_PROPERTY(bool colorSpectrumVisible READ isColorSpectrumVisible WRITE setColorSpectrumVisible NOTIFY colorSpectrumVisibleChanged)
    Q_PROPERTY(bool colorPaletteVisible READ isColorPaletteVisible WRITE setColorPaletteVisible NOTIFY colorPaletteVisibleChanged)
    Q_PROPERTY(bool colorPreviewVisible READ isColorPreviewVisible WRITE setColorPreviewVisible NOTIFY colorPreviewVisibleChanged)
    Q_PROPERTY(bool alphaSliderVisible READ isAlphaSliderVisible WRITE setAlphaSliderVisible NOTIFY alphaSliderVisibleChanged)
    Q_PROPERTY(bool colorSliderVisible READ isColorSliderVisible WRITE setColorSliderVisible NOTIFY colorSliderVisibleChanged)
    Q_PROPERTY(bool colorChannelTextInputVisible READ isColorChannelTextInputVisible WRITE setColorChannelTextInputVisible NOTIFY colorChannelTextInputVisibleChanged)
    Q_DISABLE_COPY_MOVE(ZzColorPicker)
    Q_PROPERTY(
        QColor currentColor
        READ currentColor
        WRITE setCurrentColor
        NOTIFY currentColorChanged)
    Q_PROPERTY(
        bool alphaEnabled
        READ isAlphaEnabled
        WRITE setAlphaEnabled
        NOTIFY alphaEnabledChanged)
    Q_PROPERTY(
        int paletteColorCount
        READ paletteColorCount
        NOTIFY paletteColorsChanged)

public:
    /** @brief 选择兼容紧凑装配或完整 Fluent 三页装配。 */
    enum ZzColorPickerAppearance { Compact, Fluent };
    Q_ENUM(ZzColorPickerAppearance)
    /** @brief 选择通道编辑器的 RGB 或 HSV 表示。 */
    enum ZzColorRepresentation { Rgba, Hsva };
    Q_ENUM(ZzColorRepresentation)
    /** @brief 选择 Hue×Saturation 方形或圆形色谱。 */
    enum ZzColorSpectrumShape { Box, Ring };
    Q_ENUM(ZzColorSpectrumShape)
    /** @brief 返回 appearance 展示状态。 */
    [[nodiscard]] ZzColorPickerAppearance appearance() const noexcept;
    /** @brief 更新 appearance，重复值不发信号。 */
    void setAppearance(ZzColorPickerAppearance value);
    /** @brief 返回 colorRepresentation 展示状态。 */
    [[nodiscard]] ZzColorRepresentation colorRepresentation() const noexcept;
    /** @brief 更新 colorRepresentation，重复值不发信号。 */
    void setColorRepresentation(ZzColorRepresentation value);
    /** @brief 返回 colorSpectrumShape 展示状态。 */
    [[nodiscard]] ZzColorSpectrumShape colorSpectrumShape() const noexcept;
    /** @brief 更新 colorSpectrumShape，重复值不发信号。 */
    void setColorSpectrumShape(ZzColorSpectrumShape value);
    /** @brief 返回 colorSpectrumVisible 展示状态。 */
    [[nodiscard]] bool isColorSpectrumVisible() const noexcept;
    /** @brief 更新 colorSpectrumVisible，重复值不发信号。 */
    void setColorSpectrumVisible(bool value);
    /** @brief 返回 colorPaletteVisible 展示状态。 */
    [[nodiscard]] bool isColorPaletteVisible() const noexcept;
    /** @brief 更新 colorPaletteVisible，重复值不发信号。 */
    void setColorPaletteVisible(bool value);
    /** @brief 返回 colorPreviewVisible 展示状态。 */
    [[nodiscard]] bool isColorPreviewVisible() const noexcept;
    /** @brief 更新 colorPreviewVisible，重复值不发信号。 */
    void setColorPreviewVisible(bool value);
    /** @brief 返回 alphaSliderVisible 展示状态。 */
    [[nodiscard]] bool isAlphaSliderVisible() const noexcept;
    /** @brief 更新 alphaSliderVisible，重复值不发信号。 */
    void setAlphaSliderVisible(bool value);
    /** @brief 返回 colorSliderVisible 展示状态。 */
    [[nodiscard]] bool isColorSliderVisible() const noexcept;
    /** @brief 更新 colorSliderVisible，重复值不发信号。 */
    void setColorSliderVisible(bool value);
    /** @brief 返回 colorChannelTextInputVisible 展示状态。 */
    [[nodiscard]] bool isColorChannelTextInputVisible() const noexcept;
    /** @brief 更新 colorChannelTextInputVisible，重复值不发信号。 */
    void setColorChannelTextInputVisible(bool value);
    /**
     * @brief 创建带固定默认色板和 RGB 编辑器的颜色选择器。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzColorPicker(QWidget *parent = nullptr);

    /** @brief 销毁固定 model、view、delegate 和编辑器装配。 */
    ~ZzColorPicker() override;

    /**
     * @brief 返回唯一当前颜色。
     * @return 规范化到 8 位 RGBA 的有效颜色。
     */
    [[nodiscard]] QColor currentColor() const noexcept;

    /**
     * @brief 设置当前颜色，无效值被拒绝，重复值不发信号。
     * @param color 新颜色。
     */
    void setCurrentColor(QColor color);

    /**
     * @brief 提交待编辑 HEX，并同步发送已排队的颜色通知。
     *
     * 用于对话框在清除编辑器焦点返回后读取最终颜色；不运行事件循环。
     * Qt 失焦调用栈内的通知继续延迟到安全时机。信号回调可同步删除本控件，
     * 调用者需要 QPointer 保护后续访问。
     */
    void commitPendingEdits();

    /**
     * @brief 返回是否显示 alpha 数值和 ARGB 十六进制编辑。
     * @return 启用 alpha 编辑时返回 true。
     */
    [[nodiscard]] bool isAlphaEnabled() const noexcept;

    /**
     * @brief 切换 alpha 编辑器，不改变当前颜色的 alpha 值。
     * @param enabled 是否启用 alpha 编辑。
     */
    void setAlphaEnabled(bool enabled);

    /**
     * @brief 返回色板模型的颜色快照。
     * @return 最多 256 个有效且 RGBA 唯一的颜色。
     */
    [[nodiscard]] QList<QColor> paletteColors() const;

    /**
     * @brief 一次性替换色板，过滤无效和重复颜色并限制为 256 项。
     * @param colors 新色板，允许为空。
     */
    void setPaletteColors(QList<QColor> colors);

    /**
     * @brief 返回当前色板项数。
     * @return 0 到 256。
     */
    [[nodiscard]] int paletteColorCount() const noexcept;

    /** @brief 恢复固定、跨主题一致的默认内容色板。 */
    void resetPaletteColors();

Q_SIGNALS:
    /** @brief appearance 实际变化后发出。 */
    void appearanceChanged(ZzColorPickerAppearance value);
    /** @brief colorRepresentation 实际变化后发出。 */
    void colorRepresentationChanged(ZzColorRepresentation value);
    /** @brief colorSpectrumShape 实际变化后发出。 */
    void colorSpectrumShapeChanged(ZzColorSpectrumShape value);
    /** @brief colorSpectrumVisible 实际变化后发出。 */
    void colorSpectrumVisibleChanged(bool value);
    /** @brief colorPaletteVisible 实际变化后发出。 */
    void colorPaletteVisibleChanged(bool value);
    /** @brief colorPreviewVisible 实际变化后发出。 */
    void colorPreviewVisibleChanged(bool value);
    /** @brief alphaSliderVisible 实际变化后发出。 */
    void alphaSliderVisibleChanged(bool value);
    /** @brief colorSliderVisible 实际变化后发出。 */
    void colorSliderVisibleChanged(bool value);
    /** @brief colorChannelTextInputVisible 实际变化后发出。 */
    void colorChannelTextInputVisibleChanged(bool value);
    /**
     * @brief 当前颜色实际变化后发出一次。
     * 原生 HEX 失焦期间按顺序排队，Qt 调用栈退出后发出；
     * commitPendingEdits 可在安全时机同步冲刷。
     * @param color 新的规范化 RGBA 颜色。
     */
    void currentColorChanged(const QColor &color);

    /**
     * @brief alpha 编辑可见性实际变化后发出。
     * @param enabled 当前是否启用 alpha 编辑。
     */
    void alphaEnabledChanged(bool enabled);

    /** @brief 色板集合实际变化后发出。 */
    void paletteColorsChanged();

protected:
    /** @brief 在语言、主题、palette 或方向变化后刷新派生展示。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzColorPickerPrivate> d_ptr;
};

} // namespace ZzFluentUI
