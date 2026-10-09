#include "ZzExampleRadialGaugeHelpers.h"

namespace ZzExample {
namespace {

/** @brief 装配径向仪表属性编辑器的实现。 */
class ZzExampleRadialGaugeEditor final
{
public:
    static void buildRadialGaugeEditor(QWidget *propertiesCard, QWidget *gaugePropertyPage, QVBoxLayout *gaugePropertyLayout);
};

} // namespace

void ZzExampleRadialGaugeEditor::buildRadialGaugeEditor(
    QWidget *propertiesCard, QWidget *gaugePropertyPage, QVBoxLayout *gaugePropertyLayout)
{
    auto *previewLayout = new QHBoxLayout;
    previewLayout->setSpacing(32);

    auto *gauge = new ZzRadialGauge(propertiesCard);
    gauge->setObjectName(QStringLiteral("radialGaugePreview"));
    gauge->setRange(0, 240);
    gauge->setValue(210);
    gauge->setMajorTickCount(10);
    gauge->setMinorTickCount(0);
    gauge->setMajorTickLength(7.0);
    gauge->setNeedleWidth(5.0);
    gauge->setSingleStep(1);
    gauge->setPageStep(10);
    gauge->setFixedSize(300, 300);
    const QColor firstRangeDefaultColor(QStringLiteral("#21BCE2"));
    const QColor secondRangeDefaultColor(QStringLiteral("#FFB900"));
    const QColor thirdRangeDefaultColor(QStringLiteral("#FF6475"));
    auto *firstRange = gauge->addRange(0, 80, firstRangeDefaultColor);
    auto *secondRange = gauge->addRange(80, 160, secondRangeDefaultColor);
    auto *thirdRange = gauge->addRange(160, 240, thirdRangeDefaultColor);
    auto *gaugePreviewHost = new QWidget(gaugePropertyPage);
    auto *gaugePreviewHostLayout = new QVBoxLayout(gaugePreviewHost);
    gaugePreviewHostLayout->setContentsMargins(0, 0, 0, 0);
    gaugePreviewHostLayout->addWidget(gauge, 0, Qt::AlignHCenter | Qt::AlignTop);
    gaugePreviewHostLayout->addStretch();
    previewLayout->addWidget(gaugePreviewHost, 1);

    auto *propertyTabs = makePropertyTabs(propertiesCard);
    propertyTabs->setObjectName(QStringLiteral("zzRadialEditor_propertyTabs"));

    propertyTabs->setMinimumWidth(440);
    propertyTabs->setMinimumHeight(430);

    const auto makePropertyForm = [propertyTabs](QFormLayout *&form) {
        auto *page = new QWidget(propertyTabs);
        form = new QFormLayout(page);
        form->setContentsMargins(12, 12, 12, 12);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);
        return page;
    };

    QFormLayout *basicForm = nullptr;
    QFormLayout *scaleForm = nullptr;
    QFormLayout *needleForm = nullptr;
    QFormLayout *gradientForm = nullptr;
    QFormLayout *rangeForm = nullptr;
    auto *basicPage = makePropertyForm(basicForm);
    auto *scalePage = makePropertyForm(scaleForm);
    auto *needlePage = makePropertyForm(needleForm);
    auto *gradientPage = makePropertyForm(gradientForm);
    auto *rangePage = makePropertyForm(rangeForm);
    propertyTabs->addTab(basicPage, zzGaugeText("基础"));
    propertyTabs->addTab(scalePage, zzGaugeText("刻度与标签"));
    propertyTabs->addTab(needlePage, zzGaugeText("指针与文本"));
    propertyTabs->addTab(gradientPage, zzGaugeText("进度渐变"));
    propertyTabs->addTab(rangePage, zzGaugeText("彩色区间"));

    auto *scaleModeCombo = new QComboBox(propertiesCard);
    scaleModeCombo->setObjectName(QStringLiteral("zzRadialEditor_scaleModeCombo"));
    scaleModeCombo->addItem(zzGaugeText("Track（纯轨道）"), ZzRadialGauge::TrackScale);
    scaleModeCombo->addItem(zzGaugeText("Progress（数值进度）"), ZzRadialGauge::ProgressScale);
    scaleModeCombo->addItem(zzGaugeText("Ranges（彩色区间）"), ZzRadialGauge::RangeScale);
    scaleModeCombo->setCurrentIndex(scaleModeCombo->findData(gauge->scaleMode()));

    const auto makeCapStyleCombo = [propertiesCard](Qt::PenCapStyle currentStyle) {
        auto *combo = new QComboBox(propertiesCard);
        combo->addItem(zzGaugeText("FlatCap"), Qt::FlatCap);
        combo->addItem(zzGaugeText("SquareCap"), Qt::SquareCap);
        combo->addItem(zzGaugeText("RoundCap"), Qt::RoundCap);
        combo->setCurrentIndex(combo->findData(currentStyle));
        return combo;
    };
    auto *trackCapStyleCombo = makeCapStyleCombo(gauge->trackCapStyle());
    trackCapStyleCombo->setObjectName(QStringLiteral("zzRadialEditor_trackCapStyleCombo"));
    auto *ringCapStyleCombo = makeCapStyleCombo(gauge->ringCapStyle());
    ringCapStyleCombo->setObjectName(QStringLiteral("zzRadialEditor_ringCapStyleCombo"));

