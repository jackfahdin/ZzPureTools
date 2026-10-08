#include "ZzExampleRadialGaugeHelpers.h"

namespace ZzExample {
void buildRadialRingEditor(QWidget *ringPropertyPage, QVBoxLayout *ringPropertyLayout)
{
    auto *ringPreviewLayout = new QHBoxLayout;
    ringPreviewLayout->setSpacing(32);

    auto *ring = new ZzMultiProgressRing(ringPropertyPage);
    ring->setObjectName(QStringLiteral("multiProgressRingPreview"));
    ring->setFixedSize(300, 300);
    ring->setRingWidth(8.0);
    ring->setRingSpacing(6.0);
    ring->setRingPadding(13.0);
    ring->setCapStyle(Qt::RoundCap);
    ring->setTrackVisible(false);
    const QColor perfectDefaultColor(QStringLiteral("#5470C6"));
    const QColor goodDefaultColor(QStringLiteral("#B8DE29"));
    const QColor commonDefaultColor(QStringLiteral("#5C5F7A"));
    auto *perfectItem = ring->addItem(QStringLiteral("Perfect"), 20.0, perfectDefaultColor);
    auto *goodItem = ring->addItem(QStringLiteral("Good"), 40.0, goodDefaultColor);
    auto *commonItem = ring->addItem(QStringLiteral("Commonly"), 60.0, commonDefaultColor);
    auto *ringPreviewHost = new QWidget(ringPropertyPage);
    auto *ringPreviewHostLayout = new QVBoxLayout(ringPreviewHost);
    ringPreviewHostLayout->setContentsMargins(0, 0, 0, 0);
    ringPreviewHostLayout->addWidget(ring, 0, Qt::AlignHCenter | Qt::AlignTop);
    ringPreviewHostLayout->addStretch();
    ringPreviewLayout->addWidget(ringPreviewHost, 1);

    auto *ringEditorTabs = makePropertyTabs(ringPropertyPage);
    ringEditorTabs->setObjectName(QStringLiteral("zzRadialEditor_ringEditorTabs"));

    ringEditorTabs->setMinimumWidth(440);
    ringEditorTabs->setMinimumHeight(430);

    const auto makeRingPropertyForm = [ringEditorTabs](QFormLayout *&form) {
        auto *page = new QWidget(ringEditorTabs);
        form = new QFormLayout(page);
        form->setContentsMargins(12, 12, 12, 12);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);
        return page;
    };

    QFormLayout *ringLayoutForm = nullptr;
    QFormLayout *ringDataForm = nullptr;
    QFormLayout *ringDetailsForm = nullptr;
    auto *ringLayoutPage = makeRingPropertyForm(ringLayoutForm);
    auto *ringDataPage = makeRingPropertyForm(ringDataForm);
    auto *ringDetailsPage = makeRingPropertyForm(ringDetailsForm);
    ringEditorTabs->addTab(ringLayoutPage, zzGaugeText("环与布局"));
    ringEditorTabs->addTab(ringDataPage, zzGaugeText("数据"));
    ringEditorTabs->addTab(ringDetailsPage, zzGaugeText("详情"));

    const auto makeRingMetricSlider = [ringPropertyPage](qreal value, qreal minimum, qreal maximum) {
        constexpr int scale = 2;
        return makeValueSlider(ringPropertyPage, qRound(minimum * scale), qRound(maximum * scale),
            qRound(value * scale), 1, 4, scale, 1);
    };

    auto *ringMinimumSlider = makeValueSlider(ringPropertyPage, -100, 100, qRound(ring->minimum()));
    ringMinimumSlider->setObjectName(QStringLiteral("zzRadialEditor_ringMinimumSlider"));
    auto *ringMaximumSlider = makeValueSlider(ringPropertyPage, 1, 200, qRound(ring->maximum()));
    ringMaximumSlider->setObjectName(QStringLiteral("zzRadialEditor_ringMaximumSlider"));
    auto *ringStartAngleSlider
        = makeValueSlider(ringPropertyPage, -360, 360, qRound(ring->startAngle()), 5, 15);
    ringStartAngleSlider->setObjectName(QStringLiteral("zzRadialEditor_ringStartAngleSlider"));
    auto *ringSweepAngleSlider = makeValueSlider(ringPropertyPage, 0, 360, qRound(ring->sweepAngle()), 5, 15);
    ringSweepAngleSlider->setObjectName(QStringLiteral("zzRadialEditor_ringSweepAngleSlider"));
    auto *ringWidthSlider = makeRingMetricSlider(ring->ringWidth(), 0.5, 32.0);
    ringWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_ringWidthSlider"));
    auto *ringSpacingSlider = makeRingMetricSlider(ring->ringSpacing(), 0.0, 32.0);
    ringSpacingSlider->setObjectName(QStringLiteral("zzRadialEditor_ringSpacingSlider"));
    auto *ringPaddingSlider = makeRingMetricSlider(ring->ringPadding(), 0.0, 80.0);
    ringPaddingSlider->setObjectName(QStringLiteral("zzRadialEditor_ringPaddingSlider"));
    auto *ringAnimationDurationSlider
        = makeValueSlider(ringPropertyPage, 0, 2000, ring->valueAnimationDuration(), 10, 100);
    ringAnimationDurationSlider->setObjectName(QStringLiteral("zzRadialEditor_ringAnimationDurationSlider"));

