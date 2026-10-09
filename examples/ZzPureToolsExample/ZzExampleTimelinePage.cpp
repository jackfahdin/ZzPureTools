#include "ZzExampleShowcasePagePrivate.h"
#include "ZzExampleTimelineHelpers.h"
#include <QGridLayout>

namespace ZzExample {
namespace {
    QWidget* makeTimelineSample(
        const QString& title, const QString& subtitle, ZzTimeline* timeline, QWidget* parent)
    {
        auto* sample = makeCard(parent);
        auto* layout = new QVBoxLayout(sample);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(6);
        layout->addWidget(makeSectionTitle(title, sample));
        auto* hint = new QLabel(subtitle, sample);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        timeline->setParent(sample);
        timeline->setMinimumWidth(300);
        timeline->setFixedHeight(330);
        layout->addWidget(timeline);
        return sample;
    }
    /** @brief 装配时间轴示例页内容，不持有控件所有权。 */
    class ZzExampleTimelinePage final
    {
    public:
        static void build(QVBoxLayout* mainLayout, QWidget* content);
    };

} // namespace

void ZzExampleTimelinePage::build(QVBoxLayout* mainLayout, QWidget* content)
{
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);
    auto* description = new QLabel(zzGaugeText("用于展示事件与状态变化；支持水平/垂直、正序/"
                                               "倒序、单侧/交错布局和当前事件动画。"),
        content);
    description->setWordWrap(true);
    mainLayout->addWidget(description);
    auto* examplesGrid = new QGridLayout;
    examplesGrid->setHorizontalSpacing(16);
    examplesGrid->setVerticalSpacing(16);
    examplesGrid->setColumnStretch(0, 1);
    examplesGrid->setColumnStretch(1, 1);
    auto* orderTimeline = new ZzTimeline;
    orderTimeline->setLayoutMode(ZzTimeline::ContentOnRight);
    orderTimeline->setTimestampFormat(QStringLiteral("HH:mm"));
    orderTimeline->addEvent(timelineSampleTime().addSecs(-5400), zzGaugeText("订单已提交"),
        zzGaugeText("订单信息已进入处理队列。"), ZzTimelineEvent::Completed);
    orderTimeline->addEvent(timelineSampleTime().addSecs(-3600), zzGaugeText("付款成功"),
        zzGaugeText("支付信息已经确认。"), ZzTimelineEvent::Completed);
    orderTimeline->addEvent(timelineSampleTime().addSecs(-900), zzGaugeText("正在打包"),
        zzGaugeText("仓库正在准备商品。"), ZzTimelineEvent::Current);
    orderTimeline->addEvent(timelineSampleTime().addSecs(1800), zzGaugeText("等待发货"),
        zzGaugeText("物流单号生成后将自动更新。"), ZzTimelineEvent::Pending);
    examplesGrid->addWidget(
        makeTimelineSample(zzGaugeText("订单流程"),
            zzGaugeText("内容在右侧，完成、当前和等待状态使用不同节点。"), orderTimeline, content),
        0, 0);

    auto* updateTimeline = new ZzTimeline;
    updateTimeline->setLayoutMode(ZzTimeline::ContentOnRight);
    updateTimeline->setReverse(true);
    auto* updateReady = updateTimeline->addEvent(QDateTime(), zzGaugeText("版本 2.4.0 已发布"),
        zzGaugeText("新增时间轴与多数据仪表盘。"), ZzTimelineEvent::Completed);
    updateReady->setTimeText(zzGaugeText("3 天前"));
    auto* updateTesting = updateTimeline->addEvent(QDateTime(), zzGaugeText("完成回归测试"),
        zzGaugeText("控件交互与主题切换检查通过。"), ZzTimelineEvent::Completed);
    updateTesting->setTimeText(zzGaugeText("昨天"));
    auto* updateCurrent = updateTimeline->addEvent(QDateTime(), zzGaugeText("准备发布说明"),
        zzGaugeText("正在整理新增 API 和示例。"), ZzTimelineEvent::Current);
    updateCurrent->setTimeText(zzGaugeText("刚刚"));
    examplesGrid->addWidget(makeTimelineSample(zzGaugeText("更新记录"),
                                zzGaugeText("倒序显示，事件可以使用相对时间文本。"), updateTimeline, content),
        0, 1);

