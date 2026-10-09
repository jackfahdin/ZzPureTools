#include "ZzExampleAudioMeterSource.h"
#include "ZzExampleColorButton.h"
#include "ZzExampleCustomCard.h"
#include "ZzExampleShowcasePagePrivate.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <ZzFluentUI/ZzAudioLevelMeter.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzExample {
namespace {
using Meter = ZzFluentUI::ZzAudioLevelMeter;
QString zzAudioText(const char *text) { return QCoreApplication::translate("ZzPureToolsExample", text); }

/** @brief 页面默认色与参考一致；切主题时保留用户已编辑的颜色。 */
class ZzAudioMeterThemeBinding final : public QObject
{
public:
    explicit ZzAudioMeterThemeBinding(Meter *meter)
        : QObject(meter)
        , meter_(meter)
    {
        meter->installEventFilter(this);
        apply();
    }
    void bind(ZzExampleColorButton *button, const QByteArray &property)
    {
        buttons_.append({ button, property });
        button->setSelectedColor(meter_->property(property.constData()).value<QColor>());
        QObject::connect(
            button, &ZzExampleColorButton::selectedColorChanged, this, [this, property](const QColor &color) {
                overrides_.insert(property);
                meter_->setProperty(property.constData(), color);
            });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
            apply();
        return QObject::eventFilter(watched, event);
    }

private:
    void apply()
    {
        const auto palette = meter_->palette();
        const bool dark = palette.color(QPalette::Window).lightness() < 128;
        const auto *style = qobject_cast<const ZzFluentUI::ZzFluentStyle *>(meter_->style());
        const bool hc = style && style->themeSnapshot()->mode() == ZzFluentUI::ZzThemeMode::HighContrast;
        QColor inactive = palette.color(QPalette::Text);
        inactive.setAlphaF(0.35f);
        const std::pair<const char *, QColor> colors[] = {
            { "backgroundColor", hc ? palette.color(QPalette::Base) : QColor(dark ? "#111111" : "#F4F4F4") },
            { "activeColor", hc ? palette.color(QPalette::Highlight) : QColor(dark ? "#FF9F2D" : "#C85D00") },
            { "inactiveColor", hc ? inactive : QColor(dark ? "#292929" : "#D8D8D8") },
            { "warningColor",
                hc ? palette.color(QPalette::Highlight) : QColor(dark ? "#FFD166" : "#A15C00") },
            { "clipColor", hc ? palette.color(QPalette::Text) : QColor(dark ? "#FF5A5F" : "#C42B1C") },
            { "scaleColor", hc ? palette.color(QPalette::Text) : QColor(dark ? "#A0A0A0" : "#5D5D5D") },
            { "peakColor", hc ? palette.color(QPalette::Text) : QColor(dark ? "#FFFFFF" : "#202020") }
        };
        for (const auto &[property, color] : colors)
            if (!overrides_.contains(property))
                meter_->setProperty(property, color);
        for (const auto &[button, property] : buttons_) {
            if (!button)
                continue;
            const QSignalBlocker blocker(button);
            button->setSelectedColor(meter_->property(property.constData()).value<QColor>());
        }
    }
    Meter *meter_;
    QSet<QByteArray> overrides_;
    QList<std::pair<QPointer<ZzExampleColorButton>, QByteArray>> buttons_;
};

QVBoxLayout *zzAudioCard(QVBoxLayout *outer, QWidget *parent, const char *title)
{
    auto *card = new ZzExampleCustomCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    auto *heading = new QLabel(zzAudioText(title), card);
    auto font = heading->font();
    font.setPixelSize(14);
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    outer->addWidget(card);
    return layout;
}

QFormLayout *zzAudioForm(ZzFluentUI::ZzTabWidget *tabs, const char *title)
{
    auto *page = new QWidget(tabs);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(12, 12, 12, 12);
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tabs->addTab(page, zzAudioText(title));
    return form;
}
/** @brief 装配音频电平表示例页内容，不持有控件所有权。 */
class ZzExampleAudioLevelMeterPage final
{
public:
    static void build(ZzExampleShowcasePagePrivate *host, QVBoxLayout *layout, QWidget *parent);
};

}