    auto *scoreRingCapStyleCombo = new QComboBox(ringPropertyPage);
    scoreRingCapStyleCombo->setObjectName(QStringLiteral("zzRadialEditor_scoreRingCapStyleCombo"));
    scoreRingCapStyleCombo->addItem(QStringLiteral("FlatCap"), Qt::FlatCap);
    scoreRingCapStyleCombo->addItem(QStringLiteral("SquareCap"), Qt::SquareCap);
    scoreRingCapStyleCombo->addItem(QStringLiteral("RoundCap"), Qt::RoundCap);
    scoreRingCapStyleCombo->setCurrentIndex(scoreRingCapStyleCombo->findData(ring->capStyle()));

    auto *ringTrackVisibleCheck = new QCheckBox(zzGaugeText("显示 Track"), ringPropertyPage);
    ringTrackVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_ringTrackVisibleCheck"));
    ringTrackVisibleCheck->setChecked(ring->isTrackVisible());
    auto *ringTrackColorButton = new ZzExampleColorButton(ringPropertyPage);
    ringTrackColorButton->setObjectName(QStringLiteral("zzRadialEditor_ringTrackColorButton"));
    bindGaugeColorButton(ringTrackColorButton, ring, "trackColor", QPalette::Mid);
    ringTrackColorButton->setEnabled(ring->isTrackVisible());

    ringLayoutForm->addRow(zzGaugeText("最小值"), ringMinimumSlider);
    ringLayoutForm->addRow(zzGaugeText("最大值"), ringMaximumSlider);
    ringLayoutForm->addRow(zzGaugeText("起始角度"), ringStartAngleSlider);
    ringLayoutForm->addRow(zzGaugeText("扫过角度"), ringSweepAngleSlider);
    ringLayoutForm->addRow(zzGaugeText("环宽度"), ringWidthSlider);
    ringLayoutForm->addRow(zzGaugeText("环间距"), ringSpacingSlider);
    ringLayoutForm->addRow(zzGaugeText("外圈边距"), ringPaddingSlider);
    ringLayoutForm->addRow(zzGaugeText("端点样式"), scoreRingCapStyleCombo);
    ringLayoutForm->addRow(ringTrackVisibleCheck);
    ringLayoutForm->addRow(zzGaugeText("Track 颜色"), ringTrackColorButton);
    ringLayoutForm->addRow(zzGaugeText("数值动画时长"), ringAnimationDurationSlider);

    auto *perfectLabelEdit = new QLineEdit(perfectItem->label(), ringPropertyPage);
    perfectLabelEdit->setObjectName(QStringLiteral("zzRadialEditor_perfectLabelEdit"));
    auto *goodLabelEdit = new QLineEdit(goodItem->label(), ringPropertyPage);
    goodLabelEdit->setObjectName(QStringLiteral("zzRadialEditor_goodLabelEdit"));
    auto *commonLabelEdit = new QLineEdit(commonItem->label(), ringPropertyPage);
    commonLabelEdit->setObjectName(QStringLiteral("zzRadialEditor_commonLabelEdit"));
    auto *perfectValueSlider = makeValueSlider(ringPropertyPage, 0, 100, qRound(perfectItem->value()));
    perfectValueSlider->setObjectName(QStringLiteral("zzRadialEditor_perfectValueSlider"));
    auto *goodValueSlider = makeValueSlider(ringPropertyPage, 0, 100, qRound(goodItem->value()));
    goodValueSlider->setObjectName(QStringLiteral("zzRadialEditor_goodValueSlider"));
    auto *commonValueSlider = makeValueSlider(ringPropertyPage, 0, 100, qRound(commonItem->value()));
    commonValueSlider->setObjectName(QStringLiteral("zzRadialEditor_commonValueSlider"));
    auto *perfectColorButton = new ZzExampleColorButton(ringPropertyPage);
    perfectColorButton->setObjectName(QStringLiteral("zzRadialEditor_perfectColorButton"));
    perfectColorButton->setSelectedColor(perfectItem->color());
    auto *goodColorButton = new ZzExampleColorButton(ringPropertyPage);
    goodColorButton->setObjectName(QStringLiteral("zzRadialEditor_goodColorButton"));
    goodColorButton->setSelectedColor(goodItem->color());
    auto *commonColorButton = new ZzExampleColorButton(ringPropertyPage);
    commonColorButton->setObjectName(QStringLiteral("zzRadialEditor_commonColorButton"));
    commonColorButton->setSelectedColor(commonItem->color());

