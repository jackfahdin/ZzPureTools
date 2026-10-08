#include "ZzExampleRadialGaugeHelpers.h"

namespace ZzExample {
namespace {
/** @brief 将滑块整数刻度显示为仪表的实际属性值。 */
class ZzGaugePropertySlider final : public QSlider
{
public:
    explicit ZzGaugePropertySlider(QWidget *parent)
        : QSlider(Qt::Horizontal, parent)
    {
    }

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QSlider::event(event);
        const auto type = event->type();
        if (isEnabled()
            && (type == QEvent::KeyPress || type == QEvent::Wheel
                || ((type == QEvent::MouseMove || type == QEvent::MouseButtonPress) && isSliderDown()))) {
            const int scale = qMax(1, property("scale").toInt());
            QToolTip::showText(mapToGlobal(QPoint(width() / 2, 0)),
                QString::number(qreal(value()) / scale, 'f', property("precision").toInt()), this);
        } else if (type == QEvent::MouseButtonRelease || type == QEvent::Hide || type == QEvent::FocusOut) {
            QToolTip::hideText();
        }
        return handled;
    }
};
}
QLabel *makeSectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    font.setPixelSize(14);
    label->setFont(font);
    return label;
}

QPalette::ColorRole accentRole() { return QPalette::Accent; }

void bindGaugeColorButton(ZzExampleColorButton *button, QWidget *widget, const char *property,
    QPalette::ColorRole fallback, int lighter)
{
    button->themeColor = [widget, property, fallback, lighter] {
        const QColor color = property ? widget->property(property).value<QColor>() : QColor();
        return color.isValid() ? color : widget->palette().color(QPalette::Active, fallback).lighter(lighter);
    };
    button->setSelectedColor(button->themeColor());
}

QColor gaugeAccentColor(const QWidget *widget)
{
    return widget->palette().color(QPalette::Active, accentRole());
}

void setGaugeAccentColor(QWidget *widget, const QColor &color)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Active, accentRole(), color);
    palette.setColor(QPalette::Inactive, accentRole(), color);
    widget->setPalette(palette);
}

void setGaugeTrackColor(QWidget *widget, const QColor &color)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Active, QPalette::Mid, color);
    palette.setColor(QPalette::Inactive, QPalette::Mid, color);
    widget->setPalette(palette);
}

void hideGaugeTrack(ZzRadialGauge *gauge) { setGaugeTrackColor(gauge, QColor(0, 0, 0, 0)); }

QSlider *makeValueSlider(QWidget *parent, int minimum, int maximum, int value, int singleStep, int pageStep,
    int scale, int precision)
{
    auto *slider = new ZzGaugePropertySlider(parent);
    slider->setRange(minimum, maximum);
    slider->setValue(value);
    slider->setSingleStep(singleStep);
    slider->setPageStep(pageStep);
    slider->setTracking(true);
    slider->setMinimumWidth(180);
    if (scale > 1) {
        slider->setProperty("scale", scale);
        slider->setProperty("precision", precision);
    }
    return slider;
}

ZzFluentUI::ZzTabWidget *makePropertyTabs(QWidget *parent)
{
    auto *tabs = new ZzFluentUI::ZzTabWidget(parent);
    tabs->fluentTabBar()->setAppearance(ZzFluentUI::ZzTabBarAppearance::PivotSlide);
    tabs->fluentTabBar()->setExpanding(false);
    tabs->setElideMode(Qt::ElideNone);
    tabs->setUsesScrollButtons(true);
    tabs->fluentTabBar()->newTabButton()->hide();
    tabs->fluentTabBar()->setTearOffEnabled(false);
    tabs->fluentTabBar()->setTabTransferEnabled(false);
    tabs->fluentTabBar()->setContextMenuPolicy(Qt::PreventContextMenu);
    tabs->setMovable(false);
    return tabs;
}
} // namespace ZzExample