    auto *valueSlider = makeValueSlider(propertiesCard, gauge->minimum(), gauge->maximum(), gauge->value());
    valueSlider->setObjectName(QStringLiteral("zzRadialEditor_valueSlider"));
    auto *valueAnimationDurationSlider
        = makeValueSlider(propertiesCard, 0, 1000, gauge->valueAnimationDuration(), 10, 100);
    valueAnimationDurationSlider->setObjectName(
        QStringLiteral("zzRadialEditor_valueAnimationDurationSlider"));

    auto *interactiveCheck = new QCheckBox(zzGaugeText("允许鼠标、键盘和滚轮交互"), propertiesCard);
    interactiveCheck->setObjectName(QStringLiteral("zzRadialEditor_interactiveCheck"));
    interactiveCheck->setChecked(gauge->isInteractive());

    auto *valueVisibleCheck = new QCheckBox(zzGaugeText("显示数值"), propertiesCard);
    valueVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_valueVisibleCheck"));
    valueVisibleCheck->setChecked(gauge->isValueVisible());

    auto *progressGradientEnabledCheck = new QCheckBox(zzGaugeText("进度环使用渐变色"), propertiesCard);
    progressGradientEnabledCheck->setObjectName(
        QStringLiteral("zzRadialEditor_progressGradientEnabledCheck"));
    progressGradientEnabledCheck->setChecked(gauge->isProgressGradientEnabled());

    auto *sweepAreaVisibleCheck = new QCheckBox(zzGaugeText("显示指针扫过扇形"), propertiesCard);
    sweepAreaVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_sweepAreaVisibleCheck"));
    sweepAreaVisibleCheck->setChecked(gauge->isSweepAreaVisible());

    auto *valuePositionCombo = new QComboBox(propertiesCard);
    valuePositionCombo->setObjectName(QStringLiteral("zzRadialEditor_valuePositionCombo"));
    valuePositionCombo->addItem(zzGaugeText("中心"), ZzRadialGauge::CenterValue);
    valuePositionCombo->addItem(zzGaugeText("底部"), ZzRadialGauge::BottomValue);
    valuePositionCombo->setCurrentIndex(valuePositionCombo->findData(gauge->valuePosition()));

    auto *titleEdit = new QLineEdit(gauge->title(), propertiesCard);
    titleEdit->setObjectName(QStringLiteral("zzRadialEditor_titleEdit"));
    auto *unitEdit = new QLineEdit(gauge->unit(), propertiesCard);
    unitEdit->setObjectName(QStringLiteral("zzRadialEditor_unitEdit"));

    auto *labelsVisibleCheck = new QCheckBox(zzGaugeText("显示刻度数值"), propertiesCard);
    labelsVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_labelsVisibleCheck"));
    labelsVisibleCheck->setChecked(gauge->areLabelsVisible());

    auto *hubVisibleCheck = new QCheckBox(zzGaugeText("显示指针轴心"), propertiesCard);
    hubVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_hubVisibleCheck"));
    hubVisibleCheck->setChecked(gauge->isHubVisible());

    auto *needleStyleCombo = new QComboBox(propertiesCard);
    needleStyleCombo->setObjectName(QStringLiteral("zzRadialEditor_needleStyleCombo"));
    needleStyleCombo->addItem(zzGaugeText("无指针"), ZzRadialGauge::NoNeedle);
    needleStyleCombo->addItem(zzGaugeText("线形指针"), ZzRadialGauge::LineNeedle);
    needleStyleCombo->addItem(zzGaugeText("三角指针"), ZzRadialGauge::TriangleNeedle);
    needleStyleCombo->setCurrentIndex(needleStyleCombo->findData(gauge->needleStyle()));

    auto *majorTickCountSlider = makeValueSlider(propertiesCard, 2, 100, gauge->majorTickCount());
    majorTickCountSlider->setObjectName(QStringLiteral("zzRadialEditor_majorTickCountSlider"));
    auto *minorTickCountSlider = makeValueSlider(propertiesCard, 0, 20, gauge->minorTickCount());
    minorTickCountSlider->setObjectName(QStringLiteral("zzRadialEditor_minorTickCountSlider"));

    auto makeMetricSlider = [propertiesCard](qreal value, qreal minimum, qreal maximum) {
        constexpr int scale = 2;
        return makeValueSlider(propertiesCard, qRound(minimum * scale), qRound(maximum * scale),
            qRound(value * scale), 1, 4, scale, 1);
    };