    ringDataForm->addRow(zzGaugeText("项目 1 名称"), perfectLabelEdit);
    ringDataForm->addRow(zzGaugeText("项目 1 数值"), perfectValueSlider);
    ringDataForm->addRow(zzGaugeText("项目 1 颜色"), perfectColorButton);
    ringDataForm->addRow(zzGaugeText("项目 2 名称"), goodLabelEdit);
    ringDataForm->addRow(zzGaugeText("项目 2 数值"), goodValueSlider);
    ringDataForm->addRow(zzGaugeText("项目 2 颜色"), goodColorButton);
    ringDataForm->addRow(zzGaugeText("项目 3 名称"), commonLabelEdit);
    ringDataForm->addRow(zzGaugeText("项目 3 数值"), commonValueSlider);
    ringDataForm->addRow(zzGaugeText("项目 3 颜色"), commonColorButton);

    auto *ringDetailsVisibleCheck = new QCheckBox(zzGaugeText("显示中央详情"), ringPropertyPage);
    ringDetailsVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_ringDetailsVisibleCheck"));
    ringDetailsVisibleCheck->setChecked(ring->areDetailsVisible());
    auto *ringBadgeVisibleCheck = new QCheckBox(zzGaugeText("数值使用徽标边框"), ringPropertyPage);
    ringBadgeVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_ringBadgeVisibleCheck"));
    ringBadgeVisibleCheck->setChecked(ring->isValueBadgeVisible());
    auto *ringValueSuffixEdit = new QLineEdit(ring->valueSuffix(), ringPropertyPage);
    ringValueSuffixEdit->setObjectName(QStringLiteral("zzRadialEditor_ringValueSuffixEdit"));
    auto *ringValueDecimalsSlider = makeValueSlider(ringPropertyPage, 0, 6, ring->valueDecimals());
    ringValueDecimalsSlider->setObjectName(QStringLiteral("zzRadialEditor_ringValueDecimalsSlider"));
    auto *ringLabelFontSizeSlider = makeValueSlider(ringPropertyPage, 0, 48, ring->labelFontPixelSize());
    ringLabelFontSizeSlider->setObjectName(QStringLiteral("zzRadialEditor_ringLabelFontSizeSlider"));
    auto *ringValueFontSizeSlider = makeValueSlider(ringPropertyPage, 0, 48, ring->valueFontPixelSize());
    ringValueFontSizeSlider->setObjectName(QStringLiteral("zzRadialEditor_ringValueFontSizeSlider"));
    auto *ringLabelColorButton = new ZzExampleColorButton(ringPropertyPage);
    ringLabelColorButton->setObjectName(QStringLiteral("zzRadialEditor_ringLabelColorButton"));
    bindGaugeColorButton(ringLabelColorButton, ring, "labelColor", QPalette::Text);

    ringDetailsForm->addRow(ringDetailsVisibleCheck);
    ringDetailsForm->addRow(ringBadgeVisibleCheck);
    ringDetailsForm->addRow(zzGaugeText("数值后缀"), ringValueSuffixEdit);
    ringDetailsForm->addRow(zzGaugeText("数值小数位"), ringValueDecimalsSlider);
    ringDetailsForm->addRow(zzGaugeText("名称字号（0 自动）"), ringLabelFontSizeSlider);
    ringDetailsForm->addRow(zzGaugeText("数值字号（0 自动）"), ringValueFontSizeSlider);
    ringDetailsForm->addRow(zzGaugeText("名称颜色"), ringLabelColorButton);

