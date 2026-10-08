#include "ZzExampleRadialGaugeHelpers.h"
#include "ZzExampleShowcasePagePrivate.h"

#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzSpinBox.h>

namespace ZzExample {
namespace {
using Ring = ZzFluentUI::ZzProgressRing;

Ring *makeRing(QWidget *parent, int value, int side = 80)
{
    auto *ring = new Ring(parent);
    ring->setRange(0, 100);
    ring->setValue(value);
    ring->setFixedSize(side, side);
    return ring;
}

QSpinBox *makeSpin(QWidget *parent, const char *name, int minimum, int maximum, int value)
{
    auto *spin = new ZzFluentUI::ZzSpinBox(parent);
    spin->setObjectName(QString::fromLatin1(name));
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    return spin;
}

/** @brief 编辑明确颜色或保持调色板跟随，不把主题色写成固定覆盖。 */
void addRingColor(QFormLayout *form, Ring *ring, const char *property, const QString &label,
    QPalette::ColorRole fallback, QList<QCheckBox *> &automaticChecks)
{
    auto *row = new QWidget(form->parentWidget());
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *button = new ZzExampleColorButton(row);
    button->setObjectName(QStringLiteral("zzProgressRingColor_%1").arg(QString::fromLatin1(property)));
    button->themeColor = [=] {
        const QColor configured = ring->property(property).value<QColor>();
        if (configured.isValid()) return configured;
        QColor color = ring->palette().color(QPalette::Active, fallback);
        if (qstrcmp(property, "titleColor") == 0) color.setAlphaF(color.alphaF() * 0.72F);
        return color;
    };
    button->setSelectedColor(button->themeColor());
    auto *automatic = new QCheckBox(zzGaugeText("自动"), row);
    automatic->setObjectName(QStringLiteral("zzProgressRingAuto_%1").arg(QString::fromLatin1(property)));
    automatic->setChecked(true);
    button->setEnabled(false);
    automaticChecks.append(automatic);
    layout->addWidget(button);
    layout->addWidget(automatic);
    layout->addStretch();
    QObject::connect(button, &ZzExampleColorButton::selectedColorChanged, ring, [=](const QColor &color) {
        if (!automatic->isChecked()) ring->setProperty(property, color);
    });
    QObject::connect(automatic, &QCheckBox::toggled, ring, [=](bool enabled) {
        ring->setProperty(property, enabled ? QColor() : button->selectedColor());
        button->setEnabled(!enabled);
        const QSignalBlocker blocker(button);
        button->setSelectedColor(button->themeColor());
    });
    form->addRow(label, row);
}
} // namespace

void ZzExampleShowcasePagePrivate::buildProgressRing(QVBoxLayout *mainLayout, QWidget *content)
{
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);
    auto *description = new QLabel(zzGaugeText("ZzProgressRing 继承 QProgressBar，复用范围、数值和格式，并提供独立的中央标题、数值样式与自定义中心控件。"), content);
    description->setWordWrap(true);
    mainLayout->addWidget(description);
    auto *states = makeCard(content);
    auto *statesLayout = new QVBoxLayout(states);
    statesLayout->setContentsMargins(16, 16, 16, 16);
    statesLayout->setSpacing(12);
    statesLayout->addWidget(makeSectionTitle(zzGaugeText("基本状态"), states));
    auto *samples = new QHBoxLayout;
    samples->setSpacing(24);
    samples->addStretch();
    for (int index = 0; index < 6; ++index) {
        auto *host = new QWidget(states);
        auto *layout = new QVBoxLayout(host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto *ring = makeRing(host, index < 5 ? index * 25 : 0);
        ring->setObjectName(QStringLiteral("zzProgressRingSample%1").arg(index));
        if (index == 5) {
            ring->setRange(0, 0);
            ring->setTextVisible(false);
        }
        auto *label = new QLabel(index < 5 ? QString::number(index * 25) + QLatin1Char('%')
                                         : zzGaugeText("不确定"), host);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(ring, 0, Qt::AlignHCenter);
        layout->addWidget(label);
        samples->addWidget(host);
    }
    samples->addStretch();
    statesLayout->addLayout(samples);
    mainLayout->addWidget(states);

    auto *card = makeCard(content);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeSectionTitle(zzGaugeText("属性"), card));
    auto *previewLayout = new QHBoxLayout;
    previewLayout->setSpacing(28);
    auto *ring = makeRing(card, 65, 156);
    ring->setObjectName(QStringLiteral("zzProgressRingPreview"));
    ring->setTitle(zzGaugeText("已完成"));
    QFont titleFont = ring->font();
    titleFont.setPixelSize(12);
    ring->setTitleFont(titleFont);
    QFont valueFont = ring->font();
    valueFont.setPixelSize(24);
    valueFont.setWeight(QFont::DemiBold);
    ring->setValueFont(valueFont);
    auto *previewHost = new QWidget(card);
    auto *previewHostLayout = new QVBoxLayout(previewHost);
    previewHostLayout->setContentsMargins(0, 0, 0, 0);
    previewHostLayout->addWidget(ring, 0, Qt::AlignHCenter | Qt::AlignTop);
    previewHostLayout->addStretch();
    previewLayout->addWidget(previewHost, 1);
    auto *formHost = new QWidget(card);
    auto *form = new QFormLayout(formHost);
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    auto *value = makeValueSlider(formHost, 0, 100, 65);
    value->setObjectName(QStringLiteral("zzProgressRingValue"));
    auto *duration = makeSpin(formHost, "zzProgressRingDuration", 200, 5000, 800);
    duration->setSingleStep(100);
    duration->setSuffix(QStringLiteral(" ms"));
    auto *thickness = new ZzFluentUI::ZzDoubleSpinBox(formHost);
    thickness->setObjectName(QStringLiteral("zzProgressRingThickness"));
    thickness->setRange(1.0, 32.0);
    thickness->setDecimals(1);
    thickness->setSingleStep(0.5);
    thickness->setSuffix(QStringLiteral(" px"));
    thickness->setValue(6.0);
    auto *title = new QLineEdit(ring->title(), formHost);
    title->setObjectName(QStringLiteral("zzProgressRingTitle"));
    auto *format = new QLineEdit(ring->format(), formHost);
    format->setObjectName(QStringLiteral("zzProgressRingFormat"));
    auto *titleSize = makeSpin(formHost, "zzProgressRingTitleSize", 6, 72, 12);
    auto *valueSize = makeSpin(formHost, "zzProgressRingValueSize", 6, 96, 24);
    auto *spacing = makeSpin(formHost, "zzProgressRingSpacing", 0, 40, 4);
    auto *busy = new QCheckBox(zzGaugeText("不确定进度"), formHost);
    busy->setObjectName(QStringLiteral("zzProgressRingBusy"));
    auto *textVisible = new QCheckBox(zzGaugeText("显示中央文字"), formHost);
    textVisible->setObjectName(QStringLiteral("zzProgressRingTextVisible"));
    textVisible->setChecked(true);
    auto *disabled = new QCheckBox(zzGaugeText("禁用状态"), formHost);
    disabled->setObjectName(QStringLiteral("zzProgressRingDisabled"));
    auto *custom = new QCheckBox(zzGaugeText("使用自定义中心控件"), formHost);
    custom->setObjectName(QStringLiteral("zzProgressRingCustom"));
    auto *reset = new QPushButton(zzGaugeText("恢复默认属性"), formHost);
    reset->setObjectName(QStringLiteral("zzProgressRingReset"));
    form->addRow(zzGaugeText("进度"), value);
    form->addRow(zzGaugeText("不确定动画周期"), duration);
    form->addRow(zzGaugeText("环与 Track 宽度"), thickness);
    QList<QCheckBox *> automaticChecks;
    addRingColor(form, ring, "ringColor", zzGaugeText("进度环颜色"), QPalette::Accent, automaticChecks);
    addRingColor(form, ring, "trackColor", zzGaugeText("Track 颜色"), QPalette::Mid, automaticChecks);
    form->addRow(zzGaugeText("标题"), title);
    form->addRow(zzGaugeText("数值格式"), format);
    form->addRow(zzGaugeText("标题字号"), titleSize);
    form->addRow(zzGaugeText("数值字号"), valueSize);
    form->addRow(zzGaugeText("文字间距"), spacing);
    addRingColor(form, ring, "titleColor", zzGaugeText("标题颜色"), QPalette::Text, automaticChecks);
    addRingColor(form, ring, "valueColor", zzGaugeText("数值颜色"), QPalette::Text, automaticChecks);
    form->addRow(busy);
    form->addRow(textVisible);
    form->addRow(disabled);
    form->addRow(custom);
    form->addRow(reset);
    QObject::connect(value, &QSlider::valueChanged, ring, &QProgressBar::setValue);
    QObject::connect(duration, &QSpinBox::valueChanged, ring, &Ring::setIndeterminateDuration);
    QObject::connect(thickness, &QDoubleSpinBox::valueChanged, ring, &Ring::setThickness);
    QObject::connect(title, &QLineEdit::textChanged, ring, &Ring::setTitle);
    QObject::connect(format, &QLineEdit::textChanged, ring, &QProgressBar::setFormat);
    QObject::connect(titleSize, &QSpinBox::valueChanged, ring, [ring](int size) {
        QFont font = ring->titleFont();
        font.setPixelSize(size);
        ring->setTitleFont(font);
    });
    QObject::connect(valueSize, &QSpinBox::valueChanged, ring, [ring](int size) {
        QFont font = ring->valueFont();
        font.setPixelSize(size);
        ring->setValueFont(font);
    });
    QObject::connect(spacing, &QSpinBox::valueChanged, ring, &Ring::setTextSpacing);
    QObject::connect(busy, &QCheckBox::toggled, ring, [=](bool checked) {
        ring->setRange(0, checked ? 0 : 100);
        if (!checked) ring->setValue(value->value());
        value->setEnabled(!checked);
    });
    QObject::connect(textVisible, &QCheckBox::toggled, ring, &Ring::setTextVisible);
    QObject::connect(disabled, &QCheckBox::toggled, ring, [ring](bool checked) { ring->setEnabled(!checked); });
    QObject::connect(custom, &QCheckBox::toggled, ring, [=](bool checked) {
        if (!checked) {
            ring->setCenterWidget(nullptr);
            return;
        }
        auto *center = new QWidget;
        auto *centerLayout = new QVBoxLayout(center);
        centerLayout->setContentsMargins(0, 0, 0, 0);
        centerLayout->setSpacing(4);
        auto *label = new QLabel(QString::number(ring->value()) + QLatin1Char('%'), center);
        label->setAlignment(Qt::AlignCenter);
        QFont font = label->font();
        font.setPixelSize(22);
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
        auto *restart = new QPushButton(zzGaugeText("重新开始"), center);
        restart->setObjectName(QStringLiteral("zzProgressRingRestart"));
        centerLayout->addStretch();
        centerLayout->addWidget(label);
        centerLayout->addWidget(restart, 0, Qt::AlignHCenter);
        centerLayout->addStretch();
        QObject::connect(ring, &QProgressBar::valueChanged, label,
            [label](int progress) { label->setText(QString::number(progress) + QLatin1Char('%')); });
        QObject::connect(restart, &QPushButton::clicked, ring, [=] {
            busy->setChecked(false);
            value->setValue(0);
        });
        ring->setCenterWidget(center);
    });
    QObject::connect(reset, &QPushButton::clicked, ring, [=] {
        busy->setChecked(false);
        textVisible->setChecked(true);
        disabled->setChecked(false);
        custom->setChecked(false);
        value->setValue(65);
        duration->setValue(800);
        thickness->setValue(6.0);
        title->setText(zzGaugeText("已完成"));
        format->setText(QStringLiteral("%p%"));
        titleSize->setValue(12);
        valueSize->setValue(24);
        spacing->setValue(4);
        for (auto *automatic : automaticChecks) automatic->setChecked(true);
    });
    previewLayout->addWidget(formHost, 1);
    layout->addLayout(previewLayout);
    auto *code = new QPlainTextEdit(card);
    code->setReadOnly(true);
    code->setMaximumHeight(128);
    code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    code->setPlainText(QStringLiteral(
        "auto *ring = new ZzFluentUI::ZzProgressRing(parent);\n"
        "ring->setRange(0, 100);\n"
        "ring->setValue(65);\n"
        "ring->setTitle(tr(\"Completed\"));\n"
        "ring->setFormat(QStringLiteral(\"%p%\"));\n"
        "ring->setThickness(8.5);\n"
        "// Optional: ring->setCenterWidget(customWidget);"));
    layout->addWidget(code);
    mainLayout->addWidget(card);
    mainLayout->addStretch();
}
} // namespace ZzExample
