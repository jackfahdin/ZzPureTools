#include "ZzExampleRadialGaugeHelpers.h"
#include "ZzExampleShowcasePagePrivate.h"

#include <ZzFluentUI/ZzLiquidGauge.h>
#include <array>

namespace ZzExample {
namespace {
using Gauge = ZzFluentUI::ZzLiquidGauge;

QWidget *propertyPage(ZzFluentUI::ZzTabWidget *tabs, QFormLayout *&form)
{
    auto *page = new QWidget(tabs);
    form = new QFormLayout(page);
    form->setContentsMargins(12, 12, 12, 12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    return page;
}

/** @brief 自动色始终跟随属性回退，取消自动时可直接固定当前色。 */
void addColorRow(QFormLayout *form, Gauge *gauge, const char *property,
    const QString &label, QPalette::ColorRole fallback, QList<QCheckBox *> &automaticChecks)
{
    auto *row = new QWidget(form->parentWidget());
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *button = new ZzExampleColorButton(row);
    button->setObjectName(QStringLiteral("zzLiquidColor_%1").arg(QString::fromLatin1(property)));
    button->themeColor = [gauge, property, fallback] {
        const QColor configured = gauge->property(property).value<QColor>();
        if (configured.isValid()) return configured;
        if (qstrcmp(property, "outlineColor") == 0 && gauge->waveColor().isValid())
            return gauge->waveColor();
        return gauge->palette().color(QPalette::Active, fallback);
    };
    button->setSelectedColor(button->themeColor());
    auto *automatic = new QCheckBox(zzGaugeText("自动"), row);
    automatic->setObjectName(QStringLiteral("zzLiquidAuto_%1").arg(QString::fromLatin1(property)));
    automatic->setChecked(true);
    button->setEnabled(false);
    automaticChecks.append(automatic);
    layout->addWidget(button);
    layout->addWidget(automatic);
    layout->addStretch();
    QObject::connect(button, &ZzExampleColorButton::selectedColorChanged, gauge,
        [=](const QColor &color) { if (!automatic->isChecked()) gauge->setProperty(property, color); });
    QObject::connect(automatic, &QCheckBox::toggled, gauge, [=](bool enabled) {
        gauge->setProperty(property, enabled ? QColor() : button->selectedColor());
        button->setEnabled(!enabled);
        const QSignalBlocker blocker(button);
        button->setSelectedColor(button->themeColor());
    });
    // 外轮廓自动色也要立即反映水波颜色的手动修改。
    if (qstrcmp(property, "outlineColor") == 0) {
        QObject::connect(gauge, &Gauge::waveColorChanged, button, [button] {
            const QSignalBlocker blocker(button);
            button->setSelectedColor(button->themeColor());
        });
    }
    form->addRow(label, row);
}
} // namespace

void ZzExampleShowcasePagePrivate::buildLiquidGauge(QVBoxLayout *mainLayout, QWidget *content)
{
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);
    auto *description = new QLabel(zzGaugeText("参考 Ant Design Charts Liquid 的水波图控件，继承 QProgressBar，并提供圆形、矩形、水滴和三角形裁剪、双层水波与中心文本。"), content);
    description->setWordWrap(true);
    mainLayout->addWidget(description);