    auto *scaleWidthSlider = makeMetricSlider(gauge->scaleWidth(), 0.5, 32.0);
    scaleWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_scaleWidthSlider"));
    auto *minimumAngleSlider
        = makeValueSlider(propertiesCard, -360, 360, qRound(gauge->minimumAngle()), 5, 15);
    minimumAngleSlider->setObjectName(QStringLiteral("zzRadialEditor_minimumAngleSlider"));
    auto *maximumAngleSlider
        = makeValueSlider(propertiesCard, -360, 360, qRound(gauge->maximumAngle()), 5, 15);
    maximumAngleSlider->setObjectName(QStringLiteral("zzRadialEditor_maximumAngleSlider"));
    auto *needleWidthSlider = makeMetricSlider(gauge->needleWidth(), 0.5, 24.0);
    needleWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_needleWidthSlider"));
    auto *needleLengthSlider
        = makeValueSlider(propertiesCard, 5, 100, qRound(gauge->needleLength() * 100.0), 1, 5, 100, 2);
    needleLengthSlider->setObjectName(QStringLiteral("zzRadialEditor_needleLengthSlider"));
    auto *tickLengthSlider = makeMetricSlider(gauge->tickLength(), 0.0, 40.0);
    tickLengthSlider->setObjectName(QStringLiteral("zzRadialEditor_tickLengthSlider"));
    auto *tickWidthSlider = makeMetricSlider(gauge->tickWidth(), 0.5, 16.0);
    tickWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_tickWidthSlider"));
    auto *majorTickLengthSlider = makeMetricSlider(gauge->majorTickLength(), 0.0, 40.0);
    majorTickLengthSlider->setObjectName(QStringLiteral("zzRadialEditor_majorTickLengthSlider"));
    auto *majorTickWidthSlider = makeMetricSlider(gauge->majorTickWidth(), 0.5, 16.0);
    majorTickWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_majorTickWidthSlider"));
    auto *scalePaddingSlider = makeMetricSlider(gauge->scalePadding(), 0.0, 80.0);
    scalePaddingSlider->setObjectName(QStringLiteral("zzRadialEditor_scalePaddingSlider"));
    auto *tickPaddingSlider = makeMetricSlider(gauge->tickPadding(), 0.0, 60.0);
    tickPaddingSlider->setObjectName(QStringLiteral("zzRadialEditor_tickPaddingSlider"));
    auto *labelPaddingSlider = makeMetricSlider(gauge->labelPadding(), 0.0, 80.0);
    labelPaddingSlider->setObjectName(QStringLiteral("zzRadialEditor_labelPaddingSlider"));
    auto *labelFontPixelSizeSlider = makeValueSlider(propertiesCard, 6, 48, gauge->labelFontPixelSize());
    labelFontPixelSizeSlider->setObjectName(QStringLiteral("zzRadialEditor_labelFontPixelSizeSlider"));
    auto *hubRadiusSlider = makeMetricSlider(gauge->hubRadius(), 0.5, 30.0);
    hubRadiusSlider->setObjectName(QStringLiteral("zzRadialEditor_hubRadiusSlider"));
    auto *valueFontPixelSizeSlider = makeValueSlider(propertiesCard, 0, 72, gauge->valueFontPixelSize());
    valueFontPixelSizeSlider->setObjectName(QStringLiteral("zzRadialEditor_valueFontPixelSizeSlider"));
    auto *sweepAreaOpacitySlider
        = makeValueSlider(propertiesCard, 0, 100, qRound(gauge->sweepAreaOpacity() * 100.0), 1, 10, 100, 2);
    sweepAreaOpacitySlider->setObjectName(QStringLiteral("zzRadialEditor_sweepAreaOpacitySlider"));

    auto *firstRangeFromSlider
        = makeValueSlider(propertiesCard, gauge->minimum(), gauge->maximum(), firstRange->fromValue());
    firstRangeFromSlider->setObjectName(QStringLiteral("zzRadialEditor_firstRangeFromSlider"));
    auto *firstRangeToSlider
        = makeValueSlider(propertiesCard, gauge->minimum(), gauge->maximum(), firstRange->toValue());
    firstRangeToSlider->setObjectName(QStringLiteral("zzRadialEditor_firstRangeToSlider"));
    auto *secondRangeFromSlider
        = makeValueSlider(propertiesCard, gauge->minimum(), gauge->maximum(), secondRange->fromValue());
    secondRangeFromSlider->setObjectName(QStringLiteral("zzRadialEditor_secondRangeFromSlider"));
    auto *secondRangeToSlider
        = makeValueSlider(propertiesCard, gauge->minimum(), gauge->maximum(), secondRange->toValue());
    secondRangeToSlider->setObjectName(QStringLiteral("zzRadialEditor_secondRangeToSlider"));
    auto *thirdRangeFromSlider
        = makeValueSlider(propertiesCard, gauge->minimum(), gauge->maximum(), thirdRange->fromValue());
    thirdRangeFromSlider->setObjectName(QStringLiteral("zzRadialEditor_thirdRangeFromSlider"));
    auto *thirdRangeToSlider
        = makeValueSlider(propertiesCard, gauge->minimum(), gauge->maximum(), thirdRange->toValue());
    thirdRangeToSlider->setObjectName(QStringLiteral("zzRadialEditor_thirdRangeToSlider"));

