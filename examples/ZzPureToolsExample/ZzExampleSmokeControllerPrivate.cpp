#include "ZzExampleSmokeControllerPrivate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QRect>
#include <QtCore/QSignalBlocker>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QFontInfo>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QToolButton>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzRangeSlider.h>
#include <ZzFluentUI/ZzAudioLevelMeter.h>
#include <ZzFluentUI/ZzRadialGauge.h>
#include <ZzFluentUI/ZzLiquidGauge.h>
#include <ZzFluentUI/ZzMultiRadialGauge.h>
#include <ZzFluentUI/ZzMultiProgressRing.h>
#include <ZzFluentUI/ZzBorderBeam.h>
#include <ZzFluentUI/ZzBorderBeamButton.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include "ZzExampleTimelineSmoke.h"
#include "ZzExampleInfoBarSmoke.h"
#include "ZzExampleCarouselSmoke.h"
#include "ZzExampleColorPickerSmoke.h"
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzInfoBadge.h>

#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzToggleSwitch.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include "ZzExampleActivityModel.h"
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

constexpr int zzScreenshotLogicalWidth = 1280;
constexpr int zzScreenshotLogicalHeight = 800;
constexpr int zzScreenshotTextPadding = 3;
constexpr int zzScreenshotChannelTolerance = 3;

/** @brief 验证环形进度条的真实属性连接与中心控件交互。 */
[[nodiscard]] bool zzProgressRingPageReady(const QWidget &window, ZzFluentUI::ZzThemeController *theme)
{
    using Ring = ZzFluentUI::ZzProgressRing;
    auto *page = window.findChild<QWidget *>(QStringLiteral("zzExampleProgressRingPage"));
    if (!page) return false;
    auto *ring = page->findChild<Ring *>(QStringLiteral("zzProgressRingPreview"));
    auto *value = page->findChild<QSlider *>(QStringLiteral("zzProgressRingValue"));
    auto *title = page->findChild<QLineEdit *>(QStringLiteral("zzProgressRingTitle"));
    auto *format = page->findChild<QLineEdit *>(QStringLiteral("zzProgressRingFormat"));
    auto *duration = page->findChild<QSpinBox *>(QStringLiteral("zzProgressRingDuration"));
    auto *thickness = page->findChild<QDoubleSpinBox *>(QStringLiteral("zzProgressRingThickness"));
    auto *titleSize = page->findChild<QSpinBox *>(QStringLiteral("zzProgressRingTitleSize"));
    auto *valueSize = page->findChild<QSpinBox *>(QStringLiteral("zzProgressRingValueSize"));
    auto *spacing = page->findChild<QSpinBox *>(QStringLiteral("zzProgressRingSpacing"));
    auto *busy = page->findChild<QCheckBox *>(QStringLiteral("zzProgressRingBusy"));
    auto *custom = page->findChild<QCheckBox *>(QStringLiteral("zzProgressRingCustom"));
    auto *visible = page->findChild<QCheckBox *>(QStringLiteral("zzProgressRingTextVisible"));
    auto *disabled = page->findChild<QCheckBox *>(QStringLiteral("zzProgressRingDisabled"));
    auto *automatic = page->findChild<QCheckBox *>(QStringLiteral("zzProgressRingAuto_ringColor"));
    auto *color = page->findChild<QWidget *>(QStringLiteral("zzProgressRingColor_ringColor"));
    auto *reset = page->findChild<QPushButton *>(QStringLiteral("zzProgressRingReset"));
    if (!ring || !value || !title || !format || !duration || !thickness || !titleSize || !valueSize
        || !spacing || !busy || !custom || !visible || !disabled || !automatic || !color || !reset) return false;
    bool ready = page->findChildren<Ring *>().size() == 7;
    value->setValue(42);
    title->setText(QStringLiteral("Edited"));
    format->setText(QStringLiteral("%v / %m"));
    duration->setValue(1600);
    thickness->setValue(8.5);
    titleSize->setValue(15);
    valueSize->setValue(26);
    spacing->setValue(8);
    ready = ready && ring->value() == 42 && ring->title() == QStringLiteral("Edited")
        && ring->text() == QStringLiteral("42 / 100") && ring->indeterminateDuration() == 1600
        && qFuzzyCompare(ring->thickness(), 8.5) && ring->titleFont().pixelSize() == 15
        && ring->valueFont().pixelSize() == 26 && ring->textSpacing() == 8;
    automatic->setChecked(false);
    ready = ready && ring->ringColor() == color->property("selectedColor").value<QColor>();
    color->setProperty("selectedColor", QColor(Qt::red));
    const auto mode = theme->mode();
    theme->setMode(ZzFluentUI::ZzThemeMode::Dark);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
    ready = ready && ring->ringColor() == QColor(Qt::red)
        && color->property("selectedColor").value<QColor>() == QColor(Qt::red);
    theme->setMode(mode);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
    busy->setChecked(true);
    ready = ready && ring->minimum() == 0 && ring->maximum() == 0 && !value->isEnabled();
    custom->setChecked(true);
    QPointer<QWidget> center = ring->centerWidget();
    auto *restart = ring->findChild<QPushButton *>(QStringLiteral("zzProgressRingRestart"));
    if (!center || !restart) return false;
    restart->click();
    ready = ready && ring->value() == 0 && ring->maximum() == 100 && value->isEnabled();
    visible->setChecked(false);
    static_cast<void>(ring->grab());
    ready = ready && center->isHidden();
    disabled->setChecked(true);
    reset->click();
    ready = ready && center.isNull() && !ring->centerWidget() && !custom->isChecked()
        && ring->value() == 65 && ring->isEnabled() && ring->isTextVisible()
        && ring->title() == QCoreApplication::translate("ZzPureToolsExample", "已完成")
        && ring->format() == QStringLiteral("%p%") && qFuzzyCompare(ring->thickness(), 6.0)
        && ring->indeterminateDuration() == 800 && ring->titleFont().pixelSize() == 12
        && ring->valueFont().pixelSize() == 24 && ring->textSpacing() == 4
        && !ring->ringColor().isValid() && automatic->isChecked() && !color->isEnabled();
    return ready;
}

/** @brief 验证水波页的联动、主题色选择与恢复默认。 */
[[nodiscard]] bool zzLiquidGaugePageReady(const QWidget &window)
{
    using Gauge = ZzFluentUI::ZzLiquidGauge;
    auto *page = window.findChild<QWidget *>(QStringLiteral("zzExampleLiquidGaugePage"));
    if (!page) return false;
    auto *gauge = page->findChild<Gauge *>(QStringLiteral("zzLiquidPreview"));
    auto *shared = page->findChild<QSlider *>(QStringLiteral("zzLiquidSharedValue"));
    auto *value = page->findChild<QSlider *>(QStringLiteral("zzLiquidValue"));
    auto *amplitude = page->findChild<QSlider *>(QStringLiteral("zzLiquidAmplitude"));
    auto *shape = page->findChild<QComboBox *>(QStringLiteral("zzLiquidShape"));
    auto *format = page->findChild<QLineEdit *>(QStringLiteral("zzLiquidFormat"));
    auto *animation = page->findChild<QCheckBox *>(QStringLiteral("zzLiquidAnimation"));
    auto *automatic = page->findChild<QCheckBox *>(QStringLiteral("zzLiquidAuto_waveColor"));
    auto *color = page->findChild<QWidget *>(QStringLiteral("zzLiquidColor_waveColor"));
    auto *outline = page->findChild<QWidget *>(QStringLiteral("zzLiquidColor_outlineColor"));
    auto *reset = page->findChild<QPushButton *>(QStringLiteral("zzLiquidReset"));
    auto *tabs = page->findChild<QTabWidget *>(QStringLiteral("zzLiquidPropertyTabs"));
    if (!gauge || !shared || !value || !amplitude || !shape || !format || !animation
        || !automatic || !color || !outline || !reset || !tabs || tabs->count() != 2) return false;
    shared->setValue(37);
    int matchingSamples = 0;
    for (auto *sample : page->findChildren<Gauge *>())
        matchingSamples += sample != gauge && sample->value() == 37;
    bool ready = matchingSamples == 4;
    shared->setValue(60);
    value->setValue(42);
    shape->setCurrentIndex(shape->findData(Gauge::TriangleShape));
    format->setText(QStringLiteral("%v / %m"));
    amplitude->setValue(17);
    animation->setChecked(false);
    ready = ready && gauge->value() == 42 && gauge->shape() == Gauge::TriangleShape
        && gauge->text() == QStringLiteral("42 / 100") && qFuzzyCompare(gauge->waveAmplitude(), 8.5)
        && !gauge->isAnimationEnabled() && !gauge->isRunning();
    const QColor swatch = color->property("selectedColor").value<QColor>();
    automatic->setChecked(false);
    ready = ready && color->isEnabled() && gauge->waveColor() == swatch;
    color->setProperty("selectedColor", QColor(Qt::red));
    ready = ready && gauge->waveColor() == QColor(Qt::red)
        && outline->property("selectedColor").value<QColor>() == QColor(Qt::red);
    reset->click();
    ready = ready && gauge->value() == 68 && gauge->shape() == Gauge::CircleShape
        && gauge->format() == QStringLiteral("%p%") && qFuzzyCompare(gauge->waveAmplitude(), 6.0)
        && gauge->isAnimationEnabled() && !gauge->waveColor().isValid()
        && automatic->isChecked() && !color->isEnabled()
        && color->property("selectedColor").value<QColor>() == gauge->palette().color(QPalette::Active, QPalette::Accent);
    return ready;
}