    auto *shapesCard = makeCard(content);
    auto *shapesLayout = new QVBoxLayout(shapesCard);
    shapesLayout->setContentsMargins(16, 16, 16, 16);
    shapesLayout->setSpacing(12);
    shapesLayout->addWidget(makeSectionTitle(zzGaugeText("内置形状"), shapesCard));
    auto *samplesLayout = new QHBoxLayout;
    samplesLayout->setSpacing(24);
    samplesLayout->addStretch();
    const std::array<const char *, 4> names{"圆形", "矩形", "水滴", "三角形"};
    const std::array<const char *, 4> colors{"#1677FF", "#13C2C2", "#722ED1", "#FA8C16"};
    QList<Gauge *> samples;
    for (int index = 0; index < 4; ++index) {
        auto *sample = new QWidget(shapesCard);
        auto *layout = new QVBoxLayout(sample);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto *gauge = new Gauge(sample);
        gauge->setObjectName(QStringLiteral("zzLiquidSample%1").arg(index));
        gauge->setRange(0, 100);
        gauge->setValue(60);
        gauge->setShape(static_cast<Gauge::Shape>(index));
        gauge->setWaveColor(QColor(QString::fromLatin1(colors.at(index))));
        gauge->setFixedSize(150, 150);
        samples.append(gauge);
        auto *label = new QLabel(zzGaugeText(names.at(index)), sample);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(gauge, 0, Qt::AlignHCenter);
        layout->addWidget(label);
        samplesLayout->addWidget(sample);
    }
    samplesLayout->addStretch();
    shapesLayout->addLayout(samplesLayout);
    auto *sharedLayout = new QHBoxLayout;
    sharedLayout->addWidget(new QLabel(zzGaugeText("公共数值"), shapesCard));
    auto *shared = makeValueSlider(shapesCard, 0, 100, 60);
    shared->setObjectName(QStringLiteral("zzLiquidSharedValue"));
    sharedLayout->addWidget(shared, 1);
    shapesLayout->addLayout(sharedLayout);
    for (auto *gauge : std::as_const(samples))
        QObject::connect(shared, &QSlider::valueChanged, gauge, &QProgressBar::setValue);
    mainLayout->addWidget(shapesCard);

    auto *card = makeCard(content);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeSectionTitle(zzGaugeText("实时属性"), card));
    auto *previewLayout = new QHBoxLayout;
    previewLayout->setSpacing(32);
    auto *gauge = new Gauge(card);
    gauge->setObjectName(QStringLiteral("zzLiquidPreview"));
    gauge->setRange(0, 100);
    gauge->setValue(68);
    gauge->setFixedSize(260, 260);
    previewLayout->addWidget(gauge, 0, Qt::AlignHCenter | Qt::AlignTop);
    auto *tabs = makePropertyTabs(card);
    tabs->setObjectName(QStringLiteral("zzLiquidPropertyTabs"));
    tabs->setMinimumWidth(440);
    tabs->setMinimumHeight(430);
    QFormLayout *basic = nullptr;
    QFormLayout *appearance = nullptr;
    auto *basicPage = propertyPage(tabs, basic);
    auto *appearancePage = propertyPage(tabs, appearance);
    tabs->addTab(basicPage, zzGaugeText("基础"));
    tabs->addTab(appearancePage, zzGaugeText("外观"));

    auto *value = makeValueSlider(basicPage, 0, 100, 68);
    value->setObjectName(QStringLiteral("zzLiquidValue"));
    auto *shape = new QComboBox(basicPage);
    shape->setObjectName(QStringLiteral("zzLiquidShape"));
    for (int index = 0; index < 4; ++index) shape->addItem(zzGaugeText(names.at(index)), index);
    auto *format = new QLineEdit(gauge->format(), basicPage);
    format->setObjectName(QStringLiteral("zzLiquidFormat"));
    auto *fontSize = makeValueSlider(basicPage, 0, 72, 0);
    auto *animation = new QCheckBox(zzGaugeText("播放水波动画"), basicPage);
    animation->setObjectName(QStringLiteral("zzLiquidAnimation"));
    animation->setChecked(true);
    auto *textVisible = new QCheckBox(zzGaugeText("显示中心文本"), basicPage);
    textVisible->setChecked(true);
    auto *disabled = new QCheckBox(zzGaugeText("禁用状态"), basicPage);
    basic->addRow(zzGaugeText("数值"), value);
    basic->addRow(zzGaugeText("形状"), shape);
    basic->addRow(zzGaugeText("文本格式"), format);
    basic->addRow(zzGaugeText("文本字号"), fontSize);
    basic->addRow(animation);
    basic->addRow(textVisible);
    basic->addRow(disabled);
    QObject::connect(value, &QSlider::valueChanged, gauge, &QProgressBar::setValue);
    QObject::connect(shape, &QComboBox::currentIndexChanged, gauge,
        [=](int index) { gauge->setShape(static_cast<Gauge::Shape>(shape->itemData(index).toInt())); });
    QObject::connect(format, &QLineEdit::textChanged, gauge, &QProgressBar::setFormat);
    QObject::connect(fontSize, &QSlider::valueChanged, gauge, &Gauge::setContentFontPixelSize);
    QObject::connect(animation, &QCheckBox::toggled, gauge, &Gauge::setAnimationEnabled);
    QObject::connect(textVisible, &QCheckBox::toggled, gauge, &QProgressBar::setTextVisible);
    QObject::connect(disabled, &QCheckBox::toggled, gauge, [gauge](bool checked) { gauge->setEnabled(!checked); });

