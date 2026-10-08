#include "ZzExampleRadialGaugeHelpers.h"

namespace ZzExample {
void buildMultiRadialGaugeEditor(QWidget *multiGaugePropertyPage, QVBoxLayout *multiGaugePropertyLayout)
{
    auto *multiGaugePreviewLayout = new QHBoxLayout;
    multiGaugePreviewLayout->setSpacing(32);

    auto *multiGauge = new ZzMultiRadialGauge(multiGaugePropertyPage);
    multiGauge->setObjectName(QStringLiteral("multiRadialGaugePreview"));
    multiGauge->setFixedSize(300, 300);
    configureEChartsMultiTitleGauge(multiGauge);
    multiGauge->setTrackWidth(9.0);
    multiGauge->setProgressWidth(9.0);
    multiGauge->setNeedleWidth(5.0);
    multiGauge->setTitleFontPixelSize(11);
    multiGauge->setDetailFontPixelSize(11);
    const QColor multiGaugeGoodDefaultColor(QStringLiteral("#5470C6"));
    const QColor multiGaugeBetterDefaultColor(QStringLiteral("#B8DE29"));
    const QColor multiGaugePerfectDefaultColor(QStringLiteral("#555672"));
    auto *multiGaugeGoodItem = multiGauge->addItem(QStringLiteral("Good"), 20.0, multiGaugeGoodDefaultColor);
    auto *multiGaugeBetterItem
        = multiGauge->addItem(QStringLiteral("Better"), 40.0, multiGaugeBetterDefaultColor);
    auto *multiGaugePerfectItem
        = multiGauge->addItem(QStringLiteral("Perfect"), 60.0, multiGaugePerfectDefaultColor);
    multiGaugeGoodItem->setTitleOffset(QPointF(-0.4, 0.8));
    multiGaugeGoodItem->setDetailOffset(QPointF(-0.4, 0.95));
    multiGaugeBetterItem->setTitleOffset(QPointF(0.0, 0.8));
    multiGaugeBetterItem->setDetailOffset(QPointF(0.0, 0.95));
    multiGaugePerfectItem->setTitleOffset(QPointF(0.4, 0.8));
    multiGaugePerfectItem->setDetailOffset(QPointF(0.4, 0.95));

    auto *multiGaugePreviewHost = new QWidget(multiGaugePropertyPage);
    auto *multiGaugePreviewHostLayout = new QVBoxLayout(multiGaugePreviewHost);
    multiGaugePreviewHostLayout->setContentsMargins(0, 0, 0, 0);
    multiGaugePreviewHostLayout->addWidget(multiGauge, 0, Qt::AlignHCenter | Qt::AlignTop);
    multiGaugePreviewHostLayout->addStretch();
    multiGaugePreviewLayout->addWidget(multiGaugePreviewHost, 1);

    auto *multiGaugeEditorTabs = makePropertyTabs(multiGaugePropertyPage);
    multiGaugeEditorTabs->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeEditorTabs"));

    multiGaugeEditorTabs->setMinimumWidth(440);
    multiGaugeEditorTabs->setMinimumHeight(430);

    const auto makeMultiGaugePropertyForm = [multiGaugeEditorTabs](QFormLayout *&form) {
        auto *page = new QWidget(multiGaugeEditorTabs);
        form = new QFormLayout(page);
        form->setContentsMargins(12, 12, 12, 12);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(8);
        return page;
    };

    QFormLayout *multiGaugeRingForm = nullptr;
    QFormLayout *multiGaugeScaleForm = nullptr;
    QFormLayout *multiGaugeNeedleForm = nullptr;
    QFormLayout *multiGaugeDataForm = nullptr;
    QFormLayout *multiGaugePositionForm = nullptr;
    QFormLayout *multiGaugeTextForm = nullptr;
    auto *multiGaugeRingPage = makeMultiGaugePropertyForm(multiGaugeRingForm);
    auto *multiGaugeScalePage = makeMultiGaugePropertyForm(multiGaugeScaleForm);
    auto *multiGaugeNeedlePage = makeMultiGaugePropertyForm(multiGaugeNeedleForm);
    auto *multiGaugeDataPage = makeMultiGaugePropertyForm(multiGaugeDataForm);
    auto *multiGaugePositionPage = makeMultiGaugePropertyForm(multiGaugePositionForm);
    auto *multiGaugeTextPage = makeMultiGaugePropertyForm(multiGaugeTextForm);
    multiGaugeEditorTabs->addTab(multiGaugeRingPage, zzGaugeText("范围与环"));
    multiGaugeEditorTabs->addTab(multiGaugeScalePage, zzGaugeText("刻度"));
    multiGaugeEditorTabs->addTab(multiGaugeNeedlePage, zzGaugeText("指针"));
    multiGaugeEditorTabs->addTab(multiGaugeDataPage, zzGaugeText("数据"));
    multiGaugeEditorTabs->addTab(multiGaugePositionPage, zzGaugeText("位置"));
    multiGaugeEditorTabs->addTab(multiGaugeTextPage, zzGaugeText("文本"));

    const auto makeMultiGaugeMetricSlider
        = [multiGaugePropertyPage](qreal value, qreal minimum, qreal maximum) {
              constexpr int scale = 2;
              return makeValueSlider(multiGaugePropertyPage, qRound(minimum * scale), qRound(maximum * scale),
                  qRound(value * scale), 1, 4, scale, 1);
          };
    const auto makeMultiGaugeCapStyleCombo = [multiGaugePropertyPage](Qt::PenCapStyle style) {
        auto *combo = new QComboBox(multiGaugePropertyPage);
        combo->addItem(QStringLiteral("FlatCap"), Qt::FlatCap);
        combo->addItem(QStringLiteral("SquareCap"), Qt::SquareCap);
        combo->addItem(QStringLiteral("RoundCap"), Qt::RoundCap);
        combo->setCurrentIndex(combo->findData(style));
        return combo;
    };

    auto *multiGaugeMinimumSlider
        = makeValueSlider(multiGaugePropertyPage, -100, 99, qRound(multiGauge->minimum()));
    multiGaugeMinimumSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeMinimumSlider"));
    auto *multiGaugeMaximumSlider
        = makeValueSlider(multiGaugePropertyPage, 1, 200, qRound(multiGauge->maximum()));
    multiGaugeMaximumSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeMaximumSlider"));
    auto *multiGaugeMinimumAngleSlider
        = makeValueSlider(multiGaugePropertyPage, -360, 360, qRound(multiGauge->minimumAngle()), 5, 15);
    multiGaugeMinimumAngleSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeMinimumAngleSlider"));
    auto *multiGaugeMaximumAngleSlider
        = makeValueSlider(multiGaugePropertyPage, -360, 360, qRound(multiGauge->maximumAngle()), 5, 15);
    multiGaugeMaximumAngleSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeMaximumAngleSlider"));
    auto *multiGaugeScalePaddingSlider = makeMultiGaugeMetricSlider(multiGauge->scalePadding(), 0.0, 80.0);
    multiGaugeScalePaddingSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeScalePaddingSlider"));
    auto *multiGaugeTrackVisibleCheck = new QCheckBox(zzGaugeText("显示 Track"), multiGaugePropertyPage);
    multiGaugeTrackVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTrackVisibleCheck"));
    multiGaugeTrackVisibleCheck->setChecked(multiGauge->isTrackVisible());
    auto *multiGaugeTrackWidthSlider = makeMultiGaugeMetricSlider(multiGauge->trackWidth(), 0.5, 32.0);
    multiGaugeTrackWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTrackWidthSlider"));
    auto *multiGaugeTrackColorButton = new ZzExampleColorButton(multiGaugePropertyPage);
    multiGaugeTrackColorButton->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTrackColorButton"));
    bindGaugeColorButton(multiGaugeTrackColorButton, multiGauge, "trackColor", QPalette::Mid);
    auto *multiGaugeTrackCapStyleCombo = makeMultiGaugeCapStyleCombo(multiGauge->trackCapStyle());
    multiGaugeTrackCapStyleCombo->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeTrackCapStyleCombo"));
    auto *multiGaugeProgressVisibleCheck = new QCheckBox(zzGaugeText("显示进度弧"), multiGaugePropertyPage);
    multiGaugeProgressVisibleCheck->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeProgressVisibleCheck"));
    multiGaugeProgressVisibleCheck->setChecked(multiGauge->isProgressVisible());
    auto *multiGaugeProgressOverlapCheck = new QCheckBox(zzGaugeText("进度弧重叠"), multiGaugePropertyPage);
    multiGaugeProgressOverlapCheck->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeProgressOverlapCheck"));
    multiGaugeProgressOverlapCheck->setChecked(multiGauge->isProgressOverlap());
    auto *multiGaugeProgressWidthSlider = makeMultiGaugeMetricSlider(multiGauge->progressWidth(), 0.5, 32.0);
    multiGaugeProgressWidthSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeProgressWidthSlider"));
    auto *multiGaugeProgressSpacingSlider
        = makeMultiGaugeMetricSlider(multiGauge->progressSpacing(), 0.0, 32.0);
    multiGaugeProgressSpacingSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeProgressSpacingSlider"));
    multiGaugeProgressSpacingSlider->setEnabled(!multiGauge->isProgressOverlap());
    auto *multiGaugeProgressCapStyleCombo = makeMultiGaugeCapStyleCombo(multiGauge->progressCapStyle());
    multiGaugeProgressCapStyleCombo->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeProgressCapStyleCombo"));
    auto *multiGaugeAnimationSlider
        = makeValueSlider(multiGaugePropertyPage, 0, 2000, multiGauge->valueAnimationDuration(), 10, 100);
    multiGaugeAnimationSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeAnimationSlider"));

    multiGaugeRingForm->addRow(zzGaugeText("最小值"), multiGaugeMinimumSlider);
    multiGaugeRingForm->addRow(zzGaugeText("最大值"), multiGaugeMaximumSlider);
    multiGaugeRingForm->addRow(zzGaugeText("起始角度"), multiGaugeMinimumAngleSlider);
    multiGaugeRingForm->addRow(zzGaugeText("结束角度"), multiGaugeMaximumAngleSlider);
    multiGaugeRingForm->addRow(zzGaugeText("外圈边距"), multiGaugeScalePaddingSlider);
    multiGaugeRingForm->addRow(multiGaugeTrackVisibleCheck);
    multiGaugeRingForm->addRow(zzGaugeText("Track 宽度"), multiGaugeTrackWidthSlider);
    multiGaugeRingForm->addRow(zzGaugeText("Track 颜色"), multiGaugeTrackColorButton);
    multiGaugeRingForm->addRow(zzGaugeText("Track 端点"), multiGaugeTrackCapStyleCombo);
    multiGaugeRingForm->addRow(multiGaugeProgressVisibleCheck);
    multiGaugeRingForm->addRow(multiGaugeProgressOverlapCheck);
    multiGaugeRingForm->addRow(zzGaugeText("进度弧宽度"), multiGaugeProgressWidthSlider);
    multiGaugeRingForm->addRow(zzGaugeText("同心弧间距"), multiGaugeProgressSpacingSlider);
    multiGaugeRingForm->addRow(zzGaugeText("进度弧端点"), multiGaugeProgressCapStyleCombo);
    multiGaugeRingForm->addRow(zzGaugeText("数值动画时长"), multiGaugeAnimationSlider);

    auto *multiGaugeMajorTickCountSlider
        = makeValueSlider(multiGaugePropertyPage, 2, 100, multiGauge->majorTickCount());
    multiGaugeMajorTickCountSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeMajorTickCountSlider"));
    auto *multiGaugeMinorTickCountSlider
        = makeValueSlider(multiGaugePropertyPage, 0, 20, multiGauge->minorTickCount());
    multiGaugeMinorTickCountSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeMinorTickCountSlider"));
    auto *multiGaugeTickLengthSlider = makeMultiGaugeMetricSlider(multiGauge->tickLength(), 0.0, 32.0);
    multiGaugeTickLengthSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTickLengthSlider"));
    auto *multiGaugeTickWidthSlider = makeMultiGaugeMetricSlider(multiGauge->tickWidth(), 0.5, 12.0);
    multiGaugeTickWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTickWidthSlider"));
    auto *multiGaugeMajorTickLengthSlider
        = makeMultiGaugeMetricSlider(multiGauge->majorTickLength(), 0.0, 32.0);
    multiGaugeMajorTickLengthSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeMajorTickLengthSlider"));
    auto *multiGaugeMajorTickWidthSlider
        = makeMultiGaugeMetricSlider(multiGauge->majorTickWidth(), 0.5, 12.0);
    multiGaugeMajorTickWidthSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeMajorTickWidthSlider"));
    auto *multiGaugeTickPaddingSlider = makeMultiGaugeMetricSlider(multiGauge->tickPadding(), 0.0, 40.0);
    multiGaugeTickPaddingSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTickPaddingSlider"));
    auto *multiGaugeTickColorButton = new ZzExampleColorButton(multiGaugePropertyPage);
    multiGaugeTickColorButton->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTickColorButton"));
    bindGaugeColorButton(multiGaugeTickColorButton, multiGauge, "tickColor", QPalette::Text);
    auto *multiGaugeLabelsVisibleCheck = new QCheckBox(zzGaugeText("显示刻度标签"), multiGaugePropertyPage);
    multiGaugeLabelsVisibleCheck->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeLabelsVisibleCheck"));
    multiGaugeLabelsVisibleCheck->setChecked(multiGauge->areLabelsVisible());
    auto *multiGaugeLabelPaddingSlider = makeMultiGaugeMetricSlider(multiGauge->labelPadding(), 0.0, 40.0);
    multiGaugeLabelPaddingSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeLabelPaddingSlider"));
    auto *multiGaugeLabelFontSizeSlider
        = makeValueSlider(multiGaugePropertyPage, 1, 48, multiGauge->labelFontPixelSize());
    multiGaugeLabelFontSizeSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeLabelFontSizeSlider"));
    auto *multiGaugeLabelColorButton = new ZzExampleColorButton(multiGaugePropertyPage);
    multiGaugeLabelColorButton->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeLabelColorButton"));
    bindGaugeColorButton(multiGaugeLabelColorButton, multiGauge, "labelColor", QPalette::Text);

    multiGaugeScaleForm->addRow(zzGaugeText("主刻度数量"), multiGaugeMajorTickCountSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("每段次刻度数量"), multiGaugeMinorTickCountSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("次刻度长度"), multiGaugeTickLengthSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("次刻度宽度"), multiGaugeTickWidthSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("主刻度长度"), multiGaugeMajorTickLengthSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("主刻度宽度"), multiGaugeMajorTickWidthSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("刻度内边距"), multiGaugeTickPaddingSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("刻度颜色"), multiGaugeTickColorButton);
    multiGaugeScaleForm->addRow(multiGaugeLabelsVisibleCheck);
    multiGaugeScaleForm->addRow(zzGaugeText("标签内边距"), multiGaugeLabelPaddingSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("标签字号"), multiGaugeLabelFontSizeSlider);
    multiGaugeScaleForm->addRow(zzGaugeText("标签颜色"), multiGaugeLabelColorButton);

    auto *multiGaugeNeedleStyleCombo = new QComboBox(multiGaugePropertyPage);
    multiGaugeNeedleStyleCombo->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeNeedleStyleCombo"));
    multiGaugeNeedleStyleCombo->addItem(zzGaugeText("无指针"), ZzMultiRadialGauge::NoNeedle);
    multiGaugeNeedleStyleCombo->addItem(zzGaugeText("线形指针"), ZzMultiRadialGauge::LineNeedle);
    multiGaugeNeedleStyleCombo->addItem(zzGaugeText("三角指针"), ZzMultiRadialGauge::TriangleNeedle);
    multiGaugeNeedleStyleCombo->setCurrentIndex(
        multiGaugeNeedleStyleCombo->findData(multiGauge->needleStyle()));
    auto *multiGaugeNeedleWidthSlider = makeMultiGaugeMetricSlider(multiGauge->needleWidth(), 0.5, 32.0);
    multiGaugeNeedleWidthSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeNeedleWidthSlider"));
    auto *multiGaugeNeedleLengthSlider = makeValueSlider(
        multiGaugePropertyPage, 5, 120, qRound(multiGauge->needleLength() * 100.0), 1, 5, 100, 2);
    multiGaugeNeedleLengthSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeNeedleLengthSlider"));
    auto *multiGaugeNeedleOffsetXSlider = makeValueSlider(
        multiGaugePropertyPage, -100, 100, qRound(multiGauge->needleOffset().x() * 100.0), 1, 5, 100, 2);
    multiGaugeNeedleOffsetXSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeNeedleOffsetXSlider"));
    auto *multiGaugeNeedleOffsetYSlider = makeValueSlider(
        multiGaugePropertyPage, -100, 100, qRound(multiGauge->needleOffset().y() * 100.0), 1, 5, 100, 2);
    multiGaugeNeedleOffsetYSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeNeedleOffsetYSlider"));
    auto *multiGaugeHubVisibleCheck = new QCheckBox(zzGaugeText("显示公共轴心"), multiGaugePropertyPage);
    multiGaugeHubVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeHubVisibleCheck"));
    multiGaugeHubVisibleCheck->setChecked(multiGauge->isHubVisible());
    auto *multiGaugeHubRadiusSlider = makeMultiGaugeMetricSlider(multiGauge->hubRadius(), 0.0, 32.0);
    multiGaugeHubRadiusSlider->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeHubRadiusSlider"));
    auto *multiGaugeHubColorButton = new ZzExampleColorButton(multiGaugePropertyPage);
    multiGaugeHubColorButton->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeHubColorButton"));
    multiGaugeHubColorButton->setSelectedColor(multiGauge->hubColor());

    multiGaugeNeedleForm->addRow(zzGaugeText("指针样式"), multiGaugeNeedleStyleCombo);
    multiGaugeNeedleForm->addRow(zzGaugeText("指针宽度"), multiGaugeNeedleWidthSlider);
    multiGaugeNeedleForm->addRow(zzGaugeText("指针长度比例"), multiGaugeNeedleLengthSlider);
    multiGaugeNeedleForm->addRow(zzGaugeText("指针中心 X"), multiGaugeNeedleOffsetXSlider);
    multiGaugeNeedleForm->addRow(zzGaugeText("指针中心 Y"), multiGaugeNeedleOffsetYSlider);
    multiGaugeNeedleForm->addRow(multiGaugeHubVisibleCheck);
    multiGaugeNeedleForm->addRow(zzGaugeText("轴心半径"), multiGaugeHubRadiusSlider);
    multiGaugeNeedleForm->addRow(zzGaugeText("轴心颜色"), multiGaugeHubColorButton);

    const QList<ZzMultiRadialGaugeItem *> multiGaugeItems { multiGaugeGoodItem, multiGaugeBetterItem,
        multiGaugePerfectItem };
    QList<QSlider *> multiGaugeItemValueSliders;
    for (int index = 0; index < multiGaugeItems.size(); ++index) {
        ZzMultiRadialGaugeItem *item = multiGaugeItems.at(index);
        const QString itemName = zzGaugeText("项目 %1").arg(index + 1);
        auto *visibleCheck = new QCheckBox(zzGaugeText("显示"), multiGaugePropertyPage);
        visibleCheck->setObjectName(QStringLiteral("zzRadialEditor_visibleCheck"));
        visibleCheck->setChecked(item->isVisible());
        auto *labelEdit = new QLineEdit(item->label(), multiGaugePropertyPage);
        labelEdit->setObjectName(QStringLiteral("zzRadialEditor_labelEdit"));
        auto *itemValueSlider = makeValueSlider(multiGaugePropertyPage, qRound(multiGauge->minimum()),
            qRound(multiGauge->maximum()), qRound(item->value()));
        itemValueSlider->setObjectName(QStringLiteral("zzRadialEditor_itemValueSlider"));
        auto *colorButton = new ZzExampleColorButton(multiGaugePropertyPage);
        colorButton->setObjectName(QStringLiteral("zzRadialEditor_colorButton"));
        colorButton->setSelectedColor(item->color());
        multiGaugeItemValueSliders.append(itemValueSlider);
        multiGaugeDataForm->addRow(itemName + zzGaugeText(" 可见"), visibleCheck);
        multiGaugeDataForm->addRow(itemName + zzGaugeText(" 名称"), labelEdit);
        multiGaugeDataForm->addRow(itemName + zzGaugeText(" 数值"), itemValueSlider);
        multiGaugeDataForm->addRow(itemName + zzGaugeText(" 颜色"), colorButton);
        QObject::connect(visibleCheck, &QCheckBox::toggled, item, &ZzMultiRadialGaugeItem::setVisible);
        QObject::connect(labelEdit, &QLineEdit::textChanged, item, &ZzMultiRadialGaugeItem::setLabel);
        connectScaledSlider(itemValueSlider, item, 1.0, &ZzMultiRadialGaugeItem::setValue);
        QObject::connect(colorButton, &ZzExampleColorButton::selectedColorChanged, item,
            &ZzMultiRadialGaugeItem::setColor);

        const auto addOffsetRow = [&](const QString &label, qreal value, bool titleOffset, bool xCoordinate) {
            auto *slider
                = makeValueSlider(multiGaugePropertyPage, -120, 120, qRound(value * 100.0), 1, 5, 100, 2);
            multiGaugePositionForm->addRow(itemName + label, slider);
            QObject::connect(slider, &QSlider::valueChanged, item, [=](int sliderValue) {
                QPointF offset = titleOffset ? item->titleOffset() : item->detailOffset();
                if (xCoordinate) {
                    offset.setX(sliderValue / 100.0);
                } else {
                    offset.setY(sliderValue / 100.0);
                }
                if (titleOffset) {
                    item->setTitleOffset(offset);
                } else {
                    item->setDetailOffset(offset);
                }
            });
        };
        addOffsetRow(zzGaugeText(" 标题 X"), item->titleOffset().x(), true, true);
        addOffsetRow(zzGaugeText(" 标题 Y"), item->titleOffset().y(), true, false);
        addOffsetRow(zzGaugeText(" 数值 X"), item->detailOffset().x(), false, true);
        addOffsetRow(zzGaugeText(" 数值 Y"), item->detailOffset().y(), false, false);
    }

    auto *multiGaugeTitleVisibleCheck = new QCheckBox(zzGaugeText("显示名称"), multiGaugePropertyPage);
    multiGaugeTitleVisibleCheck->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTitleVisibleCheck"));
    multiGaugeTitleVisibleCheck->setChecked(multiGauge->isTitleVisible());
    auto *multiGaugeDetailVisibleCheck = new QCheckBox(zzGaugeText("显示数值"), multiGaugePropertyPage);
    multiGaugeDetailVisibleCheck->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeDetailVisibleCheck"));
    multiGaugeDetailVisibleCheck->setChecked(multiGauge->isDetailVisible());
    auto *multiGaugeDetailBadgeVisibleCheck
        = new QCheckBox(zzGaugeText("数值使用实心徽标"), multiGaugePropertyPage);
    multiGaugeDetailBadgeVisibleCheck->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeDetailBadgeVisibleCheck"));
    multiGaugeDetailBadgeVisibleCheck->setChecked(multiGauge->isDetailBadgeVisible());
    auto *multiGaugeTitleFontSizeSlider
        = makeValueSlider(multiGaugePropertyPage, 1, 48, multiGauge->titleFontPixelSize());
    multiGaugeTitleFontSizeSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeTitleFontSizeSlider"));
    auto *multiGaugeDetailFontSizeSlider
        = makeValueSlider(multiGaugePropertyPage, 1, 48, multiGauge->detailFontPixelSize());
    multiGaugeDetailFontSizeSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeDetailFontSizeSlider"));
    auto *multiGaugeTitleColorButton = new ZzExampleColorButton(multiGaugePropertyPage);
    multiGaugeTitleColorButton->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeTitleColorButton"));
    bindGaugeColorButton(multiGaugeTitleColorButton, multiGauge, "titleColor", QPalette::Text);
    auto *multiGaugeDetailTextColorButton = new ZzExampleColorButton(multiGaugePropertyPage);
    multiGaugeDetailTextColorButton->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeDetailTextColorButton"));
    multiGaugeDetailTextColorButton->setSelectedColor(QColor(Qt::white));
    auto *autoDetailTextColorCheck = new QCheckBox(zzGaugeText("自动对比色"), multiGaugePropertyPage);
    autoDetailTextColorCheck->setObjectName(QStringLiteral("zzRadialEditor_autoDetailTextColorCheck"));
    autoDetailTextColorCheck->setChecked(!multiGauge->detailTextColor().isValid());
    multiGaugeDetailTextColorButton->setEnabled(!autoDetailTextColorCheck->isChecked());
    auto *detailTextColorRow = new QWidget(multiGaugePropertyPage);
    auto *detailTextColorLayout = new QHBoxLayout(detailTextColorRow);
    detailTextColorLayout->setContentsMargins(0, 0, 0, 0);
    detailTextColorLayout->addWidget(autoDetailTextColorCheck);
    detailTextColorLayout->addWidget(multiGaugeDetailTextColorButton);
    detailTextColorLayout->addStretch();
    auto *multiGaugeDetailBadgePaddingSlider
        = makeMultiGaugeMetricSlider(multiGauge->detailBadgePadding(), 0.0, 32.0);
    multiGaugeDetailBadgePaddingSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeDetailBadgePaddingSlider"));
    auto *multiGaugeValueSuffixEdit = new QLineEdit(multiGauge->valueSuffix(), multiGaugePropertyPage);
    multiGaugeValueSuffixEdit->setObjectName(QStringLiteral("zzRadialEditor_multiGaugeValueSuffixEdit"));
    auto *multiGaugeValueDecimalsSlider
        = makeValueSlider(multiGaugePropertyPage, 0, 6, multiGauge->valueDecimals());
    multiGaugeValueDecimalsSlider->setObjectName(
        QStringLiteral("zzRadialEditor_multiGaugeValueDecimalsSlider"));

    multiGaugeTextForm->addRow(multiGaugeTitleVisibleCheck);
    multiGaugeTextForm->addRow(multiGaugeDetailVisibleCheck);
    multiGaugeTextForm->addRow(multiGaugeDetailBadgeVisibleCheck);
    multiGaugeTextForm->addRow(zzGaugeText("名称字号"), multiGaugeTitleFontSizeSlider);
    multiGaugeTextForm->addRow(zzGaugeText("数值字号"), multiGaugeDetailFontSizeSlider);
    multiGaugeTextForm->addRow(zzGaugeText("名称颜色"), multiGaugeTitleColorButton);
    multiGaugeTextForm->addRow(zzGaugeText("数值文字颜色"), detailTextColorRow);
    multiGaugeTextForm->addRow(zzGaugeText("徽标水平内边距"), multiGaugeDetailBadgePaddingSlider);
    multiGaugeTextForm->addRow(zzGaugeText("数值后缀"), multiGaugeValueSuffixEdit);
    multiGaugeTextForm->addRow(zzGaugeText("数值小数位"), multiGaugeValueDecimalsSlider);

    QObject::connect(multiGaugeMinimumSlider, &QSlider::valueChanged, multiGauge, [=](int minimum) {
        if (minimum >= multiGaugeMaximumSlider->value()) {
            multiGaugeMaximumSlider->setValue(minimum + 1);
        }
        multiGauge->setMinimum(minimum);
        for (QSlider *slider : multiGaugeItemValueSliders) {
            slider->setMinimum(minimum);
        }
    });
    QObject::connect(multiGaugeMaximumSlider, &QSlider::valueChanged, multiGauge, [=](int maximum) {
        if (maximum <= multiGaugeMinimumSlider->value()) {
            multiGaugeMinimumSlider->setValue(maximum - 1);
        }
        multiGauge->setMaximum(maximum);
        for (QSlider *slider : multiGaugeItemValueSliders) {
            slider->setMaximum(maximum);
        }
    });
    connectScaledSlider(multiGaugeMinimumAngleSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setMinimumAngle);
    connectScaledSlider(multiGaugeMaximumAngleSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setMaximumAngle);
    connectScaledSlider(multiGaugeScalePaddingSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setScalePadding);
    QObject::connect(
        multiGaugeTrackVisibleCheck, &QCheckBox::toggled, multiGauge, &ZzMultiRadialGauge::setTrackVisible);
    connectScaledSlider(multiGaugeTrackWidthSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setTrackWidth);
    QObject::connect(multiGaugeTrackColorButton, &ZzExampleColorButton::selectedColorChanged, multiGauge,
        &ZzMultiRadialGauge::setTrackColor);
    QObject::connect(
        multiGaugeTrackCapStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged), multiGauge, [=](int) {
            multiGauge->setTrackCapStyle(
                static_cast<Qt::PenCapStyle>(multiGaugeTrackCapStyleCombo->currentData().toInt()));
        });
    QObject::connect(multiGaugeProgressVisibleCheck, &QCheckBox::toggled, multiGauge,
        &ZzMultiRadialGauge::setProgressVisible);
    QObject::connect(multiGaugeProgressOverlapCheck, &QCheckBox::toggled, multiGauge,
        &ZzMultiRadialGauge::setProgressOverlap);
    QObject::connect(multiGaugeProgressOverlapCheck, &QCheckBox::toggled, multiGaugeProgressSpacingSlider,
        [multiGaugeProgressSpacingSlider](
            bool overlap) { multiGaugeProgressSpacingSlider->setEnabled(!overlap); });
    connectScaledSlider(
        multiGaugeProgressWidthSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setProgressWidth);
    connectScaledSlider(
        multiGaugeProgressSpacingSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setProgressSpacing);
    QObject::connect(multiGaugeProgressCapStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
        multiGauge, [=](int) {
            multiGauge->setProgressCapStyle(
                static_cast<Qt::PenCapStyle>(multiGaugeProgressCapStyleCombo->currentData().toInt()));
        });
    connectScaledSlider(
        multiGaugeAnimationSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setValueAnimationDuration);