    auto* systemTimeline = new ZzTimeline;
    systemTimeline->setLayoutMode(ZzTimeline::Alternating);
    systemTimeline->setTimestampFormat(QStringLiteral("HH:mm:ss"));
    systemTimeline->addEvent(timelineSampleTime().addSecs(-300), zzGaugeText("服务已启动"),
        zzGaugeText("监听端口 8080。"), ZzTimelineEvent::Completed);
    systemTimeline->addEvent(timelineSampleTime().addSecs(-160), zzGaugeText("内存占用偏高"),
        zzGaugeText("当前使用率为 78%。"), ZzTimelineEvent::Warning);
    systemTimeline->addEvent(timelineSampleTime().addSecs(-70), zzGaugeText("连接中断"),
        zzGaugeText("远程节点暂时不可用。"), ZzTimelineEvent::Error);
    systemTimeline->addEvent(timelineSampleTime(), zzGaugeText("正在重新连接"),
        zzGaugeText("将在几秒后再次尝试。"), ZzTimelineEvent::Current);
    examplesGrid->addWidget(
        makeTimelineSample(zzGaugeText("系统事件"),
            zzGaugeText("交错布局适合同时展示状态、时间和较短的事件内容。"), systemTimeline, content),
        1, 0, 1, 2);

    auto* horizontalTimeline = new ZzTimeline;
    horizontalTimeline->setOrientation(Qt::Horizontal);
    horizontalTimeline->setLayoutMode(ZzTimeline::Alternating);
    horizontalTimeline->setHorizontalItemWidth(210);
    horizontalTimeline->setTimestampWidth(88);
    auto* createdEvent = horizontalTimeline->addEvent(
        QDateTime(), zzGaugeText("创建"), zzGaugeText("提交任务"), ZzTimelineEvent::Completed);
    createdEvent->setTimeText(zzGaugeText("09:00"));
    auto* reviewedEvent = horizontalTimeline->addEvent(
        QDateTime(), zzGaugeText("审核"), zzGaugeText("确认内容"), ZzTimelineEvent::Completed);
    reviewedEvent->setTimeText(zzGaugeText("09:20"));
    auto* processingEvent = horizontalTimeline->addEvent(
        QDateTime(), zzGaugeText("处理"), zzGaugeText("正在执行"), ZzTimelineEvent::Current);
    processingEvent->setTimeText(zzGaugeText("09:35"));
    auto* deliveryEvent = horizontalTimeline->addEvent(
        QDateTime(), zzGaugeText("交付"), zzGaugeText("等待完成"), ZzTimelineEvent::Pending);
    deliveryEvent->setTimeText(zzGaugeText("10:00"));
    QWidget* horizontalSample = makeTimelineSample(zzGaugeText("水平时间轴"),
        zzGaugeText("主线横向延伸，支持上方、下方和上下交错。"), horizontalTimeline, content);
    horizontalTimeline->setFixedHeight(250);
    examplesGrid->addWidget(horizontalSample, 2, 0, 1, 2);

    mainLayout->addLayout(examplesGrid);

    auto* propertiesCard = makeCard(content);
    auto* propertiesLayout = new QVBoxLayout(propertiesCard);
    propertiesLayout->setContentsMargins(16, 16, 16, 16);
    propertiesLayout->setSpacing(12);
    propertiesLayout->addWidget(makeSectionTitle(zzGaugeText("实时属性"), propertiesCard));

    auto* propertyPreviewLayout = new QHBoxLayout;
    propertyPreviewLayout->setSpacing(28);

    auto* previewTimeline = new ZzTimeline(propertiesCard);
    previewTimeline->setMinimumHeight(470);
    previewTimeline->setLayoutMode(ZzTimeline::Alternating);
    previewTimeline->setTimestampFormat(QStringLiteral("HH:mm"));
    resetTimelineEvents(previewTimeline);
    propertyPreviewLayout->addWidget(previewTimeline, 1);

