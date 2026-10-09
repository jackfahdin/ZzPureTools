#include "ZzExampleShowcasePagePrivate.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QPainter>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzRangeSlider.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzExample {
namespace {

QString zzRangeText(const char *text)
{
    return QCoreApplication::translate("ZzPureToolsExample", text);
}

/** @brief 绘制与参考 PageRangeSlider 一致的浅深色卡片表面。 */
class ZzRangeSliderCard final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        const auto *fluent = qobject_cast<const ZzFluentUI::ZzFluentStyle *>(style());
        const auto snapshot = fluent ? fluent->themeSnapshot() : nullptr;
        const bool highContrast = snapshot && snapshot->mode() == ZzFluentUI::ZzThemeMode::HighContrast;
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(highContrast ? palette().color(QPalette::WindowText)
                                    : QColor(dark ? "#252525" : "#e9e9e9"));
        QColor base = palette().color(QPalette::Base);
        if (base.alpha() == 0) base = palette().color(QPalette::Window);
        if (base.alpha() == 0) base = QColor(dark ? "#1e1e1e" : "#ffffff");
        const qreal alpha = (dark ? 13.0 : 179.0) / 255.0;
        painter.setBrush(highContrast ? base : QColor(
            qRound(base.red() * (1 - alpha) + 255 * alpha),
            qRound(base.green() * (1 - alpha) + 255 * alpha),
            qRound(base.blue() * (1 - alpha) + 255 * alpha)));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
};

QVBoxLayout *zzRangeCard(QVBoxLayout *outer, QWidget *parent, const char *title)
{
    auto *card = new ZzRangeSliderCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    auto *heading = new QLabel(zzRangeText(title), card);
    auto font = heading->font();
    font.setPixelSize(14);
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    outer->addWidget(card);
    return layout;
}

/** @brief 装配范围滑块示例页内容，不持有控件所有权。 */
class ZzExampleRangeSliderPage final
{
public:
    static void build(QVBoxLayout *layout, QWidget *parent);
};

} // namespace