/** @brief 验证仪表页的真实属性连接、公共联动与重置。 */
[[nodiscard]] bool zzRadialGaugePageReady(const QWidget &window, ZzFluentUI::ZzThemeController *theme)
{
    using namespace ZzFluentUI;
    auto *page = window.findChild<QWidget *>(QStringLiteral("zzExampleRadialGaugePage"));
    if (!page) return false;
    auto *gauge = page->findChild<ZzRadialGauge *>(QStringLiteral("radialGaugePreview"));
    auto *ring = page->findChild<ZzMultiProgressRing *>(QStringLiteral("multiProgressRingPreview"));
    auto *multi = page->findChild<ZzMultiRadialGauge *>(QStringLiteral("multiRadialGaugePreview"));
    auto *tabs = page->findChild<QTabWidget *>(QStringLiteral("zzRadialPropertyTabs"));
    auto *shared = page->findChild<QSlider *>(QStringLiteral("zzRadialSharedValue"));
    auto *value = page->findChild<QSlider *>(QStringLiteral("zzRadialEditor_valueSlider"));
    auto *width = page->findChild<QSlider *>(QStringLiteral("zzRadialEditor_needleWidthSlider"));
    auto *reset = page->findChild<QPushButton *>(QStringLiteral("zzRadialEditor_resetButton"));
    auto *ringValue = page->findChild<QSlider *>(QStringLiteral("zzRadialEditor_perfectValueSlider"));
    auto *ringLabel = page->findChild<QLineEdit *>(QStringLiteral("zzRadialEditor_perfectLabelEdit"));
    auto *ringTrackColor = page->findChild<QObject *>(QStringLiteral("zzRadialEditor_ringTrackColorButton"));
    auto *overlap = page->findChild<QCheckBox *>(QStringLiteral("zzRadialEditor_multiGaugeProgressOverlapCheck"));
    auto *autoDetailColor = page->findChild<QCheckBox *>(QStringLiteral("zzRadialEditor_autoDetailTextColorCheck"));
    auto *detailColor = page->findChild<QWidget *>(QStringLiteral("zzRadialEditor_multiGaugeDetailTextColorButton"));
    if (!gauge || !ring || !multi || !tabs || tabs->count() != 3 || !shared
        || !value || !width || !reset || !ringValue || !ringLabel || !overlap || !ringTrackColor
        || !autoDetailColor || !detailColor
        || ring->items().size() != 3 || multi->items().size() != 3) return false;
    const bool reducedMotion = theme->reducedMotion();
    theme->setReducedMotion(true);
    shared->setValue(37);
    bool ready = true;
    int matchingSamples = 0;
    for (auto *sample : page->findChildren<ZzRadialGauge *>())
        matchingSamples += sample != gauge && sample->value() == 37;
    ready = ready && matchingSamples == 3;
    shared->setValue(70);
    value->setValue(123);
    width->setValue(17);
    ready = ready && gauge->value() == 123 && qFuzzyCompare(gauge->needleWidth(), 8.5);
    reset->click();
    ready = ready && gauge->value() == 210 && qFuzzyCompare(gauge->needleWidth(), 5.0)
        && gauge->valueAnimationDuration() == 500 && qFuzzyCompare(gauge->hubRadius(), 11.0);
    tabs->setCurrentIndex(1);
    const QString label = ringLabel->text();
    ringValue->setValue(73);
    ringLabel->setText(QStringLiteral("Edited"));
    ready = ready && qFuzzyCompare(ring->items().first()->value(), 73.0)
        && ring->items().first()->label() == QStringLiteral("Edited");
    ringValue->setValue(20);
    ringLabel->setText(label);
    tabs->setCurrentIndex(2);
    const bool oldOverlap = overlap->isChecked();
    overlap->setChecked(!oldOverlap);
    ready = ready && multi->isProgressOverlap() == !oldOverlap;
    overlap->setChecked(oldOverlap);
    const bool originalAutoDetailColor = autoDetailColor->isChecked();
    const QColor originalDetailColor = multi->detailTextColor();
    const QVariant originalDetailSelection = detailColor->property("selectedColor");
    ready = ready && originalAutoDetailColor && !originalDetailColor.isValid() && !detailColor->isEnabled();
    detailColor->setProperty("selectedColor", QColor(Qt::white));
    autoDetailColor->setChecked(false);
    ready = ready && detailColor->isEnabled() && multi->detailTextColor() == QColor(Qt::white);
    autoDetailColor->setChecked(true);
    ready = ready && !detailColor->isEnabled() && !multi->detailTextColor().isValid();
    {
        const QSignalBlocker blocker(detailColor);
        detailColor->setProperty("selectedColor", originalDetailSelection);
    }
    autoDetailColor->setChecked(originalAutoDetailColor);
    multi->setDetailTextColor(originalDetailColor);
    tabs->setCurrentIndex(0);
    const auto originalMode = theme->mode();
    const QColor originalTrackColor = ring->trackColor();
    const QVariant originalTrackSelection = ringTrackColor->property("selectedColor");
    ringTrackColor->setProperty("selectedColor", QColor(Qt::red));
    ready = ready && ring->trackColor() == QColor(Qt::red);
    tabs->setCurrentIndex(1);
    auto *classic = page->findChild<ZzRadialGauge *>(QStringLiteral("zzRadialSample0"));
    auto *speed = page->findChild<ZzRadialGauge *>(QStringLiteral("zzRadialSample2"));
    for (const auto mode : {ZzThemeMode::Dark, ZzThemeMode::Light}) {
        theme->setMode(mode);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
        QEvent trackPaletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(ringTrackColor, &trackPaletteChange);
        QColor expected = classic ? classic->palette().color(QPalette::Text) : QColor();
        expected.setAlpha(220);
        ready = ready && classic && speed && classic->palette().color(QPalette::Mid) == expected
            && speed->tickColor() == expected
            && (classic->palette().color(QPalette::Window).lightness() < 128) == (mode == ZzThemeMode::Dark);
        ready = ready && ring->trackColor() == QColor(Qt::red)
            && ringTrackColor->property("selectedColor").value<QColor>() == QColor(Qt::red);
    }
    theme->setMode(originalMode);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
    {
        const QSignalBlocker blocker(ringTrackColor);
        ringTrackColor->setProperty("selectedColor", originalTrackSelection);
    }
    ring->setTrackColor(originalTrackColor);
    tabs->setCurrentIndex(0);
    theme->setReducedMotion(reducedMotion);
    return ready;
}


/** @brief 验证真实音频页编辑器连接，并恢复初始展示。 */
[[nodiscard]] bool zzAudioLevelMeterPageReady(const QWidget &window, ZzFluentUI::ZzThemeController *theme)
{
    using Meter = ZzFluentUI::ZzAudioLevelMeter;
    auto *meter = window.findChild<Meter *>(QStringLiteral("zzAudioPreview"));
    auto *channels = window.findChild<QSpinBox *>(QStringLiteral("zzAudioChannels"));
    auto *segments = window.findChild<QSpinBox *>(QStringLiteral("zzAudioSegments"));
    auto *position = window.findChild<QComboBox *>(QStringLiteral("zzAudioScalePosition"));
    auto *mode = window.findChild<QComboBox *>(QStringLiteral("zzAudioScaleMode"));
    auto *ticks = window.findChild<QSpinBox *>(QStringLiteral("zzAudioTicks"));
    auto *colors = window.findChild<QComboBox *>(QStringLiteral("zzAudioColorMode"));
    auto *simulation = window.findChild<QPushButton *>(QStringLiteral("zzAudioSimulation"));
    auto *source = window.findChild<QObject *>(QStringLiteral("zzAudioMeterSource"));
    auto *color = window.findChild<QObject *>(QStringLiteral("zzAudioColor_activeColor"));
    auto *warning = window.findChild<QDoubleSpinBox *>(QStringLiteral("zzAudioWarning"));
    auto *clip = window.findChild<QDoubleSpinBox *>(QStringLiteral("zzAudioClip"));
    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("zzAudioPropertyTabs"));
    if (!meter || !channels || !segments || !position || !mode || !ticks || !colors
        || !simulation || !source || !color || !warning || !clip || !tabs || tabs->count() != 4
        || (tabs->cornerWidget() && !tabs->cornerWidget()->isHidden())) return false;
    warning->setValue(-3);
    warning->setValue(-2);
    clip->setValue(0);
    clip->setValue(1);
    const bool clamped = warning->value() == meter->warningDecibels()
        && clip->value() == meter->clipDecibels();
    warning->setValue(-12);
    clip->setValue(-3);
    const QColor initialColor = meter->activeColor();
    channels->setValue(4);
    segments->setValue(48);
    position->setCurrentIndex(int(Meter::LeftScale));
    mode->setCurrentIndex(int(Meter::FixedTickCount));
    ticks->setValue(9);
    colors->setCurrentIndex(int(Meter::GradientColors));
    color->setProperty("selectedColor", QColor("#6633cc"));
    const auto initialMode = theme->mode();
    theme->setMode(ZzFluentUI::ZzThemeMode::Dark);
    const bool darkOverride = meter->activeColor() == QColor("#6633cc");
    theme->setMode(ZzFluentUI::ZzThemeMode::Light);
    const bool lightOverride = meter->activeColor() == QColor("#6633cc");
    theme->setMode(initialMode);
    simulation->setChecked(false);
    const bool valid = meter->channelCount() == 4 && meter->segmentCount() == 48
        && meter->scalePosition() == Meter::LeftScale && meter->scaleTickCount() == 9
        && meter->scaleMode() == Meter::FixedTickCount && ticks->isEnabled()
        && meter->colorMode() == Meter::GradientColors && meter->activeColor() == QColor("#6633cc")
        && !source->property("simulationEnabled").toBool();
    channels->setValue(2);
    segments->setValue(30);
    position->setCurrentIndex(int(Meter::CenterScale));
    mode->setCurrentIndex(int(Meter::IntervalScale));
    ticks->setValue(7);
    colors->setCurrentIndex(int(Meter::SingleColor));
    color->setProperty("selectedColor", initialColor);
    simulation->setChecked(true);
    return valid && clamped && darkOverride && lightOverride && source->property("simulationEnabled").toBool();
}