    auto *amplitude = makeValueSlider(appearancePage, 0, 40, 12, 1, 4, 2, 1);
    amplitude->setObjectName(QStringLiteral("zzLiquidAmplitude"));
    auto *count = makeValueSlider(appearancePage, 1, 10, 3);
    auto *duration = makeValueSlider(appearancePage, 200, 6000, 2400, 100, 500);
    auto *opacity = makeValueSlider(appearancePage, 0, 100, 45, 1, 10, 100, 2);
    auto *width = makeValueSlider(appearancePage, 0, 40, 4, 1, 4, 2, 1);
    auto *distance = makeValueSlider(appearancePage, 0, 40, 6, 1, 4, 2, 1);
    appearance->addRow(zzGaugeText("波幅"), amplitude);
    appearance->addRow(zzGaugeText("波形数量"), count);
    appearance->addRow(zzGaugeText("动画周期"), duration);
    appearance->addRow(zzGaugeText("后层水波透明度"), opacity);
    appearance->addRow(zzGaugeText("轮廓宽度"), width);
    appearance->addRow(zzGaugeText("轮廓间距"), distance);
    connectScaledSlider(amplitude, gauge, 2, &Gauge::setWaveAmplitude);
    QObject::connect(count, &QSlider::valueChanged, gauge, &Gauge::setWaveCount);
    QObject::connect(duration, &QSlider::valueChanged, gauge, &Gauge::setWaveAnimationDuration);
    connectScaledSlider(opacity, gauge, 100, &Gauge::setSecondaryWaveOpacity);
    connectScaledSlider(width, gauge, 2, &Gauge::setOutlineWidth);
    connectScaledSlider(distance, gauge, 2, &Gauge::setOutlineDistance);
    QList<QCheckBox *> automaticChecks;
    addColorRow(appearance, gauge, "waveColor", zzGaugeText("水波颜色"), QPalette::Accent, automaticChecks);
    addColorRow(appearance, gauge, "backgroundColor", zzGaugeText("背景颜色"), QPalette::Base, automaticChecks);
    addColorRow(appearance, gauge, "outlineColor", zzGaugeText("轮廓颜色"), QPalette::Accent, automaticChecks);
    addColorRow(appearance, gauge, "textColor", zzGaugeText("液面上文字颜色"), QPalette::Text, automaticChecks);
    addColorRow(appearance, gauge, "submergedTextColor", zzGaugeText("液面下文字颜色"), QPalette::HighlightedText, automaticChecks);
    previewLayout->addWidget(tabs, 1);
    layout->addLayout(previewLayout);
    auto *reset = new QPushButton(zzGaugeText("恢复默认属性"), card);
    reset->setObjectName(QStringLiteral("zzLiquidReset"));
    layout->addWidget(reset, 0, Qt::AlignRight);
    QObject::connect(reset, &QPushButton::clicked, gauge, [=] {
        value->setValue(68);
        shape->setCurrentIndex(0);
        format->setText(QStringLiteral("%p%"));
        fontSize->setValue(0);
        animation->setChecked(true);
        textVisible->setChecked(true);
        disabled->setChecked(false);
        amplitude->setValue(12);
        count->setValue(3);
        duration->setValue(2400);
        opacity->setValue(45);
        width->setValue(4);
        distance->setValue(6);
        for (auto *automatic : automaticChecks) automatic->setChecked(true);
    });
    auto *code = new QPlainTextEdit(card);
    code->setReadOnly(true);
    code->setMaximumHeight(150);
    code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    code->setPlainText(QStringLiteral(
        "auto *gauge = new ZzFluentUI::ZzLiquidGauge(this);\n"
        "gauge->setRange(0, 100);\n"
        "gauge->setValue(68);\n"
        "gauge->setShape(ZzFluentUI::ZzLiquidGauge::CircleShape);\n"
        "gauge->setWaveAmplitude(6.0);\n"
        "gauge->setWaveCount(3);\n"
        "gauge->setWaveAnimationDuration(2400);"));
    layout->addWidget(code);
    mainLayout->addWidget(card);
    mainLayout->addStretch();
}
} // namespace ZzExample