    auto* editorTabs = makePropertyTabs(propertiesCard);
    editorTabs->setMinimumWidth(380);
    editorTabs->setMinimumHeight(470);

    auto* layoutPage = new QWidget(editorTabs);
    auto* layoutForm = new QFormLayout(layoutPage);
    layoutForm->setContentsMargins(12, 12, 12, 12);
    layoutForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layoutForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layoutForm->setVerticalSpacing(9);

    auto* stylePage = new QWidget(editorTabs);
    auto* styleForm = new QFormLayout(stylePage);
    styleForm->setContentsMargins(12, 12, 12, 12);
    styleForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    styleForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    styleForm->setVerticalSpacing(9);

    auto* eventPage = new QWidget(editorTabs);
    auto* eventForm = new QFormLayout(eventPage);
    eventForm->setContentsMargins(12, 12, 12, 12);
    eventForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    eventForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    eventForm->setVerticalSpacing(9);

    editorTabs->addTab(layoutPage, zzGaugeText("布局"));
    editorTabs->addTab(stylePage, zzGaugeText("样式"));
    editorTabs->addTab(eventPage, zzGaugeText("事件"));

    auto* orientationCombo = new QComboBox(layoutPage);
    orientationCombo->addItem(zzGaugeText("垂直"), Qt::Vertical);
    orientationCombo->addItem(zzGaugeText("水平"), Qt::Horizontal);
    orientationCombo->setCurrentIndex(orientationCombo->findData(previewTimeline->orientation()));
    auto* layoutModeCombo = new QComboBox(layoutPage);
    layoutModeCombo->addItem(zzGaugeText("右侧 / 下方"), ZzTimeline::ContentOnRight);
    layoutModeCombo->addItem(zzGaugeText("左侧 / 上方"), ZzTimeline::ContentOnLeft);
    layoutModeCombo->addItem(zzGaugeText("左右 / 上下交错"), ZzTimeline::Alternating);
    layoutModeCombo->addItem(zzGaugeText("反向交错"), ZzTimeline::AlternatingReverse);
    layoutModeCombo->setCurrentIndex(layoutModeCombo->findData(previewTimeline->layoutMode()));
    auto* reverseCheck = new QCheckBox(zzGaugeText("反转显示顺序"), layoutPage);
    reverseCheck->setChecked(previewTimeline->isReverse());
    auto* timestampVisibleCheck = new QCheckBox(zzGaugeText("显示时间"), layoutPage);
    timestampVisibleCheck->setChecked(previewTimeline->isTimestampVisible());
    auto* descriptionVisibleCheck = new QCheckBox(zzGaugeText("显示描述"), layoutPage);
    descriptionVisibleCheck->setChecked(previewTimeline->isDescriptionVisible());
    auto* animationEnabledCheck = new QCheckBox(zzGaugeText("当前节点动画"), stylePage);
    animationEnabledCheck->setChecked(previewTimeline->isAnimationEnabled());
    auto* timestampFormatEdit = new QLineEdit(previewTimeline->timestampFormat(), layoutPage);
    auto* timestampWidthSlider = makeValueSlider(layoutPage, 40, 240, previewTimeline->timestampWidth());
    auto* nodeSizeSlider = makeValueSlider(stylePage, 6, 40, previewTimeline->nodeSize());
    auto* lineWidthSlider
        = makeValueSlider(stylePage, 1, 20, qRound(previewTimeline->lineWidth() * 2.0), 1, 2, 2, 1);
    auto* itemSpacingSlider = makeValueSlider(layoutPage, 0, 80, previewTimeline->itemSpacing());
    auto* horizontalItemWidthSlider
        = makeValueSlider(layoutPage, 120, 480, previewTimeline->horizontalItemWidth());
    horizontalItemWidthSlider->setEnabled(previewTimeline->orientation() == Qt::Horizontal);
    auto* contentPaddingSlider = makeValueSlider(layoutPage, 0, 80, previewTimeline->contentPadding());
    auto* titleFontSizeSlider = makeValueSlider(stylePage, 0, 36, previewTimeline->titleFontPixelSize());
    auto* descriptionFontSizeSlider
        = makeValueSlider(stylePage, 0, 36, previewTimeline->descriptionFontPixelSize());
    auto* timestampFontSizeSlider
        = makeValueSlider(stylePage, 0, 36, previewTimeline->timestampFontPixelSize());
    auto* animationDurationSlider
        = makeValueSlider(stylePage, 200, 4000, previewTimeline->animationDuration());
    auto* lineColorButton = new ZzExampleColorButton(stylePage);
    lineColorButton->setSelectedColor(previewTimeline->palette().color(QPalette::Mid));