/** @brief 通过真实编辑控件检查光束预览、主题覆盖及重置行为。 */
[[nodiscard]] bool zzBorderBeamPageReady(const QWidget &window)
{
    using Beam = ZzFluentUI::ZzBorderBeam;
    auto *beam = window.findChild<Beam *>(QStringLiteral("zzBorderBeamPreview"));
    auto *reset = window.findChild<QPushButton *>(QStringLiteral("zzBorderBeamReset"));
    auto *length = window.findChild<QSlider *>(QStringLiteral("zzBeam_lengthSlider"));
    auto *width = window.findChild<QSlider *>(QStringLiteral("zzBeam_widthSlider"));
    auto *radius = window.findChild<QSlider *>(QStringLiteral("zzBeam_radiusSlider"));
    auto *duration = window.findChild<QSlider *>(QStringLiteral("zzBeam_durationSlider"));
    auto *progress = window.findChild<QSlider *>(QStringLiteral("zzBeam_progressSlider"));
    auto *count = window.findChild<QSpinBox *>(QStringLiteral("zzBeam_countSpinBox"));
    auto *direction = window.findChild<QComboBox *>(QStringLiteral("zzBeam_directionCombo"));
    auto *theme = window.findChild<QComboBox *>(QStringLiteral("zzBeam_themeCombo"));
    auto *animation = window.findChild<QCheckBox *>(QStringLiteral("zzBeam_animationCheck"));
    if (!beam || !reset || !length || !width || !radius || !duration || !progress
        || !count || !direction || !theme || !animation
        || !window.findChild<ZzFluentUI::ZzBorderBeamButton *>()) return false;
    length->setValue(120);
    width->setValue(7);
    radius->setValue(24);
    duration->setValue(3000);
    progress->setValue(40);
    count->setValue(3);
    direction->setCurrentIndex(1);
    theme->setCurrentIndex(2);
    animation->setChecked(false);
    if (beam->beamLength() != 120 || beam->beamWidth() != 3.5 || beam->cornerRadius() != 24
        || beam->animationDuration() != 3000 || beam->initialProgress() != 0.4 || beam->beamCount() != 3
        || beam->direction() != Beam::CounterClockwise || beam->themeMode() != Beam::DarkTheme
        || beam->isAnimationEnabled() || beam->isRunning() || beam->startColor().isValid()) return false;
    beam->setStartColor(Qt::red);
    reset->click();
    const bool valid = beam->beamLength() == 60 && beam->beamWidth() == 2
        && beam->cornerRadius() == 8 && beam->animationDuration() == 6000
        && beam->initialProgress() == 0 && beam->beamCount() == 1
        && beam->direction() == Beam::Clockwise && beam->themeMode() == Beam::AutoTheme
        && beam->isAnimationEnabled() && !beam->startColor().isValid();
    // 恢复首次展示参数，便于人工截图检查。
    length->setValue(90);
    width->setValue(5);
    radius->setValue(16);
    duration->setValue(5000);
    return valid;
}

/** @brief 验证范围滑块页面属性与预览的真实连接，并恢复演示默认状态。 */
[[nodiscard]] bool zzRangeSliderPageReady(const QWidget &window)
{
    auto *slider = window.findChild<ZzFluentUI::ZzRangeSlider *>(QStringLiteral("rangeSelector"));
    auto *ticks = window.findChild<QCheckBox *>(QStringLiteral("zzRangeTicks"));
    auto *interval = window.findChild<QSpinBox *>(QStringLiteral("zzRangeTickInterval"));
    auto *tracking = window.findChild<QCheckBox *>(QStringLiteral("zzRangeTracking"));
    auto *vertical = window.findChild<QCheckBox *>(QStringLiteral("zzRangeVertical"));
    auto *snap = window.findChild<QComboBox *>(QStringLiteral("zzRangeSnapMode"));
    auto *label = window.findChild<QLabel *>(QStringLiteral("zzExampleRangeSliderValues"));
    if (!slider || !ticks || !interval || !tracking || !vertical || !snap || !label)
        return false;
    if (slider->lowerValue() != 20 || slider->upperValue() != 80)
        return false;
    ticks->click();
    tracking->click();
    vertical->click();
    snap->setCurrentIndex(2);
    slider->setValues(30, 70);
    const bool valid = !slider->hasTickPosition() && !interval->isEnabled()
        && !slider->hasTracking() && slider->orientation() == Qt::Vertical
        && slider->snapMode() == ZzFluentUI::ZzRangeSlider::ZzSnapMode::SnapOnRelease
        && label->text().contains(QStringLiteral("30"))
        && label->text().contains(QStringLiteral("70"));
    ticks->click();
    tracking->click();
    vertical->click();
    snap->setCurrentIndex(0);
    slider->setValues(20, 80);
    return valid;
}

/** @brief 验证图标路由已通过公开 API 生成完整 SVG 与字体图标集合。 */
[[nodiscard]] bool zzIconPageReady(
    const ZzPureTools::ZzApplicationWindow &window)
{
    int svgCount = 0;
    int fontCount = 0;
    const auto buttons =
        window.findChildren<ZzFluentUI::ZzIconButton *>();
    for (const ZzFluentUI::ZzIconButton *button : buttons) {
        const QString &name = button->objectName();
        if (name.startsWith(QStringLiteral("zzExampleSvgIcon_"))) {
            ++svgCount;
            if (button->icon().isNull()
                || !button->iconColor().isValid()) {
                return false;
            }
        } else if (name.startsWith(
                       QStringLiteral("zzExampleFontIcon_"))) {
            ++fontCount;
            if (button->icon().isNull()) {
                return false;
            }
        }
    }
    return svgCount == 11 && fontCount == 12;
}

/** @brief 保存一次综合示例截图比较的统计和差异图。 */
struct ZzExampleScreenshotComparison final
{
    qsizetype comparedPixels = 0;
    qsizetype differentPixels = 0;
    QImage difference;
};