void ZzExampleRangeSliderPage::build(QVBoxLayout *layout, QWidget *parent)
{
    using ZzFluentUI::ZzRangeSlider;
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);
    auto *hint = new QLabel(zzRangeText(
        "WinUI3 风格双滑块范围选择控件。可调整刻度显示、刻度间隔与 tracking（拖动时是否实时更新数值）。"), parent);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *demo = zzRangeCard(layout, parent, "演示");
    auto *slider = new ZzRangeSlider(Qt::Horizontal, parent);
    slider->setObjectName(QStringLiteral("rangeSelector"));
    slider->setAccessibleName(zzRangeText("范围选择"));
    slider->setRange(0, 100);
    slider->setValues(20, 80);
    slider->setTickPosition(true);
    slider->setTickInterval(10);
    slider->setTracking(true);
    slider->setValueTipEnabled(true);
    slider->setMinimumHeight(32);
    demo->addWidget(slider);
    auto *values = new QLabel(parent);
    values->setWordWrap(true);
    values->setObjectName(QStringLiteral("zzExampleRangeSliderValues"));
    const auto refresh = [values](int lower, int upper) {
        values->setText(zzRangeText("当前范围：%1 – %2").arg(lower).arg(upper));
    };
    refresh(20, 80);
    QObject::connect(slider, &ZzRangeSlider::valuesChanged, values, refresh);
    QObject::connect(slider, &ZzRangeSlider::sliderMoved, values, refresh);
    // 取消未提交的拖动后，恢复当前已提交范围。
    QObject::connect(slider, &ZzRangeSlider::sliderReleased, values,
        [slider, refresh] { refresh(slider->lowerValue(), slider->upperValue()); });
    demo->addWidget(values);

    auto *options = zzRangeCard(layout, parent, "属性");
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    options->addLayout(form);
    auto *ticks = new QCheckBox(zzRangeText("显示刻度"), parent);
    ticks->setObjectName(QStringLiteral("zzRangeTicks"));
    ticks->setChecked(true);
    auto *interval = new QSpinBox(parent);
    interval->setObjectName(QStringLiteral("zzRangeTickInterval"));
    interval->setRange(1, 50);
    interval->setValue(10);
    form->addRow(ticks);
    form->addRow(zzRangeText("刻度间隔"), interval);
    QObject::connect(ticks, &QCheckBox::toggled, slider, &ZzRangeSlider::setTickPosition);
    QObject::connect(ticks, &QCheckBox::toggled, interval, &QSpinBox::setEnabled);
    QObject::connect(interval, &QSpinBox::valueChanged, slider, &ZzRangeSlider::setTickInterval);
    auto *tracking = new QCheckBox(zzRangeText("Tracking（拖动时实时更新）"), parent);
    tracking->setObjectName(QStringLiteral("zzRangeTracking"));
    tracking->setChecked(true);
    form->addRow(tracking);
    QObject::connect(tracking, &QCheckBox::toggled, slider, &ZzRangeSlider::setTracking);
    auto *tip = new QCheckBox(zzRangeText("拖动数值 tooltip"), parent);
    tip->setObjectName(QStringLiteral("zzRangeValueTip"));
    tip->setChecked(true);
    form->addRow(tip);
    QObject::connect(tip, &QCheckBox::toggled, slider, &ZzRangeSlider::setValueTipEnabled);

    auto *snap = new QComboBox(parent);
    snap->setObjectName(QStringLiteral("zzRangeSnapMode"));
    snap->addItem(zzRangeText("不吸附"), int(ZzRangeSlider::ZzSnapMode::NoSnap));
    snap->addItem(zzRangeText("始终吸附"), int(ZzRangeSlider::ZzSnapMode::SnapAlways));
    snap->addItem(zzRangeText("松开时吸附"), int(ZzRangeSlider::ZzSnapMode::SnapOnRelease));
    form->addRow(zzRangeText("吸附模式"), snap);
    QObject::connect(snap, &QComboBox::currentIndexChanged, slider, [slider, snap] {
        slider->setSnapMode(static_cast<ZzRangeSlider::ZzSnapMode>(snap->currentData().toInt()));
    });
    auto *step = new QSpinBox(parent);
    step->setObjectName(QStringLiteral("zzRangeSingleStep"));
    step->setRange(1, 50);
    step->setValue(1);
    form->addRow(zzRangeText("步长"), step);
    QObject::connect(step, &QSpinBox::valueChanged, slider, &ZzRangeSlider::setSingleStep);
    auto *vertical = new QCheckBox(zzRangeText("垂直方向"), parent);
    vertical->setObjectName(QStringLiteral("zzRangeVertical"));
    form->addRow(vertical);
    QObject::connect(vertical, &QCheckBox::toggled, slider, [slider, demo](bool checked) {
        slider->setOrientation(checked ? Qt::Vertical : Qt::Horizontal);
        slider->setMinimumSize(checked ? QSize(32, 180) : QSize(44, 32));
        slider->setMaximumSize(checked ? QSize(32, 180) : QSize(QWIDGETSIZE_MAX, 32));
        demo->setAlignment(slider, checked ? Qt::AlignHCenter : Qt::Alignment{});
    });
    auto *rtl = new QCheckBox(QStringLiteral("RTL"), parent);
    rtl->setObjectName(QStringLiteral("zzRangeRtl"));
    form->addRow(rtl);
    QObject::connect(rtl, &QCheckBox::toggled, slider, [slider](bool checked) {
        slider->setLayoutDirection(checked ? Qt::RightToLeft : Qt::LeftToRight);
    });
    auto *disabled = new QCheckBox(zzRangeText("禁用"), parent);
    disabled->setObjectName(QStringLiteral("zzRangeDisabled"));
    form->addRow(disabled);
    QObject::connect(disabled, &QCheckBox::toggled, slider, &QWidget::setDisabled);
}


void ZzExampleShowcasePagePrivate::buildRangeSlider(QVBoxLayout *layout, QWidget *parent)
{
    ZzExampleRangeSliderPage::build(layout, parent);
}

} // namespace ZzExample
