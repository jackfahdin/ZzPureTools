#pragma once
#include "ZzExampleCustomWidgetHelpers.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolTip>
#include <QtMath>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzMultiProgressRing.h>
#include <ZzFluentUI/ZzMultiRadialGauge.h>
#include <ZzFluentUI/ZzRadialGauge.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <type_traits>
#include <utility>

namespace ZzExample {
using ZzFluentUI::ZzFlowLayout;
using ZzFluentUI::ZzMultiProgressRing;
using ZzFluentUI::ZzMultiProgressRingItem;
using ZzFluentUI::ZzMultiRadialGauge;
using ZzFluentUI::ZzMultiRadialGaugeItem;
using ZzFluentUI::ZzRadialGauge;
using ZzFluentUI::ZzRadialGaugeRange;
inline QString zzGaugeText(const char *text)
{
    return QCoreApplication::translate("ZzPureToolsExample", text);
}
inline QWidget *makeCard(QWidget *parent) { return new ZzExampleCustomCard(parent); }
QLabel *makeSectionTitle(const QString &text, QWidget *parent);
QColor gaugeAccentColor(const QWidget *widget);
/** @brief 色块显示显式属性色或主题回退色，主题切换不写回控件属性。 */
void bindGaugeColorButton(ZzExampleColorButton *button, QWidget *widget, const char *property,
    QPalette::ColorRole fallback, int lighter = 100);
void setGaugeAccentColor(QWidget *widget, const QColor &color);
void setGaugeTrackColor(QWidget *widget, const QColor &color);
void hideGaugeTrack(ZzRadialGauge *gauge);
QSlider *makeValueSlider(QWidget *parent, int minimum, int maximum, int value, int singleStep = 1,
    int pageStep = 10, int scale = 1, int precision = 0);
ZzFluentUI::ZzTabWidget *makePropertyTabs(QWidget *parent);

template <class Widget, class Value>
void connectScaledSlider(QSlider *slider, Widget *widget, qreal scale, void (Widget::*setter)(Value))
{
    QObject::connect(slider, &QSlider::valueChanged, widget, [=](int value) {
        if constexpr (std::is_integral_v<Value>)
            (widget->*setter)(qRound(value / scale));
        else
            (widget->*setter)(value / scale);
    });
}
void buildRadialGaugeEditor(
    QWidget *propertiesCard, QWidget *gaugePropertyPage, QVBoxLayout *gaugePropertyLayout);
void buildRadialRingEditor(QWidget *ringPropertyPage, QVBoxLayout *ringPropertyLayout);
void buildMultiRadialGaugeEditor(QWidget *multiGaugePropertyPage, QVBoxLayout *multiGaugePropertyLayout);
void configureClassicGauge(ZzRadialGauge *gauge);
void configureProgressGauge(ZzRadialGauge *gauge);
void configureSpeedometerGauge(ZzRadialGauge *gauge);
void configureEChartsBaseGauge(ZzRadialGauge *gauge);
void configureEChartsBasicGauge(ZzRadialGauge *gauge);
void configureEChartsSimpleGauge(ZzRadialGauge *gauge);
void configureEChartsSpeedGauge(ZzRadialGauge *gauge);
void configureEChartsProgressGauge(ZzRadialGauge *gauge);
void configureEChartsStageGauge(ZzRadialGauge *gauge);
void configureEChartsGradeGauge(ZzRadialGauge *gauge);
void configureEChartsTemperatureGauge(ZzRadialGauge *gauge);
void configureEChartsMultiTitleGauge(ZzMultiRadialGauge *gauge);
void configureEChartsBarometerGauge(ZzRadialGauge *gauge);
} // namespace ZzExample