    layoutForm->addRow(zzGaugeText("方向"), orientationCombo);
    layoutForm->addRow(zzGaugeText("布局模式"), layoutModeCombo);
    layoutForm->addRow(reverseCheck);
    layoutForm->addRow(timestampVisibleCheck);
    layoutForm->addRow(descriptionVisibleCheck);
    layoutForm->addRow(zzGaugeText("时间格式"), timestampFormatEdit);
    layoutForm->addRow(zzGaugeText("时间列宽"), timestampWidthSlider);
    layoutForm->addRow(zzGaugeText("事件间距"), itemSpacingSlider);
    layoutForm->addRow(zzGaugeText("水平项宽度"), horizontalItemWidthSlider);
    layoutForm->addRow(zzGaugeText("左右边距"), contentPaddingSlider);

    styleForm->addRow(animationEnabledCheck);
    styleForm->addRow(zzGaugeText("节点大小"), nodeSizeSlider);
    styleForm->addRow(zzGaugeText("连接线宽度"), lineWidthSlider);
    styleForm->addRow(zzGaugeText("标题字号（0 自动）"), titleFontSizeSlider);
    styleForm->addRow(zzGaugeText("描述字号（0 自动）"), descriptionFontSizeSlider);
    styleForm->addRow(zzGaugeText("时间字号（0 自动）"), timestampFontSizeSlider);
    styleForm->addRow(zzGaugeText("动画时长"), animationDurationSlider);
    styleForm->addRow(zzGaugeText("连接线颜色"), lineColorButton);