    auto *accentColorButton = new ZzExampleColorButton(propertiesCard);
    accentColorButton->setObjectName(QStringLiteral("zzRadialEditor_accentColorButton"));
    bindGaugeColorButton(accentColorButton, gauge, nullptr, QPalette::Accent);
    auto *trackColorButton = new ZzExampleColorButton(propertiesCard);
    trackColorButton->setObjectName(QStringLiteral("zzRadialEditor_trackColorButton"));
    bindGaugeColorButton(trackColorButton, gauge, nullptr, QPalette::Mid);
    auto *needleColorButton = new ZzExampleColorButton(propertiesCard);
    needleColorButton->setObjectName(QStringLiteral("zzRadialEditor_needleColorButton"));
    bindGaugeColorButton(needleColorButton, gauge, "needleColor", QPalette::Accent);
    auto *tickColorButton = new ZzExampleColorButton(propertiesCard);
    tickColorButton->setObjectName(QStringLiteral("zzRadialEditor_tickColorButton"));
    bindGaugeColorButton(tickColorButton, gauge, "tickColor", QPalette::Text);
    auto *labelColorButton = new ZzExampleColorButton(propertiesCard);
    labelColorButton->setObjectName(QStringLiteral("zzRadialEditor_labelColorButton"));
    bindGaugeColorButton(labelColorButton, gauge, "labelColor", QPalette::Text);
    auto *valueColorButton = new ZzExampleColorButton(propertiesCard);
    valueColorButton->setObjectName(QStringLiteral("zzRadialEditor_valueColorButton"));
    bindGaugeColorButton(valueColorButton, gauge, "valueColor", QPalette::Text);
    auto *progressGradientStartColorButton = new ZzExampleColorButton(propertiesCard);
    progressGradientStartColorButton->setObjectName(
        QStringLiteral("zzRadialEditor_progressGradientStartColorButton"));
    bindGaugeColorButton(
        progressGradientStartColorButton, gauge, "progressGradientStartColor", QPalette::Accent, 135);
    auto *progressGradientEndColorButton = new ZzExampleColorButton(propertiesCard);
    progressGradientEndColorButton->setObjectName(
        QStringLiteral("zzRadialEditor_progressGradientEndColorButton"));
    bindGaugeColorButton(progressGradientEndColorButton, gauge, "progressGradientEndColor", QPalette::Accent);
    auto *firstRangeColorButton = new ZzExampleColorButton(propertiesCard);
    firstRangeColorButton->setObjectName(QStringLiteral("zzRadialEditor_firstRangeColorButton"));
    firstRangeColorButton->setSelectedColor(firstRange->color());
    auto *secondRangeColorButton = new ZzExampleColorButton(propertiesCard);
    secondRangeColorButton->setObjectName(QStringLiteral("zzRadialEditor_secondRangeColorButton"));
    secondRangeColorButton->setSelectedColor(secondRange->color());
    auto *thirdRangeColorButton = new ZzExampleColorButton(propertiesCard);
    thirdRangeColorButton->setObjectName(QStringLiteral("zzRadialEditor_thirdRangeColorButton"));
    thirdRangeColorButton->setSelectedColor(thirdRange->color());

    auto *disabledCheck = new QCheckBox(zzGaugeText("禁用状态"), propertiesCard);
    disabledCheck->setObjectName(QStringLiteral("zzRadialEditor_disabledCheck"));
    auto *resetButton = new QPushButton(zzGaugeText("恢复默认属性"), propertiesCard);
    resetButton->setObjectName(QStringLiteral("zzRadialEditor_resetButton"));

    basicForm->addRow(zzGaugeText("Scale 模式"), scaleModeCombo);
    basicForm->addRow(zzGaugeText("数值"), valueSlider);
    basicForm->addRow(zzGaugeText("数值动画时长"), valueAnimationDurationSlider);
    basicForm->addRow(interactiveCheck);
    basicForm->addRow(disabledCheck);
    basicForm->addRow(zzGaugeText("刻度环宽度"), scaleWidthSlider);
    basicForm->addRow(zzGaugeText("起始角度"), minimumAngleSlider);
    basicForm->addRow(zzGaugeText("结束角度"), maximumAngleSlider);
    basicForm->addRow(zzGaugeText("外圈边距"), scalePaddingSlider);
    basicForm->addRow(zzGaugeText("强调色"), accentColorButton);
    basicForm->addRow(zzGaugeText("Track 颜色"), trackColorButton);
    basicForm->addRow(resetButton);

    scaleForm->addRow(zzGaugeText("主刻度数量"), majorTickCountSlider);
    scaleForm->addRow(zzGaugeText("每段次刻度数量"), minorTickCountSlider);
    scaleForm->addRow(zzGaugeText("次刻度长度"), tickLengthSlider);
    scaleForm->addRow(zzGaugeText("次刻度宽度"), tickWidthSlider);
    scaleForm->addRow(zzGaugeText("主刻度长度"), majorTickLengthSlider);
    scaleForm->addRow(zzGaugeText("主刻度宽度"), majorTickWidthSlider);
    scaleForm->addRow(zzGaugeText("刻度边距"), tickPaddingSlider);
    scaleForm->addRow(zzGaugeText("刻线颜色"), tickColorButton);
    scaleForm->addRow(labelsVisibleCheck);
    scaleForm->addRow(zzGaugeText("标签边距"), labelPaddingSlider);
    scaleForm->addRow(zzGaugeText("标签字号"), labelFontPixelSizeSlider);
    scaleForm->addRow(zzGaugeText("标签颜色"), labelColorButton);
    scaleForm->addRow(zzGaugeText("Track 端点"), trackCapStyleCombo);
    scaleForm->addRow(zzGaugeText("环端点"), ringCapStyleCombo);