/** @brief 将路由表中的 UTF-8 常量转换为 Qt 字符串。 */
[[nodiscard]] QString zzFromUtf8(std::string_view text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

/** @brief 返回关闭场景对应的 QMessageBox 按钮角色。 */
[[nodiscard]] QMessageBox::ButtonRole zzCloseButtonRole(
    ZzExampleSmokeScenario scenario)
{
    switch (scenario) {
    case ZzExampleSmokeScenario::CloseCancel:
        return QMessageBox::RejectRole;
    case ZzExampleSmokeScenario::CloseMinimize:
        return QMessageBox::ActionRole;
    case ZzExampleSmokeScenario::CloseConfirm:
        return QMessageBox::AcceptRole;
    default:
        return QMessageBox::InvalidRole;
    }
}

/** @brief 返回当前 Qt minor 对应的综合示例非文字差异上限。 */
[[nodiscard]] constexpr qreal zzScreenshotMaximumDifferenceRatio()
{
#if QT_VERSION_MAJOR == 6 && QT_VERSION_MINOR == 11
    return 0.005;
#else
    return 0.02;
#endif
}

/** @brief 把子控件逻辑矩形映射到综合示例窗口。 */
[[nodiscard]] QRect zzMapToWindow(
    const QWidget *widget,
    const QRect &rect,
    const QWidget *window)
{
    return QRect(widget->mapTo(window, rect.topLeft()), rect.size());
}

/** @brief 将外扩后的逻辑文字矩形加入物理像素遮罩。 */
void zzPaintTextMask(
    QPainter *painter,
    const QWidget *widget,
    QRect rect,
    const QWidget *window)
{
    if (rect.isEmpty()) {
        return;
    }
    painter->fillRect(
        zzMapToWindow(widget, rect, window).adjusted(
            -zzScreenshotTextPadding,
            -zzScreenshotTextPadding,
            zzScreenshotTextPadding,
            zzScreenshotTextPadding),
        Qt::white);
}

/** @brief 为单行或自动换行文字计算实际字体边界。 */
[[nodiscard]] QRect zzTextBounds(
    const QWidget *widget,
    const QRect &bounds,
    int alignment,
    const QString &text)
{
    if (bounds.isEmpty() || text.isEmpty()) {
        return {};
    }
    return widget->fontMetrics().boundingRect(
        bounds,
        alignment,
        text);
}

/** @brief 递归遮罩 item view 中当前可见索引的展示文字。 */
void zzMaskVisibleIndexes(
    QPainter *painter,
    QAbstractItemView *view,
    const QModelIndex &parent,
    const QWidget *window)
{
    QAbstractItemModel *model = view->model();
    if (model == nullptr) {
        return;
    }
    for (int row = 0; row < model->rowCount(parent); ++row) {
        for (int column = 0; column < model->columnCount(parent); ++column) {
            const QModelIndex index = model->index(row, column, parent);
            const QRect itemRect = view->visualRect(index);
            const QString text = index.data(Qt::DisplayRole).toString();
            if (!text.isEmpty() && itemRect.isValid()
                && itemRect.intersects(view->viewport()->rect())) {
                const QRect textRect = zzTextBounds(
                    view->viewport(),
                    itemRect.adjusted(4, 1, -4, -1),
                    Qt::AlignLeading | Qt::AlignVCenter,
                    text).adjusted(-36, 0, 36, 0);
                zzPaintTextMask(
                    painter,
                    view->viewport(),
                    textRect,
                    window);
            }
            if (column == 0 && model->hasChildren(index)) {
                zzMaskVisibleIndexes(painter, view, index, window);
            }
        }
    }
}

/** @brief 为综合窗口内可见 Qt 控件建立字体差异遮罩。 */
[[nodiscard]] QImage zzBuildExampleTextMask(
    ZzPureTools::ZzApplicationWindow &window,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzScreenshotLogicalWidth * dpr),
        qRound(zzScreenshotLogicalHeight * dpr));
    QImage mask(physicalSize, QImage::Format_Grayscale8);
    mask.setDevicePixelRatio(dpr);
    mask.fill(0);
    QPainter painter(&mask);

    const auto widgets = window.findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (!widget->isVisible()) {
            continue;
        }
        if (auto *label = qobject_cast<QLabel *>(widget)) {
            int flags = static_cast<int>(label->alignment());
            if (label->wordWrap()) {
                flags |= Qt::TextWordWrap;
            }
            zzPaintTextMask(
                &painter,
                label,
                zzTextBounds(
                    label,
                    label->contentsRect(),
                    flags,
                    label->text()),
                &window);
            continue;
        }
        if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
            zzPaintTextMask(
                &painter,
                button,
                zzTextBounds(
                    button,
                    button->contentsRect().adjusted(6, 2, -6, -2),
                    Qt::AlignCenter,
                    button->text()),
                &window);
            continue;
        }
        if (auto *editor = qobject_cast<QLineEdit *>(widget)) {
            const QString text = editor->displayText().isEmpty()
                ? editor->placeholderText() : editor->displayText();
            zzPaintTextMask(
                &painter,
                editor,
                zzTextBounds(
                    editor,
                    editor->contentsRect().adjusted(4, 1, -4, -1),
                    static_cast<int>(editor->alignment() | Qt::AlignVCenter),
                    text),
                &window);
            continue;
        }
        if (auto *combo = qobject_cast<QComboBox *>(widget)) {
            zzPaintTextMask(
                &painter,
                combo,
                zzTextBounds(
                    combo,
                    combo->contentsRect().adjusted(8, 1, -28, -1),
                    Qt::AlignLeading | Qt::AlignVCenter,
                    combo->currentText()),
                &window);
            continue;
        }
        if (auto *view = qobject_cast<QAbstractItemView *>(widget)) {
            zzMaskVisibleIndexes(&painter, view, {}, &window);
            continue;
        }
        if (auto *tabs = qobject_cast<QTabBar *>(widget)) {
            for (int index = 0; index < tabs->count(); ++index) {
                zzPaintTextMask(
                    &painter,
                    tabs,
                    zzTextBounds(
                        tabs,
                        tabs->tabRect(index),
                        Qt::AlignCenter,
                        tabs->tabText(index)),
                    &window);
            }
            continue;
        }
        if (auto *progress = qobject_cast<QProgressBar *>(widget)) {
            if (progress->isTextVisible()) {
                QRect textRect = progress->contentsRect();
                if (progress->orientation() == Qt::Horizontal) {
                    textRect.adjust(0, 0, 0, -8);
                } else if (progress->layoutDirection() == Qt::RightToLeft) {
                    textRect.adjust(8, 0, 0, 0);
                } else {
                    textRect.adjust(0, 0, -8, 0);
                }
                zzPaintTextMask(
                    &painter,
                    progress,
                    zzTextBounds(
                        progress,
                        textRect,
                        Qt::AlignCenter,
                        progress->text()),
                    &window);
            }
            continue;
        }
        if (auto *group = qobject_cast<QGroupBox *>(widget)) {
            zzPaintTextMask(
                &painter,
                group,
                zzTextBounds(
                    group,
                    group->contentsRect().adjusted(8, 0, -8, 0),
                    Qt::AlignLeading | Qt::AlignTop,
                    group->title()),
                &window);
            continue;
        }
        if (auto *textEdit = qobject_cast<QTextEdit *>(widget)) {
            if (!textEdit->toPlainText().isEmpty()) {
                zzPaintTextMask(
                    &painter,
                    textEdit->viewport(),
                    textEdit->viewport()->rect(),
                    &window);
            }
        }
    }
    painter.end();
    return mask;
}