    QObject::connect(ringMinimumSlider, &QSlider::valueChanged, ring, [=](int minimum) {
        if (minimum >= ringMaximumSlider->value()) {
            ringMaximumSlider->setValue(minimum + 1);
        }
        ring->setMinimum(minimum);
        perfectValueSlider->setMinimum(minimum);
        goodValueSlider->setMinimum(minimum);
        commonValueSlider->setMinimum(minimum);
    });
    QObject::connect(ringMaximumSlider, &QSlider::valueChanged, ring, [=](int maximum) {
        if (maximum <= ringMinimumSlider->value()) {
            ringMinimumSlider->setValue(maximum - 1);
        }
        ring->setMaximum(maximum);
        perfectValueSlider->setMaximum(maximum);
        goodValueSlider->setMaximum(maximum);
        commonValueSlider->setMaximum(maximum);
    });
    connectScaledSlider(ringStartAngleSlider, ring, 1.0, &ZzMultiProgressRing::setStartAngle);
    connectScaledSlider(ringSweepAngleSlider, ring, 1.0, &ZzMultiProgressRing::setSweepAngle);
    connectScaledSlider(ringWidthSlider, ring, 2.0, &ZzMultiProgressRing::setRingWidth);
    connectScaledSlider(ringSpacingSlider, ring, 2.0, &ZzMultiProgressRing::setRingSpacing);
    connectScaledSlider(ringPaddingSlider, ring, 2.0, &ZzMultiProgressRing::setRingPadding);
    connectScaledSlider(
        ringAnimationDurationSlider, ring, 1.0, &ZzMultiProgressRing::setValueAnimationDuration);
    QObject::connect(scoreRingCapStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged), ring, [=](int) {
        ring->setCapStyle(static_cast<Qt::PenCapStyle>(scoreRingCapStyleCombo->currentData().toInt()));
    });
    QObject::connect(ringTrackVisibleCheck, &QCheckBox::toggled, ring, &ZzMultiProgressRing::setTrackVisible);
    QObject::connect(ringTrackVisibleCheck, &QCheckBox::toggled, ringTrackColorButton, &QWidget::setEnabled);
    QObject::connect(ringTrackColorButton, &ZzExampleColorButton::selectedColorChanged, ring,
        &ZzMultiProgressRing::setTrackColor);

    QObject::connect(
        perfectLabelEdit, &QLineEdit::textChanged, perfectItem, &ZzMultiProgressRingItem::setLabel);
    QObject::connect(goodLabelEdit, &QLineEdit::textChanged, goodItem, &ZzMultiProgressRingItem::setLabel);
    QObject::connect(
        commonLabelEdit, &QLineEdit::textChanged, commonItem, &ZzMultiProgressRingItem::setLabel);
    connectScaledSlider(perfectValueSlider, perfectItem, 1.0, &ZzMultiProgressRingItem::setValue);
    connectScaledSlider(goodValueSlider, goodItem, 1.0, &ZzMultiProgressRingItem::setValue);
    connectScaledSlider(commonValueSlider, commonItem, 1.0, &ZzMultiProgressRingItem::setValue);
    QObject::connect(perfectColorButton, &ZzExampleColorButton::selectedColorChanged, perfectItem,
        &ZzMultiProgressRingItem::setColor);
    QObject::connect(goodColorButton, &ZzExampleColorButton::selectedColorChanged, goodItem,
        &ZzMultiProgressRingItem::setColor);
    QObject::connect(commonColorButton, &ZzExampleColorButton::selectedColorChanged, commonItem,
        &ZzMultiProgressRingItem::setColor);

    QObject::connect(
        ringDetailsVisibleCheck, &QCheckBox::toggled, ring, &ZzMultiProgressRing::setDetailsVisible);
    QObject::connect(
        ringBadgeVisibleCheck, &QCheckBox::toggled, ring, &ZzMultiProgressRing::setValueBadgeVisible);
    QObject::connect(
        ringValueSuffixEdit, &QLineEdit::textChanged, ring, &ZzMultiProgressRing::setValueSuffix);
    connectScaledSlider(ringValueDecimalsSlider, ring, 1.0, &ZzMultiProgressRing::setValueDecimals);
    connectScaledSlider(ringLabelFontSizeSlider, ring, 1.0, &ZzMultiProgressRing::setLabelFontPixelSize);
    connectScaledSlider(ringValueFontSizeSlider, ring, 1.0, &ZzMultiProgressRing::setValueFontPixelSize);
    QObject::connect(ringLabelColorButton, &ZzExampleColorButton::selectedColorChanged, ring,
        &ZzMultiProgressRing::setLabelColor);

    ringPreviewLayout->addWidget(ringEditorTabs, 1);
    ringPropertyLayout->addLayout(ringPreviewLayout);
    auto *ringHint = new QLabel(
        zzGaugeText("每个数据项对应一条独立圆环；数值、名称和颜色均可单独设置，其余属性由控件统一管理。"),
        ringPropertyPage);
    ringHint->setWordWrap(true);
    ringPropertyLayout->addWidget(ringHint);
    ringPropertyLayout->addStretch();
}
} // namespace ZzExample