    needleForm->addRow(zzGaugeText("指针样式"), needleStyleCombo);
    needleForm->addRow(zzGaugeText("指针宽度"), needleWidthSlider);
    needleForm->addRow(zzGaugeText("指针长度比例"), needleLengthSlider);
    needleForm->addRow(zzGaugeText("指针颜色"), needleColorButton);
    needleForm->addRow(hubVisibleCheck);
    needleForm->addRow(zzGaugeText("轴心半径"), hubRadiusSlider);
    needleForm->addRow(valueVisibleCheck);
    needleForm->addRow(zzGaugeText("数值位置"), valuePositionCombo);
    needleForm->addRow(zzGaugeText("标题"), titleEdit);
    needleForm->addRow(zzGaugeText("单位"), unitEdit);
    needleForm->addRow(zzGaugeText("数值字号"), valueFontPixelSizeSlider);
    needleForm->addRow(zzGaugeText("数值颜色"), valueColorButton);

    gradientForm->addRow(progressGradientEnabledCheck);
    gradientForm->addRow(sweepAreaVisibleCheck);
    gradientForm->addRow(zzGaugeText("扇形不透明度"), sweepAreaOpacitySlider);
    gradientForm->addRow(zzGaugeText("起点颜色"), progressGradientStartColorButton);
    gradientForm->addRow(zzGaugeText("终点颜色"), progressGradientEndColorButton);

    rangeForm->addRow(zzGaugeText("区间 1 起点"), firstRangeFromSlider);
    rangeForm->addRow(zzGaugeText("区间 1 终点"), firstRangeToSlider);
    rangeForm->addRow(zzGaugeText("区间 1 颜色"), firstRangeColorButton);
    rangeForm->addRow(zzGaugeText("区间 2 起点"), secondRangeFromSlider);
    rangeForm->addRow(zzGaugeText("区间 2 终点"), secondRangeToSlider);
    rangeForm->addRow(zzGaugeText("区间 2 颜色"), secondRangeColorButton);
    rangeForm->addRow(zzGaugeText("区间 3 起点"), thirdRangeFromSlider);
    rangeForm->addRow(zzGaugeText("区间 3 终点"), thirdRangeToSlider);
    rangeForm->addRow(zzGaugeText("区间 3 颜色"), thirdRangeColorButton);

    previewLayout->addWidget(propertyTabs, 1);
    gaugePropertyLayout->addLayout(previewLayout);

    auto *angleHint = new QLabel(
        zzGaugeText("角度以正上方为 0°，顺时针为正；起止角度相同表示完整的 360°。"), propertiesCard);
    angleHint->setWordWrap(true);
    gaugePropertyLayout->addWidget(angleHint);

    const auto updateRangeEditorState = [=] {
        const bool enabled = gauge->scaleMode() == ZzRadialGauge::RangeScale;
        firstRangeFromSlider->setEnabled(enabled);
        firstRangeToSlider->setEnabled(enabled);
        firstRangeColorButton->setEnabled(enabled);
        secondRangeFromSlider->setEnabled(enabled);
        secondRangeToSlider->setEnabled(enabled);
        secondRangeColorButton->setEnabled(enabled);
        thirdRangeFromSlider->setEnabled(enabled);
        thirdRangeToSlider->setEnabled(enabled);
        thirdRangeColorButton->setEnabled(enabled);
    };
    const auto updateGradientEditorState = [=] {
        const bool progressMode = gauge->scaleMode() == ZzRadialGauge::ProgressScale;
        progressGradientEnabledCheck->setEnabled(progressMode);
        sweepAreaVisibleCheck->setEnabled(progressMode);
        sweepAreaOpacitySlider->setEnabled(progressMode && sweepAreaVisibleCheck->isChecked());
        const bool colorsEnabled = progressMode
            && (progressGradientEnabledCheck->isChecked() || sweepAreaVisibleCheck->isChecked());
        progressGradientStartColorButton->setEnabled(colorsEnabled);
        progressGradientEndColorButton->setEnabled(colorsEnabled);
    };
    const auto updateLabelEditorState = [=] {
        const bool enabled = labelsVisibleCheck->isChecked();
        labelPaddingSlider->setEnabled(enabled);
        labelFontPixelSizeSlider->setEnabled(enabled);
        labelColorButton->setEnabled(enabled);
    };
    const auto updateNeedleEditorState = [=] {
        const auto style = static_cast<ZzRadialGauge::ZzNeedleStyle>(needleStyleCombo->currentData().toInt());
        const bool needleEnabled = style != ZzRadialGauge::NoNeedle;
        needleWidthSlider->setEnabled(needleEnabled);
        needleLengthSlider->setEnabled(needleEnabled);
        needleColorButton->setEnabled(needleEnabled);
        hubVisibleCheck->setEnabled(needleEnabled);
        hubRadiusSlider->setEnabled(needleEnabled && hubVisibleCheck->isChecked());
    };
    const auto updateValueEditorState = [=] {
        const bool enabled = valueVisibleCheck->isChecked();
        valuePositionCombo->setEnabled(enabled);
        titleEdit->setEnabled(enabled);
        unitEdit->setEnabled(enabled);
        valueFontPixelSizeSlider->setEnabled(enabled);
        valueColorButton->setEnabled(enabled);
    };

