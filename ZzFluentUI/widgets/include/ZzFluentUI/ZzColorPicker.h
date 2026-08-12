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
    /**
     * @brief 当前颜色实际变化后发出一次。
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