/** @brief 将完整综合窗口渲染到指定 DPR 的固定物理画布。 */
[[nodiscard]] QImage zzRenderExampleWindow(
    ZzPureTools::ZzApplicationWindow &window,
    qreal dpr)
{
    const QSize physicalSize(
        qRound(zzScreenshotLogicalWidth * dpr),
        qRound(zzScreenshotLogicalHeight * dpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    window.render(&painter);
    painter.end();
    return image;
}

/** @brief 验证截图不是透明或单色空画布。 */
[[nodiscard]] bool zzHasVisualContent(const QImage &image)
{
    int minimumLuma = 255;
    int maximumLuma = 0;
    bool hasOpaquePixel = false;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(
            image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            if (qAlpha(pixel) == 0) {
                continue;
            }
            hasOpaquePixel = true;
            const int luma = (qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3;
            minimumLuma = std::min(minimumLuma, luma);
            maximumLuma = std::max(maximumLuma, luma);
        }
    }
    return hasOpaquePixel && maximumLuma - minimumLuma >= 8;
}

/** @brief 返回截图遮罩覆盖的物理像素数量。 */
[[nodiscard]] qsizetype zzMaskedPixelCount(const QImage &mask)
{
    qsizetype result = 0;
    for (int y = 0; y < mask.height(); ++y) {
        const uchar *line = mask.constScanLine(y);
        for (int x = 0; x < mask.width(); ++x) {
            if (line[x] != 0) {
                ++result;
            }
        }
    }
    return result;
}

/** @brief 比较未被文字遮罩覆盖的截图像素并生成洋红差异证据。 */
[[nodiscard]] ZzExampleScreenshotComparison zzCompareExampleImages(
    const QImage &expected,
    const QImage &actual,
    const QImage &mask)
{
    ZzExampleScreenshotComparison result;
    result.difference = QImage(
        actual.size(), QImage::Format_ARGB32_Premultiplied);
    result.difference.fill(Qt::transparent);
    const QImage expectedArgb = expected.convertToFormat(
        QImage::Format_ARGB32_Premultiplied);
    const QImage actualArgb = actual.convertToFormat(
        QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < actual.height(); ++y) {
        const auto *expectedLine = reinterpret_cast<const QRgb *>(
            expectedArgb.constScanLine(y));
        const auto *actualLine = reinterpret_cast<const QRgb *>(
            actualArgb.constScanLine(y));
        const uchar *maskLine = mask.constScanLine(y);
        auto *differenceLine = reinterpret_cast<QRgb *>(
            result.difference.scanLine(y));
        for (int x = 0; x < actual.width(); ++x) {
            if (maskLine[x] != 0) {
                continue;
            }
            ++result.comparedPixels;
            const QRgb expectedPixel = expectedLine[x];
            const QRgb actualPixel = actualLine[x];
            const bool different =
                std::abs(qRed(expectedPixel) - qRed(actualPixel))
                    > zzScreenshotChannelTolerance
                || std::abs(qGreen(expectedPixel) - qGreen(actualPixel))
                    > zzScreenshotChannelTolerance
                || std::abs(qBlue(expectedPixel) - qBlue(actualPixel))
                    > zzScreenshotChannelTolerance
                || std::abs(qAlpha(expectedPixel) - qAlpha(actualPixel))
                    > zzScreenshotChannelTolerance;
            if (different) {
                ++result.differentPixels;
                differenceLine[x] = qRgba(255, 0, 255, 255);
            }
        }
    }
    return result;
}

/** @brief 返回三种显式主题及其稳定基线文件名。 */
[[nodiscard]] constexpr auto zzScreenshotThemes()
{
    using ZzTheme = std::pair<ZzFluentUI::ZzThemeMode, const char *>;
    return std::array<ZzTheme, 3>{
        ZzTheme{ZzFluentUI::ZzThemeMode::Light, "light"},
        ZzTheme{ZzFluentUI::ZzThemeMode::Dark, "dark"},
        ZzTheme{ZzFluentUI::ZzThemeMode::HighContrast, "high-contrast"}};
}

} // namespace

ZzExampleSmokeControllerPrivate::ZzExampleSmokeControllerPrivate(
    bool enabled,
    ZzPureTools::ZzPureApplication *pureApplication,
    std::shared_ptr<ZzExampleApplicationContext> applicationContext)
    : application(pureApplication)
    , context(std::move(applicationContext))
    , scenario(readScenario(enabled))
{
    Q_ASSERT(application != nullptr);
    Q_ASSERT(context != nullptr);
    if (scenario == ZzExampleSmokeScenario::MultiWindow
        || scenario == ZzExampleSmokeScenario::CloseConfirm) {
        application->setQuitOnLastWindowClosed(false);
    }
}

bool ZzExampleSmokeControllerPrivate::closeGuardEnabled() const noexcept
{
    return scenario == ZzExampleSmokeScenario::Disabled
        || scenario == ZzExampleSmokeScenario::CloseCancel
        || scenario == ZzExampleSmokeScenario::CloseMinimize
        || scenario == ZzExampleSmokeScenario::CloseConfirm;
}

void ZzExampleSmokeControllerPrivate::windowAttached(
    ZzPureTools::ZzApplicationWindow &window)
{
    if (scenario == ZzExampleSmokeScenario::MultiWindow
        && awaitingActionCreatedWindow) {
        actionCreatedWindow = &window;
    }
    if (scheduled || scenario == ZzExampleSmokeScenario::Disabled) {
        return;
    }
    scheduled = true;
    switch (scenario) {
    case ZzExampleSmokeScenario::Routes:
        scheduleRouteSmoke(window);
        break;
    case ZzExampleSmokeScenario::MultiWindow:
        scheduleMultiWindowSmoke(window);
        break;
    case ZzExampleSmokeScenario::CloseCancel:
    case ZzExampleSmokeScenario::CloseMinimize:
    case ZzExampleSmokeScenario::CloseConfirm:
        scheduleCloseGuardSmoke(window);
        break;
    case ZzExampleSmokeScenario::Screenshot:
        scheduleScreenshotSmoke(window);
        break;
    case ZzExampleSmokeScenario::Invalid:
        QTimer::singleShot(0, application, [this] {
            fail("unsupported smoke scenario");
        });
        break;
    case ZzExampleSmokeScenario::Disabled:
        break;
    }
}

ZzExampleSmokeScenario ZzExampleSmokeControllerPrivate::readScenario(
    bool enabled)
{
    if (!enabled) {
        return ZzExampleSmokeScenario::Disabled;
    }
    const QString value = qEnvironmentVariable(
        "ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO").trimmed();
    if (value.isEmpty() || value == QStringLiteral("routes")) {
        return ZzExampleSmokeScenario::Routes;
    }
    if (value == QStringLiteral("multi-window")) {
        return ZzExampleSmokeScenario::MultiWindow;
    }
    if (value == QStringLiteral("close-cancel")) {
        return ZzExampleSmokeScenario::CloseCancel;
    }
    if (value == QStringLiteral("close-minimize")) {
        return ZzExampleSmokeScenario::CloseMinimize;
    }
    if (value == QStringLiteral("close-confirm")) {
        return ZzExampleSmokeScenario::CloseConfirm;
    }
    if (value == QStringLiteral("screenshot")) {
        return ZzExampleSmokeScenario::Screenshot;
    }
    return ZzExampleSmokeScenario::Invalid;
}

void ZzExampleSmokeControllerPrivate::scheduleRouteSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        auto *controller = window.navigationController();
        auto *searchEdit = window.findChild<QLineEdit *>(
            QStringLiteral("zzExamplePageSearch"));
        auto *backAction = window.findChild<QAction *>(
            QStringLiteral("zzExampleBackAction"));
        auto *forwardAction = window.findChild<QAction *>(
            QStringLiteral("zzExampleForwardAction"));
        auto *themeAction = window.findChild<QAction *>(
            QStringLiteral("zzExampleThemeAction"));
        auto *settingsAction = window.findChild<QAction *>(
            QStringLiteral("zzExampleSettingsAction"));
        auto *theme = application->themeController();
        if (controller == nullptr || searchEdit == nullptr
            || backAction == nullptr || forwardAction == nullptr
            || themeAction == nullptr || settingsAction == nullptr
            || theme == nullptr) {
            fail("route smoke has incomplete window shell controls");
            return;
        }
        if (controller->currentRoute().value() != QStringLiteral("home")) {
            fail("route smoke did not start on the home route");
            return;
        }
        // 独立路由必须创建专属预览，而非隐藏旧的混合页内容。
        if (!controller->navigate(ZzPureTools::ZzRouteId(
                QStringLiteral("push-button")))) {
            fail("button demo route is unavailable");
            return;
        }
        auto *buttonPage = window.findChild<QWidget *>(
            QStringLiteral("zzExampleControlPage_push-button"));
        if (buttonPage == nullptr
            || buttonPage->findChildren<ZzFluentUI::ZzPushButton *>().isEmpty()
            || !buttonPage->findChildren<QProgressBar *>().isEmpty()
            || !buttonPage->findChildren<QLineEdit *>().isEmpty()) {
            fail("button demo is not isolated");
            return;
        }
        if (!controller->navigate(ZzPureTools::ZzRouteId(
                QStringLiteral("progress-bar")))) {
            fail("progress demo route is unavailable");
            return;
        }
        auto *progressPage = window.findChild<QWidget *>(
            QStringLiteral("zzExampleControlPage_progress-bar"));
        auto *progress = progressPage != nullptr
            ? progressPage->findChild<QProgressBar *>(
                QStringLiteral("zzExampleProgressDeterminate")) : nullptr;
        auto *valueSlider = progressPage != nullptr
            ? progressPage->findChild<QSlider *>(
                QStringLiteral("zzExampleControlValue")) : nullptr;
        if (progress == nullptr || valueSlider == nullptr) {
            fail("progress demo has no adjustable preview");
            return;
        }
        valueSlider->setValue(37);
        if (progress->value() != 37 || progress->text() != QStringLiteral("37%")) {
            fail("progress demo value and label are not synchronized");
            return;
        }
        for (const auto &route : ZzExampleRouteCatalog::routes()) {
            const QString routeId = zzFromUtf8(route.routeId);
            auto result = controller->navigate(
                ZzPureTools::ZzRouteId(routeId));
            if (!result) {
                fail("route smoke navigation failed");
                return;
            }
            if (!verifyStandardSurfaceComposition(window, routeId)) {
                fail("route smoke standard surface composition failed", routeId);
                return;
            }
            if (routeId == QStringLiteral("icons")
                && !zzIconPageReady(window)) {
                fail("route smoke icon integration failed");
                return;
            }
            // 可选人工验收产物，仅在烟测模式下按路由导出真实窗口。
            if (routeId == QStringLiteral("range-slider") && !zzRangeSliderPageReady(window)) {
                fail("route smoke range slider integration failed");
                return;
            }
            if (routeId == QStringLiteral("border-beam") && !zzBorderBeamPageReady(window)) {
                fail("route smoke border beam integration failed");
                return;
            }
            if (routeId == QStringLiteral("audio-level-meter") && !zzAudioLevelMeterPageReady(window, theme)) {
                fail("route smoke audio level meter integration failed");
                return;
            }
            const QString previewDirectory = qEnvironmentVariable(
                "ZZ_EXAMPLE_ROUTE_SCREENSHOT_DIR");
            if (routeId == QStringLiteral("radial-gauge") && !zzRadialGaugePageReady(window, theme)) {
                fail("route smoke radial gauge integration failed");
                return;
            }
            if (routeId == QStringLiteral("liquid-gauge") && !zzLiquidGaugePageReady(window)) {
                fail("route smoke liquid gauge integration failed");
                return;
            }
            if (routeId == QStringLiteral("progress-ring") && !zzProgressRingPageReady(window, theme)) {
                fail("route smoke progress ring integration failed");
                return;
            }
            if (routeId == QStringLiteral("timeline") && !ZzExampleTimelineSmoke::isPageReady(window, theme)) {
                fail("route smoke timeline integration failed");
                return;
            }
            if (routeId == QStringLiteral("info-bar") && !ZzExampleInfoBarSmoke::isPageReady(window, theme)) {
                fail("route smoke info bar integration failed");
                return;
            }
            if (routeId == QStringLiteral("carousel") && !ZzExampleCarouselSmoke::isPageReady(window)) {
                fail("route smoke carousel integration failed");
                return;
            }
            if (routeId == QStringLiteral("color-picker") && !ZzExampleColorPickerSmoke::isPageReady(window)) {
                fail("route smoke color picker integration failed");
                return;
            }
            if (!previewDirectory.isEmpty()) {
                // 只刷新布局；嵌套事件循环会触发烟测自动关闭定时器并销毁窗口。
                QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                if (!QDir().mkpath(previewDirectory)
                    || !window.grab().save(QDir(previewDirectory).filePath(
                        routeId + QStringLiteral(".png")))) {
                    fail("could not export route preview", routeId);
                    return;
                }
                if (routeId == QStringLiteral("audio-level-meter") || routeId == QStringLiteral("radial-gauge")
                    || routeId == QStringLiteral("liquid-gauge") || routeId == QStringLiteral("progress-ring")
                    || routeId == QStringLiteral("timeline") || routeId == QStringLiteral("info-bar")
                    || routeId == QStringLiteral("carousel") || routeId == QStringLiteral("color-picker")) {
                    const bool colorPicker = routeId == QStringLiteral("color-picker");
                    const bool carousel = routeId == QStringLiteral("carousel");
                    const bool infoBar = routeId == QStringLiteral("info-bar");
                    const bool timeline = routeId == QStringLiteral("timeline");
                    const bool liquid = routeId == QStringLiteral("liquid-gauge");
                    const bool progressRing = routeId == QStringLiteral("progress-ring");
                    auto *page = window.findChild<QWidget *>(colorPicker ? QStringLiteral("zzExampleColorPickerPage")
                        : carousel ? QStringLiteral("zzExampleCarouselPage")
                        : infoBar ? QStringLiteral("zzExampleInfoBarPage")
                        : timeline ? QStringLiteral("zzExampleTimelinePage")
                        : progressRing ? QStringLiteral("zzExampleProgressRingPage")
                        : liquid ? QStringLiteral("zzExampleLiquidGaugePage")
                        : routeId == QStringLiteral("radial-gauge") ? QStringLiteral("zzExampleRadialGaugePage")
                        : QStringLiteral("zzExampleAudioLevelMeterPage"));
                    auto *scroll = page ? page->findChild<QScrollArea *>() : nullptr;
                    if (!scroll || !scroll->widget()
                        || !scroll->widget()->grab().save(QDir(previewDirectory).filePath(
                            routeId + QStringLiteral("-content.png")))) {
                        fail("could not export custom widget content preview");
                        return;
                    }
                    if (routeId == QStringLiteral("radial-gauge") || liquid || timeline || infoBar || carousel || colorPicker) {
                        auto *tabs = page->findChild<QTabWidget *>(colorPicker ? QStringLiteral("colorPickerEditorTabs")
                            : carousel ? QStringLiteral("carouselEditorTabs")
                            : infoBar ? QStringLiteral("infoBarEditorTabs")
                            : timeline ? QStringLiteral("zzTimeline_editorTabs")
                            : liquid ? QStringLiteral("zzLiquidPropertyTabs")
                            : QStringLiteral("zzRadialPropertyTabs"));
                        const bool reducedMotion = theme->reducedMotion();
                        theme->setReducedMotion(true);
                        const auto originalMode = theme->mode();
                        for (const auto mode : {ZzFluentUI::ZzThemeMode::Light, ZzFluentUI::ZzThemeMode::Dark,
                                 ZzFluentUI::ZzThemeMode::HighContrast}) {
                            if (mode == ZzFluentUI::ZzThemeMode::HighContrast && !timeline && !infoBar && !carousel && !colorPicker) continue;
                            theme->setMode(mode);
                            QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
                            const QString name = mode == ZzFluentUI::ZzThemeMode::HighContrast
                                ? QStringLiteral("high-contrast") : mode == ZzFluentUI::ZzThemeMode::Dark
                                ? QStringLiteral("dark") : QStringLiteral("light");
                            for (int index = 0; index < tabs->count(); ++index) {
                                tabs->setCurrentIndex(index);
                                QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                                if (!scroll->widget()->grab().save(QDir(previewDirectory).filePath(
                                    QStringLiteral("%1-%2-%3.png").arg(routeId, name).arg(index)))) {
                                    fail("could not export gauge editor preview");
                                    return;
                                }
                            }
                        }
                        tabs->setCurrentIndex(0);
                        theme->setMode(originalMode);
                        theme->setReducedMotion(reducedMotion);
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
                    }
                    if (progressRing) {
                        const auto originalMode = theme->mode();
                        const bool reducedMotion = theme->reducedMotion();
                        theme->setReducedMotion(true);
                        auto *custom = page->findChild<QCheckBox *>(QStringLiteral("zzProgressRingCustom"));
                        for (const auto mode : {ZzFluentUI::ZzThemeMode::Light, ZzFluentUI::ZzThemeMode::Dark}) {
                            theme->setMode(mode);
                            QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
                            for (int index = 0; index < 2; ++index) {
                                custom->setChecked(index == 1);
                                QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                                const QString name = mode == ZzFluentUI::ZzThemeMode::Dark
                                    ? QStringLiteral("dark") : QStringLiteral("light");
                                if (!scroll->widget()->grab().save(QDir(previewDirectory).filePath(
                                        QStringLiteral("progress-ring-%1-%2.png").arg(name).arg(index)))) {
                                    fail("could not export progress ring editor preview");
                                    return;
                                }
                            }
                        }
                        custom->setChecked(false);
                        theme->setMode(originalMode);
                        theme->setReducedMotion(reducedMotion);
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
                    }
                }
            }
        }

        settingsAction->trigger();
        auto *settingsWindow = window.findChild<QWidget *>(
            QStringLiteral("zzExampleSettingsWindow"),
            Qt::FindDirectChildrenOnly);
        if (settingsWindow == nullptr || !settingsWindow->isVisible()) {
            fail("route smoke settings action integration failed");
            return;
        }
        settingsWindow->close();

        const QString routeBeforeSearch = controller->currentRoute().value();
        searchEdit->setText(QStringLiteral("push-button"));
        if (!QMetaObject::invokeMethod(
                searchEdit, "returnPressed", Qt::DirectConnection)
            || controller->currentRoute().value()
                != QStringLiteral("push-button")
            || !searchEdit->text().isEmpty()
            || !backAction->isEnabled()) {
            fail("route smoke search integration failed");
            return;
        }
        backAction->trigger();
        if (controller->currentRoute().value() != routeBeforeSearch
            || !forwardAction->isEnabled()) {
            fail("route smoke back action integration failed");
            return;
        }
        forwardAction->trigger();
        if (controller->currentRoute().value()
            != QStringLiteral("push-button")) {
            fail("route smoke forward action integration failed");
            return;
        }

        const auto themeBeforeAction = theme->mode();
        themeAction->trigger();
        if (theme->mode() == themeBeforeAction) {
            fail("route smoke theme action integration failed");
            return;
        }

        auto homeResult = controller->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("home")));
        auto *controlsCard = window.findChild<QAbstractButton *>(
            QStringLiteral("zzExampleRouteCard_push-button"));
        if (!homeResult || controlsCard == nullptr
            || !controlsCard->isVisibleTo(&window)) {
            fail("route smoke could not reach the home quick actions");
            return;
        }
        controlsCard->click();
        if (controller->currentRoute().value()
            != QStringLiteral("push-button")) {
            fail("route smoke home quick action integration failed");
            return;
        }

        verifyActivityTailFollowing(window);
    });
}