void ZzExampleAudioLevelMeterPage::build(ZzExampleShowcasePagePrivate *host, QVBoxLayout *layout, QWidget *parent)
{
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);
    auto *description = new QLabel(zzAudioText("只读的实时音频电平表。接收每个声道的 dBFS "
                                               "或线性幅度，支持单声道、立体声、峰值保持、衰减和超时归零。"),
        parent);
    description->setWordWrap(true);
    layout->addWidget(description);
    const int transportIndex = layout->count();

    auto *examples = zzAudioCard(layout, parent, "单声道与立体声");
    auto *row = new QHBoxLayout;
    row->setSpacing(24);
    row->addStretch();
    const auto sample = [parent, row](const char *title, const char *name, int channels, QSize size) {
        auto *column = new QVBoxLayout;
        auto *label = new QLabel(QString::fromLatin1(title), parent);
        label->setAlignment(Qt::AlignCenter);
        column->addWidget(label);
        auto *meter = new Meter(parent);
        meter->setObjectName(QString::fromLatin1(name));
        meter->setAccessibleName(QString::fromLatin1(title));
        meter->setChannelCount(channels);
        meter->setScalePosition(channels == 1 ? Meter::RightScale : Meter::CenterScale);
        meter->setChannelLabelsVisible(channels != 1);
        meter->setFixedSize(size);
        new ZzAudioMeterThemeBinding(meter);
        column->addWidget(meter);
        row->addLayout(column);
        return meter;
    };
    auto *mono = sample("Mono", "zzAudioMono", 1, QSize(105, 330));
    auto *stereo = sample("Stereo", "zzAudioStereo", 2, QSize(190, 330));
    row->addStretch();
    examples->addLayout(row);

    auto *properties = zzAudioCard(layout, parent, "实时属性");
    auto *editorRow = new QHBoxLayout;
    editorRow->setSpacing(28);
    auto *preview = new Meter(parent);
    preview->setObjectName(QStringLiteral("zzAudioPreview"));
    preview->setAccessibleName(zzAudioText("音频电平预览"));
    preview->setScalePosition(Meter::CenterScale);
    preview->setCustomScaleValues({ 0, -3, -6, -12, -24, -36, -48, -60 });
    preview->setMinimumSize(220, 390);
    preview->setMaximumWidth(300);
    auto *theme = new ZzAudioMeterThemeBinding(preview);
    editorRow->addWidget(preview, 1, Qt::AlignHCenter | Qt::AlignTop);
    auto *tabs = new ZzFluentUI::ZzTabWidget(parent);
    tabs->setObjectName(QStringLiteral("zzAudioPropertyTabs"));
    tabs->fluentTabBar()->setAppearance(ZzFluentUI::ZzTabBarAppearance::PivotSlide);
    tabs->fluentTabBar()->newTabButton()->hide();
    tabs->fluentTabBar()->setTearOffEnabled(false);
    tabs->fluentTabBar()->setTabTransferEnabled(false);
    tabs->fluentTabBar()->setContextMenuPolicy(Qt::PreventContextMenu);
    tabs->setMinimumWidth(390);
    tabs->setMovable(false);
    auto *basic = zzAudioForm(tabs, "基础");
    auto *scale = zzAudioForm(tabs, "刻度");
    auto *animation = zzAudioForm(tabs, "动画");
    auto *colors = zzAudioForm(tabs, "颜色");
    editorRow->addWidget(tabs, 1);
    properties->addLayout(editorRow);

    const auto addInt = [preview](QFormLayout *form, const char *label, const char *id, int minimum,
                            int maximum, int value, auto setter) {
        auto *spin = new QSpinBox(form->parentWidget());
        spin->setObjectName(QString::fromLatin1(id));
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        form->addRow(zzAudioText(label), spin);
        QObject::connect(spin, &QSpinBox::valueChanged, preview, setter);
        return spin;
    };
    const auto addDouble = [preview](QFormLayout *form, const char *label, const char *id, double minimum,
                               double maximum, double value, auto setter, const char *suffix) {
        auto *spin = new QDoubleSpinBox(form->parentWidget());
        spin->setObjectName(QString::fromLatin1(id));
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        spin->setSuffix(QString::fromLatin1(suffix));
        form->addRow(zzAudioText(label), spin);
        QObject::connect(spin, &QDoubleSpinBox::valueChanged, preview, setter);
        return spin;
    };
    const auto addCheck
        = [preview](QFormLayout *form, const char *label, const char *id, bool value, auto setter) {
              auto *check = new QCheckBox(zzAudioText(label), form->parentWidget());
              check->setObjectName(QString::fromLatin1(id));
              check->setChecked(value);
              form->addRow(check);
              QObject::connect(check, &QCheckBox::toggled, preview, setter);
              return check;
          };
    addInt(basic, "声道数", "zzAudioChannels", 1, 8, 2, &Meter::setChannelCount);
    addInt(basic, "分段数量", "zzAudioSegments", 2, 120, 30, &Meter::setSegmentCount);
    addDouble(basic, "最小电平", "zzAudioMinimum", -160, -1, -60, &Meter::setMinimumDecibels, " dB");
    auto *warning
        = addDouble(basic, "警告电平", "zzAudioWarning", -160, 24, -12, &Meter::setWarningDecibels, " dB");
    auto *clip = addDouble(basic, "过载电平", "zzAudioClip", -160, 24, -3, &Meter::setClipDecibels, " dB");
    QObject::connect(preview, &Meter::warningDecibelsChanged, warning, &QDoubleSpinBox::setValue);
    QObject::connect(preview, &Meter::clipDecibelsChanged, clip, &QDoubleSpinBox::setValue);
    // 钳制后值未变化时控件不会发通知，编辑器仍须回读真实值。
    QObject::connect(warning, &QDoubleSpinBox::valueChanged, preview, [preview, warning] {
        const QSignalBlocker blocker(warning);
        warning->setValue(preview->warningDecibels());
    });
    QObject::connect(clip, &QDoubleSpinBox::valueChanged, preview, [preview, clip] {
        const QSignalBlocker blocker(clip);
        clip->setValue(preview->clipDecibels());
    });
    addCheck(basic, "显示声道标签", "zzAudioLabels", true, &Meter::setChannelLabelsVisible);

    auto *position = new QComboBox(scale->parentWidget());
    position->setObjectName(QStringLiteral("zzAudioScalePosition"));
    for (const char *text : { "隐藏", "左侧", "右侧", "中间（立体声）" })
        position->addItem(zzAudioText(text));
    position->setCurrentIndex(int(Meter::CenterScale));
    scale->addRow(zzAudioText("dB 刻度"), position);
    QObject::connect(position, &QComboBox::currentIndexChanged, preview,
        [preview](int value) { preview->setScalePosition(static_cast<Meter::ZzMeterScalePosition>(value)); });
    auto *mode = new QComboBox(scale->parentWidget());
    mode->setObjectName(QStringLiteral("zzAudioScaleMode"));
    for (const char *text : { "按间隔", "固定数量", "自定义数值" })
        mode->addItem(zzAudioText(text));
    scale->addRow(zzAudioText("刻度生成"), mode);
    QObject::connect(mode, &QComboBox::currentIndexChanged, preview,
        [preview](int value) { preview->setScaleMode(static_cast<Meter::ZzMeterScaleMode>(value)); });
    auto *ticks = addInt(scale, "刻度数量", "zzAudioTicks", 2, 64, 7, &Meter::setScaleTickCount);
    ticks->setEnabled(false);
    QObject::connect(mode, &QComboBox::currentIndexChanged, ticks,
        [ticks](int value) { ticks->setEnabled(value == int(Meter::FixedTickCount)); });
    addInt(scale, "小数位数", "zzAudioPrecision", 0, 3, 0, &Meter::setScalePrecision);
    auto *unit = new QLineEdit(preview->scaleUnit(), scale->parentWidget());
    unit->setObjectName(QStringLiteral("zzAudioUnit"));
    scale->addRow(zzAudioText("刻度单位"), unit);
    QObject::connect(unit, &QLineEdit::textChanged, preview, &Meter::setScaleUnit);
    addCheck(scale, "在刻度后显示单位", "zzAudioUnitVisible", false, &Meter::setScaleUnitVisible);
    addCheck(scale, "显示短刻度线", "zzAudioTickMarks", false, &Meter::setScaleTickMarksVisible);

    addDouble(animation, "衰减速度", "zzAudioDecay", 0, 200, 36, &Meter::setDecayRate, " dB/s");
    auto *hold = addInt(animation, "峰值保持", "zzAudioHold", 0, 10000, 1000, &Meter::setPeakHoldDuration);
    hold->setSuffix(QStringLiteral(" ms"));
    addCheck(animation, "显示峰值保持", "zzAudioPeaks", true, &Meter::setPeakHoldEnabled);
    auto *resetPeaks = new QPushButton(zzAudioText("重置峰值"), animation->parentWidget());
    animation->addRow(resetPeaks);
    QObject::connect(resetPeaks, &QPushButton::clicked, preview, &Meter::resetPeaks);

    auto *colorMode = new QComboBox(colors->parentWidget());
    colorMode->setObjectName(QStringLiteral("zzAudioColorMode"));
    for (const char *text : { "统一颜色", "警告/过载分区", "连续渐变" })
        colorMode->addItem(zzAudioText(text));
    colors->addRow(zzAudioText("颜色模式"), colorMode);
    QObject::connect(colorMode, &QComboBox::currentIndexChanged, preview,
        [preview](int value) { preview->setColorMode(static_cast<Meter::ZzMeterColorMode>(value)); });
    const std::pair<const char *, const char *> colorProperties[] = { { "激活颜色", "activeColor" },
        { "未激活颜色", "inactiveColor" }, { "背景颜色", "backgroundColor" }, { "警告颜色", "warningColor" },
        { "过载颜色", "clipColor" } };
    for (const auto &[label, property] : colorProperties) {
        auto *button = new ZzExampleColorButton(colors->parentWidget());
        button->setObjectName(QStringLiteral("zzAudioColor_") + QString::fromLatin1(property));
        button->setAccessibleName(zzAudioText(label));
        theme->bind(button, property);
        colors->addRow(zzAudioText(label), button);
    }
    auto *source = new ZzExampleAudioMeterSource(host->q_ptr, mono, stereo, preview);
    layout->insertWidget(transportIndex, source->createTransport(parent));
}

void ZzExampleShowcasePagePrivate::buildAudioLevelMeter(QVBoxLayout *layout, QWidget *parent)
{
    ZzExampleAudioLevelMeterPage::build(this, layout, parent);
}

} // namespace ZzExample
