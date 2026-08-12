#pragma once

#include <QtGui/QColor>

#include "ZzWidgetTheme.h"

class QLabel;
class QLineEdit;
class QListView;
class QRegularExpressionValidator;

namespace ZzFluentUI {

class ZzColorPaletteModel;
class ZzColorPicker;
class ZzColorPreviewWidget;
class ZzColorSwatchDelegate;
class ZzSpinBox;

/** @brief 管理颜色选择器固定装配和单向派生状态同步。 */
class ZzColorPickerPrivate final
{
public:
    /**
     * @brief 构造唯一 model、view、delegate、预览和编辑器集合。
     * @param q 非空、非拥有的公开颜色选择器。
     */
    explicit ZzColorPickerPrivate(ZzColorPicker *q);

    /** @brief 销毁私有值状态，QObject 子项继续由公开控件拥有。 */
    ~ZzColorPickerPrivate();

    /** @brief 规范化并应用有效当前颜色，返回是否实际变化。 */
    [[nodiscard]] bool applyCurrentColor(QColor color);

    /** @brief 过滤并替换色板模型，返回是否实际变化。 */
    [[nodiscard]] bool applyPaletteColors(QList<QColor> colors);

    /** @brief 返回唯一色板模型的颜色快照。 */
    [[nodiscard]] QList<QColor> paletteColors() const;

    /** @brief 返回唯一色板模型的当前项数。 */
    [[nodiscard]] int paletteColorCount() const noexcept;

    /** @brief 返回固定默认内容色板。 */
    [[nodiscard]] static QList<QColor> defaultPaletteColors();

    /** @brief 同步全部编辑器、预览和色板选择。 */
    void syncDerivedState();

    /** @brief 从 RGB(A) 数值编辑器提交一次当前颜色。 */
    void commitChannelEditors();

    /** @brief 从十六进制编辑器提交或恢复当前颜色。 */
    void commitHexEditor();

    /** @brief 刷新 alpha 编辑器、validator 和派生文本。 */
    void syncAlphaPresentation();

    /** @brief 刷新本地化标签和无障碍名称。 */
    void refreshAccessibleText();

    /** @brief 重建回退主题并刷新 delegate、预览和网格尺寸。 */
    void refreshTheme();

    /** @brief 仅按当前主题刷新色板逻辑尺寸。 */
    void syncPaletteMetrics();

    ZzColorPicker *const q_ptr;
    ZzWidgetTheme theme;
    ZzColorPaletteModel *const paletteModel;
    QListView *const paletteView;
    ZzColorSwatchDelegate *const swatchDelegate;
    ZzColorPreviewWidget *const preview;
    QLabel *const redLabel;
    QLabel *const greenLabel;
    QLabel *const blueLabel;
    QLabel *const alphaLabel;
    QLabel *const hexLabel;
    ZzSpinBox *const redSpinBox;
    ZzSpinBox *const greenSpinBox;
    ZzSpinBox *const blueSpinBox;
    ZzSpinBox *const alphaSpinBox;
    QLineEdit *const hexEditor;
    QRegularExpressionValidator *const hexValidator;
    QColor currentColor{QColor::fromRgb(0, 120, 212)};
    bool alphaEnabled = false;
    bool syncing = false;
};

} // namespace ZzFluentUI