bool ZzExampleSmokeControllerPrivate::verifyStandardSurfaceComposition(
    ZzPureTools::ZzApplicationWindow &window,
    const QString &routeId) const
{
    for (const auto &route : ZzExampleRouteCatalog::routes()) {
        if (!route.controlKind.has_value() || zzFromUtf8(route.routeId) != routeId) {
            continue;
        }
        auto *page = window.findChild<QWidget *>(
            QStringLiteral("zzExampleControlPage_%1").arg(routeId));
        if (page == nullptr || !page->isVisibleTo(&window)) {
            return false;
        }
        const auto has = [page]<typename Control>() {
            return !page->findChildren<Control *>().isEmpty();
        };
        using Kind = ZzExampleControlKind;
        switch (*route.controlKind) {
        case Kind::PushButton: {
            auto *button = page->findChild<ZzFluentUI::ZzPushButton *>(
                QStringLiteral("zzExampleCheckableButton"));
            if (button == nullptr || !button->isCheckable()) return false;
            const bool before = button->isChecked();
            button->click();
            const bool changed = button->isChecked() != before;
            button->click();
            return changed && button->isChecked() == before;
        }
        case Kind::IconButton: return has.operator()<ZzFluentUI::ZzIconButton>();
        case Kind::ToolButton: return has.operator()<QToolButton>();
        case Kind::RadioButton: return has.operator()<QRadioButton>();
        case Kind::CheckBox: return page->findChildren<QCheckBox *>().size() >= 4;
        case Kind::ToggleSwitch: return has.operator()<ZzFluentUI::ZzToggleSwitch>();
        case Kind::LineEdit: return has.operator()<QLineEdit>();
        case Kind::PlainTextEdit: return has.operator()<QPlainTextEdit>();
        case Kind::ComboBox: return has.operator()<QComboBox>();
        case Kind::MultiSelectComboBox: return has.operator()<ZzFluentUI::ZzMultiSelectComboBox>();
        case Kind::SpinBox: return has.operator()<ZzFluentUI::ZzSpinBox>();
        case Kind::DoubleSpinBox: return has.operator()<ZzFluentUI::ZzDoubleSpinBox>();
        case Kind::CalendarPicker: return has.operator()<ZzFluentUI::ZzCalendarPicker>();
        case Kind::RollerPicker: return has.operator()<ZzFluentUI::ZzRollerPicker>();
        case Kind::Slider: return has.operator()<QSlider>();
        case Kind::ScrollBar: {
            auto *horizontal = page->findChild<ZzFluentUI::ZzScrollBar *>(
                QStringLiteral("zzExampleScrollHorizontal"));
            auto *vertical = page->findChild<ZzFluentUI::ZzScrollBar *>(
                QStringLiteral("zzExampleScrollVertical"));
            if (horizontal == nullptr || vertical == nullptr) return false;
            horizontal->setValue(62);
            vertical->setValue(47);
            return horizontal->orientation() == Qt::Horizontal && horizontal->value() == 62
                && vertical->orientation() == Qt::Vertical && vertical->value() == 47;
        }
        case Kind::ProgressRing: return has.operator()<ZzFluentUI::ZzProgressRing>();
        case Kind::MessageBar: return has.operator()<ZzFluentUI::ZzMessageBar>();
        case Kind::GroupBox: {
            auto *normal = page->findChild<QGroupBox *>(QStringLiteral("zzExampleGroupBoxNormal"));
            auto *checkable = page->findChild<QGroupBox *>(QStringLiteral("zzExampleGroupBoxCheckable"));
            auto *disabled = page->findChild<QGroupBox *>(QStringLiteral("zzExampleGroupBoxDisabled"));
            auto *flat = page->findChild<QGroupBox *>(QStringLiteral("zzExampleGroupBoxFlat"));
            auto *nested = page->findChild<QGroupBox *>(QStringLiteral("zzExampleGroupBoxNested"));
            auto *option = page->findChild<QCheckBox *>(QStringLiteral("zzExampleGroupBoxOption"));
            if (normal == nullptr || checkable == nullptr || disabled == nullptr || flat == nullptr
                || nested == nullptr || option == nullptr || !checkable->isCheckable()
                || !checkable->isChecked() || disabled->isEnabled() || !flat->isFlat()
                || nested->findChildren<QGroupBox *>().isEmpty()) return false;
            checkable->setChecked(false);
            const bool disabledByGroup = !option->isEnabled();
            checkable->setChecked(true);
            return disabledByGroup && option->isEnabled() && option->isChecked();
        }
        case Kind::InfoBadge: return has.operator()<ZzFluentUI::ZzInfoBadge>();
        case Kind::ProgressBar: {
            auto *busy = page->findChild<QProgressBar *>(QStringLiteral("zzExampleProgressBusy"));
            auto *disabled = page->findChild<QProgressBar *>(QStringLiteral("zzExampleProgressDisabled"));
            auto *vertical = page->findChild<QProgressBar *>(QStringLiteral("zzExampleProgressVertical"));
            return busy != nullptr && busy->minimum() == 0 && busy->maximum() == 0
                && disabled != nullptr && !disabled->isEnabled() && disabled->value() == 42
                && vertical != nullptr && vertical->orientation() == Qt::Vertical;
        }
        }
    }
    if (routeId == QStringLiteral("list-view")) {
        auto *view = window.findChild<QListView *>(
            QStringLiteral("zzExampleListView"));
        return view != nullptr && view->model() != nullptr
            && view->model()->rowCount() > 0
            && view->itemDelegate() != nullptr;
    }
    if (routeId == QStringLiteral("table-view")) {
        auto *view = window.findChild<QTableView *>(
            QStringLiteral("zzExampleTableView"));
        return view != nullptr && view->model() != nullptr
            && view->model()->rowCount() > 0
            && view->model()->columnCount() > 1
            && view->itemDelegate() != nullptr;
    }
    if (routeId == QStringLiteral("tree-view")) {
        auto *view = window.findChild<QTreeView *>(
            QStringLiteral("zzExampleTreeView"));
        return view != nullptr && view->model() != nullptr
            && view->model()->rowCount() > 0
            && view->itemDelegate() != nullptr;
    }
    if (routeId == QStringLiteral("cards")) {
        auto *page = window.findChild<QWidget *>(
            QStringLiteral("zzExampleCardsPage"));
        return page != nullptr
            && !page->findChildren<QLCDNumber *>().isEmpty();
    }
    return true;
}