    connectScaledSlider(
        multiGaugeMajorTickCountSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setMajorTickCount);
    connectScaledSlider(
        multiGaugeMinorTickCountSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setMinorTickCount);
    connectScaledSlider(multiGaugeTickLengthSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setTickLength);
    connectScaledSlider(multiGaugeTickWidthSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setTickWidth);
    connectScaledSlider(
        multiGaugeMajorTickLengthSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setMajorTickLength);
    connectScaledSlider(
        multiGaugeMajorTickWidthSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setMajorTickWidth);
    connectScaledSlider(multiGaugeTickPaddingSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setTickPadding);
    QObject::connect(multiGaugeTickColorButton, &ZzExampleColorButton::selectedColorChanged, multiGauge,
        &ZzMultiRadialGauge::setTickColor);
    QObject::connect(
        multiGaugeLabelsVisibleCheck, &QCheckBox::toggled, multiGauge, &ZzMultiRadialGauge::setLabelsVisible);
    connectScaledSlider(multiGaugeLabelPaddingSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setLabelPadding);
    connectScaledSlider(
        multiGaugeLabelFontSizeSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setLabelFontPixelSize);
    QObject::connect(multiGaugeLabelColorButton, &ZzExampleColorButton::selectedColorChanged, multiGauge,
        &ZzMultiRadialGauge::setLabelColor);

