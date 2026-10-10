#pragma once

#include <QtGui/QColor>
#include <ZzFluentUI/ZzColorPicker.h>

#include "ZzWidgetTheme.h"

class QLabel;
class QLineEdit;
class QListView;
class QRegularExpressionValidator;
class QVBoxLayout;
class QGridLayout;
class QTabBar;
class QStackedWidget;
class QComboBox;

namespace ZzFluentUI {

class ZzColorPaletteModel;
class ZzColorPicker;
class ZzColorPreviewWidget;
class ZzColorSwatchDelegate;
class ZzSpinBox;
class ZzColorSpectrum;
class ZzColorGradientSlider;
class ZzColorShadeStrip;

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

    /** @brief 只提交实际编辑的 RGB/HSV/alpha 分量，保留其余分量精度。 */
    void commitChannelEditor(ZzSpinBox *editor);

    /** @brief 从十六进制编辑器提交或恢复当前颜色。 */
    void commitHexEditor();

    /** @brief 在 HEX 失焦调用栈前开始一批有序颜色通知。 */
    void deferColorNotifications();
    /** @brief 立即发送颜色快照，或在失焦期间依次排队。 */
    void notifyCurrentColorChanged();
    /** @brief 在 Qt 失焦调用栈外安全发送已排队快照，不运行事件循环。 */
    void flushColorNotifications();

    /** @brief 刷新 alpha 编辑器、validator 和派生文本。 */
    void syncAlphaPresentation();

    /** @brief 刷新本地化标签和无障碍名称。 */
    void refreshAccessibleText();

    /** @brief 重建回退主题并刷新 delegate、预览和网格尺寸。 */
    void refreshTheme();

    /** @brief 仅按当前主题刷新色板逻辑尺寸。 */
    void syncPaletteMetrics();

    /** @brief 创建一次 Fluent 页面和私有绘制部件。 */
    void buildFluentPresentation();
    /** @brief 在固定装配之间移动唯一编辑器和色板。 */
    void syncAppearance();
    /** @brief 刷新可见页并回退到首个可见页面。 */
    void syncVisibility();
    /** @brief 刷新渐变、HSV 派生状态和本地化通道。 */
    void syncFluentState();
    /** @brief 提交 HSV 编辑并保留退化颜色的 hue/saturation。 */
    void commitHsv(qreal hue, qreal saturation, qreal value);

    ZzColorPicker *const q_ptr;
    ZzWidgetTheme theme;
    ZzColorPaletteModel *const paletteModel;
    QListView *const paletteView;
    ZzColorSwatchDelegate *const swatchDelegate;
    QWidget *const preview;
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
    bool syncingPaletteMetrics = false;
    bool notificationsDeferred = false;
    bool hexFocusOutActive = false;
    QList<QColor> pendingColorNotifications;
    ZzColorPicker::ZzColorPickerAppearance appearance = ZzColorPicker::Compact;
    ZzColorPicker::ZzColorRepresentation representation = ZzColorPicker::Rgba;
    ZzColorPicker::ZzColorSpectrumShape shape = ZzColorPicker::Box;
    bool spectrumVisible = true;
    bool paletteVisible = true;
    bool previewVisible = true;
    bool alphaSliderVisible = true;
    bool sliderVisible = true;
    bool channelTextInputVisible = true;
    qreal hue = currentColor.hsvHueF();
    qreal saturation = currentColor.hsvSaturationF();
    qreal value = currentColor.valueF();
    QWidget *compactHost = nullptr;
    QWidget *fluentHost = nullptr;
    QWidget *editorHost = nullptr;
    QVBoxLayout *compactLayout = nullptr;
    QGridLayout *editorLayout = nullptr;
    QVBoxLayout *fluentLayout = nullptr;
    QTabBar *tabs = nullptr;
    QStackedWidget *pages = nullptr;
    QWidget *spectrumPage = nullptr;
    QWidget *palettePage = nullptr;
    QWidget *slidersPage = nullptr;
    QComboBox *representationCombo = nullptr;
    ZzColorSpectrum *spectrum = nullptr;
    ZzColorGradientSlider *valueSlider = nullptr;
    ZzColorGradientSlider *alphaSlider = nullptr;
    ZzColorGradientSlider *channelSliders[4]{};
    ZzColorShadeStrip *shadeStrip = nullptr;
};

} // namespace ZzFluentUI