void ZzExampleSmokeControllerPrivate::verifyActivityTailFollowing(
    ZzPureTools::ZzApplicationWindow &window)
{
    auto *activity = window.findChild<QTableView *>(
        QStringLiteral("zzExampleActivityLogView"));
    if (activity == nullptr) {
        fail("route smoke has no activity log view");
        return;
    }

    constexpr int overflowRows = 48;
    for (int row = 0; row < overflowRows; ++row) {
        context->activityModel().append(
            QStringLiteral("tail-follow-%1").arg(row));
    }
    QTimer::singleShot(0, &window, [this, &window, activity] {
        QScrollBar *const scrollBar = activity->verticalScrollBar();
        if (scrollBar->maximum() <= scrollBar->minimum()
            || scrollBar->value() != scrollBar->maximum()) {
            fail("activity log view did not follow appended rows at the tail");
            return;
        }

        scrollBar->setValue(scrollBar->minimum());
        const int pausedValue = scrollBar->value();
        context->activityModel().append(
            QStringLiteral("tail-follow-paused"));
        QTimer::singleShot(
            0,
            &window,
            [this, &window, activity, pausedValue] {
                QScrollBar *const pausedScrollBar =
                    activity->verticalScrollBar();
                if (pausedScrollBar->value() != pausedValue) {
                    fail("activity log view interrupted manual history browsing");
                    return;
                }

                pausedScrollBar->setValue(pausedScrollBar->maximum());
                context->activityModel().append(
                    QStringLiteral("tail-follow-resumed"));
                QTimer::singleShot(0, &window, [this, activity] {
                    QScrollBar *const resumedScrollBar =
                        activity->verticalScrollBar();
                    if (resumedScrollBar->value()
                        != resumedScrollBar->maximum()) {
                        fail("activity log view did not resume tail following");
                        return;
                    }
                    application->beginShutdown();
                    QCoreApplication::exit(EXIT_SUCCESS);
                });
            });
    });
}

void ZzExampleSmokeControllerPrivate::scheduleMultiWindowSmoke(
    ZzPureTools::ZzApplicationWindow &firstWindow)
{
    QTimer::singleShot(0, &firstWindow, [this, &firstWindow] {
        if (application->windowCount() != 1) {
            fail("multi-window smoke did not start with one window");
            return;
        }
        auto *newWindowAction = firstWindow.findChild<QAction *>(
            QStringLiteral("zzExampleNewWindowAction"));
        if (newWindowAction == nullptr || !newWindowAction->isEnabled()) {
            fail("multi-window smoke has no enabled new-window action");
            return;
        }
        actionCreatedWindow = nullptr;
        awaitingActionCreatedWindow = true;
        newWindowAction->trigger();
        awaitingActionCreatedWindow = false;
        auto *secondWindow = actionCreatedWindow;
        actionCreatedWindow = nullptr;
        auto *firstNavigation = firstWindow.navigationController();
        auto *secondNavigation = secondWindow == nullptr
            ? nullptr : secondWindow->navigationController();
        auto *firstShell = ZzExampleWindowShell::attachedTo(firstWindow);
        auto *secondShell = secondWindow == nullptr
            ? nullptr : ZzExampleWindowShell::attachedTo(*secondWindow);
        if (application->windowCount() != 2
            || secondWindow == nullptr
            || firstNavigation == nullptr || secondNavigation == nullptr
            || firstNavigation == secondNavigation
            || firstWindow.windowAgent() == nullptr
            || secondWindow->windowAgent() == nullptr
            || firstWindow.windowAgent() == secondWindow->windowAgent()
            || firstShell == nullptr || secondShell == nullptr
            || firstShell == secondShell) {
            fail("multi-window smoke found shared window-owned state");
            return;
        }

        auto *firstActivity = firstWindow.findChild<QTableView *>(
            QStringLiteral("zzExampleActivityLogView"));
        auto *secondActivity = secondWindow->findChild<QTableView *>(
            QStringLiteral("zzExampleActivityLogView"));
        if (firstActivity == nullptr || secondActivity == nullptr
            || firstActivity->model() != &context->activityModel()
            || secondActivity->model() != &context->activityModel()) {
            fail("multi-window smoke did not share the activity model");
            return;
        }

        const int activityRows = context->activityModel().rowCount();
        auto firstRoute = firstNavigation->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("push-button")));
        auto secondRoute = secondNavigation->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("platform")));
        auto *secondSettingsAction = secondWindow->findChild<QAction *>(
            QStringLiteral("zzExampleSettingsAction"));
        if (secondSettingsAction != nullptr) {
            secondSettingsAction->trigger();
        }
        auto *secondSettingsWindow = secondWindow->findChild<QWidget *>(
            QStringLiteral("zzExampleSettingsWindow"),
            Qt::FindDirectChildrenOnly);
        const bool secondDockWasVisible =
            secondShell->isActivityDockVisible();
        firstShell->setActivityDockVisible(!secondDockWasVisible);
        if (!firstRoute || !secondRoute
            || firstNavigation->currentRoute().value()
                != QStringLiteral("push-button")
            || secondNavigation->currentRoute().value()
                != QStringLiteral("platform")
            || secondSettingsAction == nullptr
            || secondSettingsWindow == nullptr
            || !secondSettingsWindow->isVisible()
            || secondShell->isActivityDockVisible()
                != secondDockWasVisible
            || context->activityModel().rowCount() < activityRows + 2) {
            fail("multi-window smoke isolation assertions failed");
            return;
        }

        QPointer<ZzPureTools::ZzApplicationWindow> secondObserver(
            secondWindow);
        if (!secondWindow->close()) {
            fail("multi-window smoke could not close the second window");
            return;
        }
        QTimer::singleShot(0, application,
            [this, secondObserver] {
                if (!secondObserver.isNull()
                    || application->windowCount() != 1) {
                    fail("multi-window smoke did not erase the closed window");
                    return;
                }
                QCoreApplication::quit();
            });
    });
}