    QObject::connect(scaleModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), gauge, [=](int index) {
        gauge->setScaleMode(static_cast<ZzRadialGauge::ZzGaugeScaleMode>(scaleModeCombo->itemData(index).toInt()));
        updateRangeEditorState();
        updateGradientEditorState();
    });
    QObject::connect(valueSlider, &QSlider::valueChanged, gauge, &ZzRadialGauge::setValue);
    QObject::connect(gauge, &QDial::valueChanged, valueSlider, [gauge, valueSlider](int value) {
        if (valueSlider->isSliderDown() || gauge->isValueAnimating()) {
            return;
        }
        const QSignalBlocker blocker(valueSlider);
        valueSlider->setValue(value);
    });
    QObject::connect(valueAnimationDurationSlider, &QSlider::valueChanged, gauge,
        &ZzRadialGauge::setValueAnimationDuration);
    QObject::connect(interactiveCheck, &QCheckBox::toggled, gauge, &ZzRadialGauge::setInteractive);
    QObject::connect(
        progressGradientEnabledCheck, &QCheckBox::toggled, gauge, &ZzRadialGauge::setProgressGradientEnabled);
    QObject::connect(
        progressGradientEnabledCheck, &QCheckBox::toggled, gauge, [=] { updateGradientEditorState(); });
    QObject::connect(sweepAreaVisibleCheck, &QCheckBox::toggled, gauge, &ZzRadialGauge::setSweepAreaVisible);
    QObject::connect(sweepAreaVisibleCheck, &QCheckBox::toggled, gauge, [=] { updateGradientEditorState(); });
    connectScaledSlider(sweepAreaOpacitySlider, gauge, 100.0, &ZzRadialGauge::setSweepAreaOpacity);
    QObject::connect(valueVisibleCheck, &QCheckBox::toggled, gauge, &ZzRadialGauge::setValueVisible);
    QObject::connect(valueVisibleCheck, &QCheckBox::toggled, gauge, [=] { updateValueEditorState(); });
    QObject::connect(
        valuePositionCombo, qOverload<int>(&QComboBox::currentIndexChanged), gauge, [=](int index) {
            gauge->setValuePosition(
                static_cast<ZzRadialGauge::ZzGaugeValuePosition>(valuePositionCombo->itemData(index).toInt()));
        });
    QObject::connect(titleEdit, &QLineEdit::textChanged, gauge, &ZzRadialGauge::setTitle);
    QObject::connect(unitEdit, &QLineEdit::textChanged, gauge, &ZzRadialGauge::setUnit);
    QObject::connect(
        valueFontPixelSizeSlider, &QSlider::valueChanged, gauge, &ZzRadialGauge::setValueFontPixelSize);
    QObject::connect(majorTickCountSlider, &QSlider::valueChanged, gauge, &ZzRadialGauge::setMajorTickCount);
    QObject::connect(minorTickCountSlider, &QSlider::valueChanged, gauge, &ZzRadialGauge::setMinorTickCount);
    connectScaledSlider(scaleWidthSlider, gauge, 2.0, &ZzRadialGauge::setScaleWidth);
    connectScaledSlider(minimumAngleSlider, gauge, 1.0, &ZzRadialGauge::setMinimumAngle);
    connectScaledSlider(maximumAngleSlider, gauge, 1.0, &ZzRadialGauge::setMaximumAngle);
    connectScaledSlider(needleWidthSlider, gauge, 2.0, &ZzRadialGauge::setNeedleWidth);
    connectScaledSlider(needleLengthSlider, gauge, 100.0, &ZzRadialGauge::setNeedleLength);
    connectScaledSlider(tickLengthSlider, gauge, 2.0, &ZzRadialGauge::setTickLength);
    connectScaledSlider(tickWidthSlider, gauge, 2.0, &ZzRadialGauge::setTickWidth);
    connectScaledSlider(majorTickLengthSlider, gauge, 2.0, &ZzRadialGauge::setMajorTickLength);
    connectScaledSlider(majorTickWidthSlider, gauge, 2.0, &ZzRadialGauge::setMajorTickWidth);
    connectScaledSlider(scalePaddingSlider, gauge, 2.0, &ZzRadialGauge::setScalePadding);
    connectScaledSlider(tickPaddingSlider, gauge, 2.0, &ZzRadialGauge::setTickPadding);
    QObject::connect(
        trackCapStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged), gauge, [=](int index) {
            gauge->setTrackCapStyle(
                static_cast<Qt::PenCapStyle>(trackCapStyleCombo->itemData(index).toInt()));
        });
    QObject::connect(
        ringCapStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged), gauge, [=](int index) {
            gauge->setRingCapStyle(static_cast<Qt::PenCapStyle>(ringCapStyleCombo->itemData(index).toInt()));
        });
    QObject::connect(gauge, &ZzRadialGauge::trackCapStyleChanged, trackCapStyleCombo,
        [trackCapStyleCombo](Qt::PenCapStyle style) {
            const QSignalBlocker blocker(trackCapStyleCombo);
            trackCapStyleCombo->setCurrentIndex(trackCapStyleCombo->findData(style));
        });
    QObject::connect(gauge, &ZzRadialGauge::ringCapStyleChanged, ringCapStyleCombo,
        [ringCapStyleCombo](Qt::PenCapStyle style) {
            const QSignalBlocker blocker(ringCapStyleCombo);
            ringCapStyleCombo->setCurrentIndex(ringCapStyleCombo->findData(style));
        });
    QObject::connect(labelsVisibleCheck, &QCheckBox::toggled, gauge, &ZzRadialGauge::setLabelsVisible);
    QObject::connect(labelsVisibleCheck, &QCheckBox::toggled, gauge, [=] { updateLabelEditorState(); });
    connectScaledSlider(labelPaddingSlider, gauge, 2.0, &ZzRadialGauge::setLabelPadding);
    QObject::connect(
        labelFontPixelSizeSlider, &QSlider::valueChanged, gauge, &ZzRadialGauge::setLabelFontPixelSize);
    QObject::connect(
        needleStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged), gauge, [=](int index) {
            gauge->setNeedleStyle(
                static_cast<ZzRadialGauge::ZzNeedleStyle>(needleStyleCombo->itemData(index).toInt()));
            updateNeedleEditorState();
        });
    QObject::connect(hubVisibleCheck, &QCheckBox::toggled, gauge, &ZzRadialGauge::setHubVisible);
    QObject::connect(hubVisibleCheck, &QCheckBox::toggled, gauge, [=] { updateNeedleEditorState(); });
    connectScaledSlider(hubRadiusSlider, gauge, 2.0, &ZzRadialGauge::setHubRadius);
    QObject::connect(
        firstRangeFromSlider, &QSlider::valueChanged, firstRange, &ZzRadialGaugeRange::setFromValue);
    QObject::connect(firstRangeToSlider, &QSlider::valueChanged, firstRange, &ZzRadialGaugeRange::setToValue);
    QObject::connect(
        secondRangeFromSlider, &QSlider::valueChanged, secondRange, &ZzRadialGaugeRange::setFromValue);
    QObject::connect(
        secondRangeToSlider, &QSlider::valueChanged, secondRange, &ZzRadialGaugeRange::setToValue);
    QObject::connect(
        thirdRangeFromSlider, &QSlider::valueChanged, thirdRange, &ZzRadialGaugeRange::setFromValue);
    QObject::connect(thirdRangeToSlider, &QSlider::valueChanged, thirdRange, &ZzRadialGaugeRange::setToValue);
    QObject::connect(accentColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        [gauge](const QColor &color) { setGaugeAccentColor(gauge, color); });
    QObject::connect(trackColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        [gauge](const QColor &color) { setGaugeTrackColor(gauge, color); });
    QObject::connect(needleColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        [gauge](const QColor &color) { gauge->setNeedleColor(color); });
    QObject::connect(tickColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        [gauge](const QColor &color) { gauge->setTickColor(color); });
    QObject::connect(labelColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        [gauge](const QColor &color) { gauge->setLabelColor(color); });
    QObject::connect(valueColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        [gauge](const QColor &color) { gauge->setValueColor(color); });
    QObject::connect(progressGradientStartColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        &ZzRadialGauge::setProgressGradientStartColor);
    QObject::connect(progressGradientEndColorButton, &ZzExampleColorButton::selectedColorChanged, gauge,
        &ZzRadialGauge::setProgressGradientEndColor);
    QObject::connect(firstRangeColorButton, &ZzExampleColorButton::selectedColorChanged, firstRange,
        [firstRange](const QColor &color) { firstRange->setColor(color); });
    QObject::connect(secondRangeColorButton, &ZzExampleColorButton::selectedColorChanged, secondRange,
        [secondRange](const QColor &color) { secondRange->setColor(color); });
    QObject::connect(thirdRangeColorButton, &ZzExampleColorButton::selectedColorChanged, thirdRange,
        [thirdRange](const QColor &color) { thirdRange->setColor(color); });
    QObject::connect(
        disabledCheck, &QCheckBox::toggled, gauge, [gauge](bool disabled) { gauge->setEnabled(!disabled); });
    QObject::connect(resetButton, &QPushButton::clicked, gauge, [=] {
        scaleModeCombo->setCurrentIndex(scaleModeCombo->findData(ZzRadialGauge::ProgressScale));
        valueAnimationDurationSlider->setValue(500);
        valueSlider->setValue(210);
        interactiveCheck->setChecked(true);
        progressGradientEnabledCheck->setChecked(false);
        sweepAreaVisibleCheck->setChecked(false);
        sweepAreaOpacitySlider->setValue(16);
        valueVisibleCheck->setChecked(true);
        valuePositionCombo->setCurrentIndex(valuePositionCombo->findData(ZzRadialGauge::BottomValue));
        titleEdit->clear();
        unitEdit->clear();
        valueFontPixelSizeSlider->setValue(0);
        majorTickCountSlider->setValue(10);
        minorTickCountSlider->setValue(0);
        scaleWidthSlider->setValue(16);
        minimumAngleSlider->setValue(-135);
        maximumAngleSlider->setValue(135);
        needleWidthSlider->setValue(10);
        needleLengthSlider->setValue(62);
        tickLengthSlider->setValue(14);
        tickWidthSlider->setValue(3);
        majorTickLengthSlider->setValue(14);
        majorTickWidthSlider->setValue(4);
        scalePaddingSlider->setValue(24);
        trackCapStyleCombo->setCurrentIndex(trackCapStyleCombo->findData(Qt::RoundCap));
        ringCapStyleCombo->setCurrentIndex(ringCapStyleCombo->findData(Qt::RoundCap));
        tickPaddingSlider->setValue(16);
        labelsVisibleCheck->setChecked(false);
        labelPaddingSlider->setValue(56);
        labelFontPixelSizeSlider->setValue(11);
        needleStyleCombo->setCurrentIndex(needleStyleCombo->findData(ZzRadialGauge::LineNeedle));
        hubVisibleCheck->setChecked(false);
        hubRadiusSlider->setValue(22);
        firstRangeFromSlider->setValue(0);
        firstRangeToSlider->setValue(80);
        secondRangeFromSlider->setValue(80);
        secondRangeToSlider->setValue(160);
        thirdRangeFromSlider->setValue(160);
        thirdRangeToSlider->setValue(240);
        disabledCheck->setChecked(false);

        gauge->setPalette(QPalette());
        gauge->setNeedleColor(QColor());
        gauge->setTickColor(QColor());
        gauge->setLabelColor(QColor());
        gauge->setValueColor(QColor());
        gauge->setProgressGradientStartColor(QColor());
        gauge->setProgressGradientEndColor(QColor());
        firstRange->setColor(firstRangeDefaultColor);
        secondRange->setColor(secondRangeDefaultColor);
        thirdRange->setColor(thirdRangeDefaultColor);
        const QSignalBlocker accentBlocker(accentColorButton);
        const QSignalBlocker trackBlocker(trackColorButton);
        const QSignalBlocker needleBlocker(needleColorButton);
        const QSignalBlocker tickBlocker(tickColorButton);
        const QSignalBlocker labelBlocker(labelColorButton);
        const QSignalBlocker valueBlocker(valueColorButton);
        const QSignalBlocker progressGradientStartBlocker(progressGradientStartColorButton);
        const QSignalBlocker progressGradientEndBlocker(progressGradientEndColorButton);
        const QSignalBlocker firstRangeBlocker(firstRangeColorButton);
        const QSignalBlocker secondRangeBlocker(secondRangeColorButton);
        const QSignalBlocker thirdRangeBlocker(thirdRangeColorButton);
        accentColorButton->setSelectedColor(gaugeAccentColor(gauge));
        trackColorButton->setSelectedColor(gauge->palette().color(QPalette::Active, QPalette::Mid));
        needleColorButton->setSelectedColor(gaugeAccentColor(gauge));
        tickColorButton->setSelectedColor(gauge->palette().color(QPalette::Active, QPalette::Text));
        labelColorButton->setSelectedColor(gauge->palette().color(QPalette::Active, QPalette::Text));
        valueColorButton->setSelectedColor(gauge->palette().color(QPalette::Active, QPalette::Text));
        progressGradientStartColorButton->setSelectedColor(gaugeAccentColor(gauge).lighter(135));
        progressGradientEndColorButton->setSelectedColor(gaugeAccentColor(gauge));
        firstRangeColorButton->setSelectedColor(firstRangeDefaultColor);
        secondRangeColorButton->setSelectedColor(secondRangeDefaultColor);
        thirdRangeColorButton->setSelectedColor(thirdRangeDefaultColor);
        updateRangeEditorState();
        updateGradientEditorState();
        updateLabelEditorState();
        updateNeedleEditorState();
        updateValueEditorState();
        gauge->update();
    });

    updateRangeEditorState();
    updateGradientEditorState();
    updateLabelEditorState();
    updateNeedleEditorState();
    updateValueEditorState();

    auto *code = new QPlainTextEdit(propertiesCard);
    code->setReadOnly(true);
    code->setMaximumHeight(150);
    code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    code->setPlainText(QStringLiteral("auto *gauge = new ZzRadialGauge(parent);\n"
                                      "gauge->setRange(0, 100);\n"
                                      "gauge->setValue(70);\n"
                                      "gauge->setMinimumAngle(-135.0);\n"
                                      "gauge->setMaximumAngle(135.0);\n"
                                      "gauge->setScaleMode(ZzRadialGauge::RangeScale);\n"
                                      "gauge->addRange(0, 60, QColor(\"#21BCE2\"));\n"
                                      "gauge->addRange(60, 80, QColor(\"#FFB900\"));\n"
                                      "gauge->addRange(80, 100, QColor(\"#FF6475\"));"));
    gaugePropertyLayout->addWidget(code);
}

void buildRadialGaugeEditor(QWidget *propertiesCard, QWidget *gaugePropertyPage, QVBoxLayout *gaugePropertyLayout)
{
    ZzExampleRadialGaugeEditor::buildRadialGaugeEditor(propertiesCard, gaugePropertyPage, gaugePropertyLayout);
}

} // namespace ZzExample