    QObject::connect(
        orientationCombo, qOverload<int>(&QComboBox::currentIndexChanged), previewTimeline, [=](int) {
            const auto orientation = static_cast<Qt::Orientation>(orientationCombo->currentData().toInt());
            previewTimeline->setOrientation(orientation);
            horizontalItemWidthSlider->setEnabled(orientation == Qt::Horizontal);
            itemSpacingSlider->setEnabled(orientation == Qt::Vertical);
        });
    QObject::connect(
        layoutModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), previewTimeline, [=](int) {
            previewTimeline->setLayoutMode(
                static_cast<ZzTimeline::ZzTimelineLayoutMode>(layoutModeCombo->currentData().toInt()));
        });
    QObject::connect(reverseCheck, &QCheckBox::toggled, previewTimeline, &ZzTimeline::setReverse);
    QObject::connect(
        timestampVisibleCheck, &QCheckBox::toggled, previewTimeline, &ZzTimeline::setTimestampVisible);
    QObject::connect(
        descriptionVisibleCheck, &QCheckBox::toggled, previewTimeline, &ZzTimeline::setDescriptionVisible);
    QObject::connect(
        animationEnabledCheck, &QCheckBox::toggled, previewTimeline, &ZzTimeline::setAnimationEnabled);
    QObject::connect(
        timestampFormatEdit, &QLineEdit::textChanged, previewTimeline, &ZzTimeline::setTimestampFormat);
    QObject::connect(
        timestampWidthSlider, &QSlider::valueChanged, previewTimeline, &ZzTimeline::setTimestampWidth);
    QObject::connect(nodeSizeSlider, &QSlider::valueChanged, previewTimeline, &ZzTimeline::setNodeSize);
    QObject::connect(lineWidthSlider, &QSlider::valueChanged, previewTimeline,
        [=](int value) { previewTimeline->setLineWidth(value / 2.0); });
    QObject::connect(itemSpacingSlider, &QSlider::valueChanged, previewTimeline, &ZzTimeline::setItemSpacing);
    QObject::connect(horizontalItemWidthSlider, &QSlider::valueChanged, previewTimeline,
        &ZzTimeline::setHorizontalItemWidth);
    QObject::connect(
        contentPaddingSlider, &QSlider::valueChanged, previewTimeline, &ZzTimeline::setContentPadding);
    QObject::connect(
        titleFontSizeSlider, &QSlider::valueChanged, previewTimeline, &ZzTimeline::setTitleFontPixelSize);
    QObject::connect(descriptionFontSizeSlider, &QSlider::valueChanged, previewTimeline,
        &ZzTimeline::setDescriptionFontPixelSize);
    QObject::connect(timestampFontSizeSlider, &QSlider::valueChanged, previewTimeline,
        &ZzTimeline::setTimestampFontPixelSize);
    QObject::connect(
        animationDurationSlider, &QSlider::valueChanged, previewTimeline, &ZzTimeline::setAnimationDuration);

    orderTimeline->setObjectName(QStringLiteral("zzTimeline_orderTimeline"));
    updateTimeline->setObjectName(QStringLiteral("zzTimeline_updateTimeline"));
    systemTimeline->setObjectName(QStringLiteral("zzTimeline_systemTimeline"));
    horizontalTimeline->setObjectName(QStringLiteral("zzTimeline_horizontalTimeline"));
    previewTimeline->setObjectName(QStringLiteral("zzTimeline_previewTimeline"));
    editorTabs->setObjectName(QStringLiteral("zzTimeline_editorTabs"));
    orientationCombo->setObjectName(QStringLiteral("zzTimeline_orientationCombo"));
    layoutModeCombo->setObjectName(QStringLiteral("zzTimeline_layoutModeCombo"));
    reverseCheck->setObjectName(QStringLiteral("zzTimeline_reverseCheck"));
    timestampVisibleCheck->setObjectName(QStringLiteral("zzTimeline_timestampVisibleCheck"));
    descriptionVisibleCheck->setObjectName(QStringLiteral("zzTimeline_descriptionVisibleCheck"));
    animationEnabledCheck->setObjectName(QStringLiteral("zzTimeline_animationEnabledCheck"));
    timestampFormatEdit->setObjectName(QStringLiteral("zzTimeline_timestampFormatEdit"));
    timestampWidthSlider->setObjectName(QStringLiteral("zzTimeline_timestampWidthSlider"));
    nodeSizeSlider->setObjectName(QStringLiteral("zzTimeline_nodeSizeSlider"));
    lineWidthSlider->setObjectName(QStringLiteral("zzTimeline_lineWidthSlider"));
    itemSpacingSlider->setObjectName(QStringLiteral("zzTimeline_itemSpacingSlider"));
    horizontalItemWidthSlider->setObjectName(QStringLiteral("zzTimeline_horizontalItemWidthSlider"));
    contentPaddingSlider->setObjectName(QStringLiteral("zzTimeline_contentPaddingSlider"));
    titleFontSizeSlider->setObjectName(QStringLiteral("zzTimeline_titleFontSizeSlider"));
    descriptionFontSizeSlider->setObjectName(QStringLiteral("zzTimeline_descriptionFontSizeSlider"));
    timestampFontSizeSlider->setObjectName(QStringLiteral("zzTimeline_timestampFontSizeSlider"));
    animationDurationSlider->setObjectName(QStringLiteral("zzTimeline_animationDurationSlider"));
    lineColorButton->setObjectName(QStringLiteral("zzTimeline_lineColorButton"));
    buildTimelineEventEditor(eventForm, previewTimeline);
    auto* automatic = new QCheckBox(zzGaugeText("自动"), stylePage);
    automatic->setObjectName(QStringLiteral("zzTimelineAutoLineColor"));
    automatic->setChecked(true);
    lineColorButton->setEnabled(false);
    lineColorButton->themeColor = [=] { return timelineRailColor(previewTimeline); };
    lineColorButton->setSelectedColor(lineColorButton->themeColor());
    styleForm->addRow(automatic);
    QObject::connect(lineColorButton, &ZzExampleColorButton::selectedColorChanged, previewTimeline,
        [=](const QColor& color) {
            if (!automatic->isChecked())
                previewTimeline->setLineColor(color);
        });
    QObject::connect(automatic, &QCheckBox::toggled, previewTimeline, [=](bool checked) {
        previewTimeline->setLineColor(checked ? QColor() : lineColorButton->selectedColor());
        lineColorButton->setEnabled(!checked);
        const QSignalBlocker blocker(lineColorButton);
        lineColorButton->setSelectedColor(lineColorButton->themeColor());
    });
    auto* disabled = new QCheckBox(zzGaugeText("禁用状态"), stylePage);
    disabled->setObjectName(QStringLiteral("zzTimelineDisabled"));
    QObject::connect(disabled, &QCheckBox::toggled, previewTimeline,
        [=](bool checked) { previewTimeline->setEnabled(!checked); });
    styleForm->addRow(disabled);
    auto* reset = new QPushButton(zzGaugeText("恢复默认属性"), propertiesCard);
    reset->setObjectName(QStringLiteral("zzTimelineReset"));
    QObject::connect(reset, &QPushButton::clicked, previewTimeline, [=] {
        orientationCombo->setCurrentIndex(0);
        layoutModeCombo->setCurrentIndex(layoutModeCombo->findData(ZzTimeline::Alternating));
        reverseCheck->setChecked(false);
        timestampVisibleCheck->setChecked(true);
        descriptionVisibleCheck->setChecked(true);
        animationEnabledCheck->setChecked(true);
        timestampFormatEdit->setText(QStringLiteral("HH:mm"));
        timestampWidthSlider->setValue(116);
        nodeSizeSlider->setValue(14);
        lineWidthSlider->setValue(4);
        itemSpacingSlider->setValue(18);
        horizontalItemWidthSlider->setValue(240);
        contentPaddingSlider->setValue(12);
        titleFontSizeSlider->setValue(0);
        descriptionFontSizeSlider->setValue(0);
        timestampFontSizeSlider->setValue(0);
        animationDurationSlider->setValue(1400);
        automatic->setChecked(true);
        disabled->setChecked(false);
        resetTimelineEvents(previewTimeline);
        editorTabs->setCurrentIndex(0);
    });
    propertiesLayout->addWidget(reset, 0, Qt::AlignRight);
    propertyPreviewLayout->addWidget(editorTabs, 1);
    propertiesLayout->addLayout(propertyPreviewLayout);

    auto* code = new QPlainTextEdit(propertiesCard);
    code->setReadOnly(true);
    code->setMaximumHeight(170);
    code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    code->setPlainText(QStringLiteral("auto *timeline = new ZzFluentUI::ZzTimeline(parent);\n"
                                      "timeline->setOrientation(Qt::Horizontal);\n"
                                      "timeline->setLayoutMode(ZzFluentUI::ZzTimeline::Alternating);\n"
                                      "timeline->setHorizontalItemWidth(220);\n"
                                      "auto *event = timeline->addEvent(QDateTime::currentDateTime(),\n"
                                      "    tr(\"Task completed\"), tr(\"Results saved.\"),\n"
                                      "    ZzFluentUI::ZzTimelineEvent::Completed);\n"
                                      "event->setColor(QColor(\"#107C10\"));"));
    propertiesLayout->addWidget(code);
    mainLayout->addWidget(propertiesCard);
    mainLayout->addStretch();
}

void ZzExampleShowcasePagePrivate::buildTimeline(QVBoxLayout* mainLayout, QWidget* content)
{
    ZzExampleTimelinePage::build(mainLayout, content);
}

} // namespace ZzExample