void ZzExampleSmokeControllerPrivate::scheduleCloseGuardSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        const int activityRows = context->activityModel().rowCount();
        QTimer::singleShot(0, application,
            [this] { chooseCloseDialogButton(); });
        QPointer<ZzPureTools::ZzApplicationWindow> observer(&window);
        const bool closed = window.close();
        if (scenario == ZzExampleSmokeScenario::CloseConfirm) {
            if (!closed) {
                fail("close-confirm smoke did not accept the close event");
                return;
            }
            QTimer::singleShot(0, application,
                [this, observer, activityRows] {
                    if (!observer.isNull()
                        || application->windowCount() != 0
                        || context->activityModel().rowCount()
                            != activityRows + 1) {
                        fail("close-confirm smoke state mismatch");
                        return;
                    }
                    QCoreApplication::quit();
                });
            return;
        }

        const bool expectsMinimized =
            scenario == ZzExampleSmokeScenario::CloseMinimize;
        if (closed || application->windowCount() != 1
            || observer.isNull()
            || window.isMinimized() != expectsMinimized
            || context->activityModel().rowCount() != activityRows + 1) {
            fail("close guard smoke state mismatch");
            return;
        }
        QTimer::singleShot(0, application, [this] {
            application->beginShutdown();
            QCoreApplication::exit(EXIT_SUCCESS);
        });
    });
}

void ZzExampleSmokeControllerPrivate::scheduleScreenshotSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        bool dprValid = false;
        const qreal expectedDpr = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_DPR").toDouble(&dprValid);
        const QString baselineRoot = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_BASELINE_DIR").trimmed();
        const QString reportRoot = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_REPORT_DIR").trimmed();
        const QString baselineSubdirectory = qEnvironmentVariable(
            "ZZ_PURETOOLS_EXAMPLE_SCREENSHOT_BASELINE_SUBDIR").trimmed();
        const bool safeSubdirectory = !baselineSubdirectory.isEmpty()
            && !baselineSubdirectory.contains(QLatin1Char('/'))
            && !baselineSubdirectory.contains(QLatin1Char('\\'))
            && baselineSubdirectory != QStringLiteral(".")
            && baselineSubdirectory != QStringLiteral("..");
        if (!dprValid || !std::isfinite(expectedDpr) || expectedDpr <= 0.0
            || baselineRoot.isEmpty() || reportRoot.isEmpty()
            || !safeSubdirectory) {
            fail("invalid screenshot environment");
            return;
        }

        QScreen *screen = QApplication::primaryScreen();
        if (screen == nullptr
            || std::abs(screen->devicePixelRatio() - expectedDpr) > 0.01) {
            fail(
                "screenshot DPR mismatch",
                screen == nullptr
                    ? QStringLiteral("primary screen is unavailable")
                    : QStringLiteral("actual=%1; expected=%2")
                          .arg(screen->devicePixelRatio(), 0, 'f', 2)
                          .arg(expectedDpr, 0, 'f', 2));
            return;
        }
        if (QFontInfo(QApplication::font()).family()
                != QStringLiteral("DejaVu Sans")
            || QApplication::font().pointSize() != 10) {
            fail("screenshot reference font mismatch");
            return;
        }

        auto *theme = application->themeController();
        if (theme == nullptr) {
            fail("screenshot theme controller is unavailable");
            return;
        }
        theme->setReducedMotion(true);
        window.setFixedSize(
            zzScreenshotLogicalWidth,
            zzScreenshotLogicalHeight);
        if (QWidget *focused = QApplication::focusWidget()) {
            focused->clearFocus();
        }
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        const QString baselineDirectory = QDir(baselineRoot).filePath(
            baselineSubdirectory);
        const QString reportDirectory = QDir(reportRoot).filePath(
            baselineSubdirectory);
        const bool updateBaselines = qEnvironmentVariableIntValue(
            "ZZ_UPDATE_EXAMPLE_SCREENSHOTS") == 1;
        for (const auto &[mode, fileStem] : zzScreenshotThemes()) {
            theme->setMode(mode);
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            window.repaint();
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            const QImage actual = zzRenderExampleWindow(window, expectedDpr);
            const QImage mask = zzBuildExampleTextMask(window, expectedDpr);
            const qsizetype totalPixels =
                static_cast<qsizetype>(actual.width())
                * static_cast<qsizetype>(actual.height());
            const qsizetype maskedPixels = zzMaskedPixelCount(mask);
            if (actual.size() != QSize(
                    qRound(zzScreenshotLogicalWidth * expectedDpr),
                    qRound(zzScreenshotLogicalHeight * expectedDpr))
                || !zzHasVisualContent(actual)
                || mask.size() != actual.size()
                || maskedPixels == 0
                || maskedPixels * 2 >= totalPixels) {
                fail(
                    "invalid screenshot surface",
                    QStringLiteral("theme=%1; masked=%2; total=%3")
                        .arg(QString::fromLatin1(fileStem))
                        .arg(maskedPixels)
                        .arg(totalPixels));
                return;
            }

            const QString fileName = QString::fromLatin1(fileStem)
                + QStringLiteral(".png");
            const QString baselinePath = QDir(baselineDirectory).filePath(
                fileName);
            if (updateBaselines) {
                if (!QDir().mkpath(baselineDirectory)
                    || !actual.save(baselinePath, "PNG")) {
                    fail("could not update screenshot baseline", baselinePath);
                    return;
                }
                continue;
            }

            const QImage expected(baselinePath);
            if (expected.isNull() || expected.size() != actual.size()) {
                fail("missing or invalid screenshot baseline", baselinePath);
                return;
            }
            const ZzExampleScreenshotComparison comparison =
                zzCompareExampleImages(expected, actual, mask);
            if (comparison.comparedPixels <= 0) {
                fail(
                    "screenshot comparison has no visible pixels",
                    baselinePath);
                return;
            }
            const qreal differenceRatio =
                static_cast<qreal>(comparison.differentPixels)
                / static_cast<qreal>(comparison.comparedPixels);
            const qreal maximumDifferenceRatio =
                zzScreenshotMaximumDifferenceRatio();
            if (differenceRatio <= maximumDifferenceRatio) {
                continue;
            }

            if (!QDir().mkpath(reportDirectory)) {
                fail(
                    "could not create screenshot report directory",
                    reportDirectory);
                return;
            }
            const QString actualPath = QDir(reportDirectory).filePath(
                QString::fromLatin1(fileStem) + QStringLiteral("-actual.png"));
            const QString differencePath = QDir(reportDirectory).filePath(
                QString::fromLatin1(fileStem) + QStringLiteral("-diff.png"));
            if (!actual.save(actualPath, "PNG")
                || !comparison.difference.save(differencePath, "PNG")) {
                fail(
                    "could not write screenshot difference evidence",
                    reportDirectory);
                return;
            }
            fail(
                "screenshot difference exceeds tolerance",
                QStringLiteral(
                    "theme=%1; ratio=%2; maximum=%3; actual=%4; diff=%5")
                    .arg(QString::fromLatin1(fileStem))
                    .arg(differenceRatio, 0, 'f', 6)
                    .arg(maximumDifferenceRatio, 0, 'f', 6)
                    .arg(actualPath, differencePath));
            return;
        }

        application->beginShutdown();
        QCoreApplication::exit(EXIT_SUCCESS);
    });
}

void ZzExampleSmokeControllerPrivate::chooseCloseDialogButton()
{
    auto *dialog = qobject_cast<QMessageBox *>(
        QApplication::activeModalWidget());
    if (dialog == nullptr) {
        fail("close guard smoke did not open a message box");
        return;
    }
    const QMessageBox::ButtonRole expectedRole =
        zzCloseButtonRole(scenario);
    for (QAbstractButton *button : dialog->buttons()) {
        if (dialog->buttonRole(button) == expectedRole) {
            button->click();
            return;
        }
    }
    dialog->reject();
    fail("close guard smoke could not find the expected button");
}

void ZzExampleSmokeControllerPrivate::fail(
    const char *reason,
    const QString &details) const
{
    qCritical().noquote()
        << "ZzPureToolsExample smoke failed:"
        << reason
        << details;
    QCoreApplication::exit(EXIT_FAILURE);
}

} // namespace ZzExample