    QObject::connect(
        multiGaugeNeedleStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged), multiGauge, [=](int) {
            multiGauge->setNeedleStyle(static_cast<ZzMultiRadialGauge::NeedleStyle>(
                multiGaugeNeedleStyleCombo->currentData().toInt()));
        });
    connectScaledSlider(multiGaugeNeedleWidthSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setNeedleWidth);
    connectScaledSlider(
        multiGaugeNeedleLengthSlider, multiGauge, 100.0, &ZzMultiRadialGauge::setNeedleLength);
    const auto updateMultiGaugeNeedleOffset = [=] {
        multiGauge->setNeedleOffset(QPointF(
            multiGaugeNeedleOffsetXSlider->value() / 100.0, multiGaugeNeedleOffsetYSlider->value() / 100.0));
    };
    QObject::connect(
        multiGaugeNeedleOffsetXSlider, &QSlider::valueChanged, multiGauge, updateMultiGaugeNeedleOffset);
    QObject::connect(
        multiGaugeNeedleOffsetYSlider, &QSlider::valueChanged, multiGauge, updateMultiGaugeNeedleOffset);
    QObject::connect(
        multiGaugeHubVisibleCheck, &QCheckBox::toggled, multiGauge, &ZzMultiRadialGauge::setHubVisible);
    connectScaledSlider(multiGaugeHubRadiusSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setHubRadius);
    QObject::connect(multiGaugeHubColorButton, &ZzExampleColorButton::selectedColorChanged, multiGauge,
        &ZzMultiRadialGauge::setHubColor);

    QObject::connect(
        multiGaugeTitleVisibleCheck, &QCheckBox::toggled, multiGauge, &ZzMultiRadialGauge::setTitleVisible);
    QObject::connect(
        multiGaugeDetailVisibleCheck, &QCheckBox::toggled, multiGauge, &ZzMultiRadialGauge::setDetailVisible);
    QObject::connect(multiGaugeDetailBadgeVisibleCheck, &QCheckBox::toggled, multiGauge,
        &ZzMultiRadialGauge::setDetailBadgeVisible);
    connectScaledSlider(
        multiGaugeTitleFontSizeSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setTitleFontPixelSize);
    connectScaledSlider(
        multiGaugeDetailFontSizeSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setDetailFontPixelSize);
    QObject::connect(multiGaugeTitleColorButton, &ZzExampleColorButton::selectedColorChanged, multiGauge,
        &ZzMultiRadialGauge::setTitleColor);
    QObject::connect(autoDetailTextColorCheck, &QCheckBox::toggled, multiGauge, [=](bool automatic) {
        multiGaugeDetailTextColorButton->setEnabled(!automatic);
        multiGauge->setDetailTextColor(automatic ? QColor() : multiGaugeDetailTextColorButton->selectedColor());
    });
    QObject::connect(multiGaugeDetailTextColorButton, &ZzExampleColorButton::selectedColorChanged, multiGauge,
        [=](const QColor &color) {
            if (!autoDetailTextColorCheck->isChecked()) multiGauge->setDetailTextColor(color);
        });
    connectScaledSlider(
        multiGaugeDetailBadgePaddingSlider, multiGauge, 2.0, &ZzMultiRadialGauge::setDetailBadgePadding);
    QObject::connect(
        multiGaugeValueSuffixEdit, &QLineEdit::textChanged, multiGauge, &ZzMultiRadialGauge::setValueSuffix);
    connectScaledSlider(
        multiGaugeValueDecimalsSlider, multiGauge, 1.0, &ZzMultiRadialGauge::setValueDecimals);

    multiGaugePreviewLayout->addWidget(multiGaugeEditorTabs, 1);
    multiGaugePropertyLayout->addLayout(multiGaugePreviewLayout);
    auto *multiGaugeHint = new QLabel(zzGaugeText("共享范围、刻度、Track "
                                                  "和轴心；每个数据项分别配置名称、数值、颜色以及标题和详情位"
                                                  "置。关闭重叠后，进度弧会改为同心排列。"),
        multiGaugePropertyPage);
    multiGaugeHint->setWordWrap(true);
    multiGaugePropertyLayout->addWidget(multiGaugeHint);
    multiGaugePropertyLayout->addStretch();
}
} // namespace ZzExample
