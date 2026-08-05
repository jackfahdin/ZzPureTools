#include <ZzFluentUI/ZzFluentStyle.h>

#include "private/ZzFluentStylePrivate.h"

#include <QtCore/QThread>
#include <QtGui/QPainter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTextEdit>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzFluentStyle::ZzFluentStyle(
    ZzThemeController *controller,
    QStyle *baseStyle)
    : QProxyStyle(baseStyle)
    , d_ptr(std::make_unique<ZzFluentStylePrivate>(this, controller))
{
}

ZzFluentStyle::~ZzFluentStyle() = default;

quint64 ZzFluentStyle::themeRevision() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->snapshot->revision();
}

int ZzFluentStyle::iconCacheBytes() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->cache.iconBytes();
}

QPixmap ZzFluentStyle::iconPixmap(
    const ZzIconDescriptor &descriptor,
    QSize logicalSize,
    qreal devicePixelRatio,
    QColor color,
    Qt::LayoutDirection direction)
{
    return d_ptr->iconPixmap(
        descriptor,
        logicalSize,
        devicePixelRatio,
        color,
        direction);
}

int ZzFluentStyle::pixelMetric(
    PixelMetric metric,
    const QStyleOption *option,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    switch (metric) {
    case PM_ButtonMargin:
        return qRound(d_ptr->snapshot->metric(
            ZzMetricToken::HorizontalPadding));
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
        return 18;
    case PM_SliderLength:
        return 20;
    case PM_SliderThickness:
        return 4;
    case PM_ProgressBarChunkWidth:
        return 1;
    case PM_ScrollBarExtent:
        return 12;
    case PM_ScrollBarSliderMin:
    case PM_TabBarTabHSpace:
        return 24;
    case PM_TabBarTabVSpace:
        return 12;
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:
        return 2;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int ZzFluentStyle::styleHint(
    StyleHint hint,
    const QStyleOption *option,
    const QWidget *widget,
    QStyleHintReturn *returnData) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (hint == SH_Menu_SubMenuPopupDelay) {
        return 200;
    }
    if (hint == SH_Widget_Animate) {
        if (d_ptr->snapshot != nullptr
            && d_ptr->snapshot->reducedMotion()) {
            return 0;
        }
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

QPalette ZzFluentStyle::standardPalette() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QPalette palette = QProxyStyle::standardPalette();
    const QColor textPrimary = d_ptr->snapshot->color(
        ZzColorToken::TextPrimary);
    const QColor textSecondary = d_ptr->snapshot->color(
        ZzColorToken::TextSecondary);
    const QColor surface = d_ptr->snapshot->color(
        ZzColorToken::Surface);
    const QColor surfaceSecondary = d_ptr->snapshot->color(
        ZzColorToken::SurfaceSecondary);
    const QColor controlFill = d_ptr->snapshot->color(
        ZzColorToken::ControlFill);
    const QColor controlStroke = d_ptr->snapshot->color(
        ZzColorToken::ControlStroke);
    const QColor accent = d_ptr->snapshot->color(ZzColorToken::Accent);
    const QColor accentText = d_ptr->snapshot->color(
        ZzColorToken::AccentText);

    palette.setColor(
        QPalette::Window,
        surface);
    palette.setColor(QPalette::WindowText, textPrimary);
    palette.setColor(
        QPalette::Base,
        surfaceSecondary);
    palette.setColor(QPalette::AlternateBase, controlFill);
    palette.setColor(QPalette::ToolTipBase, controlFill);
    palette.setColor(QPalette::ToolTipText, textPrimary);
    palette.setColor(QPalette::Text, textPrimary);
    palette.setColor(QPalette::PlaceholderText, textSecondary);
    palette.setColor(QPalette::Button, controlFill);
    palette.setColor(QPalette::ButtonText, textPrimary);
    palette.setColor(QPalette::BrightText, textPrimary);
    palette.setColor(QPalette::Light, controlStroke);
    palette.setColor(QPalette::Midlight, controlStroke);
    palette.setColor(QPalette::Mid, controlStroke);
    palette.setColor(QPalette::Dark, controlStroke);
    palette.setColor(QPalette::Shadow, controlStroke);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, accentText);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accent);

    for (const QPalette::ColorRole role : {
             QPalette::WindowText,
             QPalette::Text,
             QPalette::ButtonText,
             QPalette::ToolTipText,
             QPalette::PlaceholderText}) {
        palette.setColor(QPalette::Disabled, role, textSecondary);
    }
    return palette;
}

QSize ZzFluentStyle::sizeFromContents(
    ContentsType type,
    const QStyleOption *option,
    const QSize &contentsSize,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QSize result = QProxyStyle::sizeFromContents(
        type,
        option,
        contentsSize,
        widget);
    if (type == CT_LineEdit || type == CT_SpinBox) {
        result = result.expandedTo(QSize(96, 32));
    }
    return result;
}

void ZzFluentStyle::drawPrimitive(
    PrimitiveElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if ((element == PE_IndicatorCheckBox
         || element == PE_IndicatorRadioButton)
        && option != nullptr && painter != nullptr) {
        d_ptr->drawCheckIndicator(
            option,
            painter,
            element == PE_IndicatorRadioButton);
        return;
    }
    const bool textFrame = element == PE_Frame
        && (qobject_cast<const QLineEdit *>(widget) != nullptr
            || qobject_cast<const QTextEdit *>(widget) != nullptr
            || qobject_cast<const QPlainTextEdit *>(widget) != nullptr);
    if ((element == PE_PanelLineEdit
         || element == PE_FrameLineEdit
         || textFrame)
        && option != nullptr && painter != nullptr) {
        d_ptr->drawInputPanel(option, painter, widget);
        return;
    }
    if (element == PE_PanelTipLabel
        && option != nullptr && painter != nullptr) {
        d_ptr->drawToolTipPanel(option, painter);
        return;
    }
    if (element == PE_PanelScrollAreaCorner
        && option != nullptr && painter != nullptr) {
        painter->fillRect(
            option->rect,
            option->palette.color(QPalette::Window));
        return;
    }
    if (element == PE_FrameFocusRect
        && option != nullptr && painter != nullptr) {
        const qreal dpr = widget != nullptr
            ? widget->devicePixelRatioF()
            : 1.0;
        ZzFluentPainter::drawFocusRing(
            painter,
            option->rect,
            *d_ptr->snapshot,
            dpr);
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void ZzFluentStyle::drawControl(
    ControlElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (element == CE_PushButton) {
        const auto *button = qstyleoption_cast<
            const QStyleOptionButton *>(option);
        if (button != nullptr && painter != nullptr) {
            d_ptr->drawPushButton(button, painter, widget);
            return;
        }
    }
    if (element == CE_ProgressBar) {
        const auto *progress = qstyleoption_cast<
            const QStyleOptionProgressBar *>(option);
        if (progress != nullptr && painter != nullptr) {
            d_ptr->drawProgressBar(progress, painter, widget);
            return;
        }
    }
    if (element == CE_TabBarTab) {
        const auto *tab = qstyleoption_cast<
            const QStyleOptionTab *>(option);
        if (tab != nullptr && painter != nullptr) {
            d_ptr->drawTabBarTab(tab, painter, widget);
            return;
        }
    }
    if (element == CE_MenuItem) {
        const auto *menuItem = qstyleoption_cast<
            const QStyleOptionMenuItem *>(option);
        if (menuItem != nullptr && painter != nullptr) {
            d_ptr->drawMenuItem(menuItem, painter, widget);
            return;
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void ZzFluentStyle::drawComplexControl(
    ComplexControl control,
    const QStyleOptionComplex *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (control == CC_Slider) {
        const auto *slider = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (slider != nullptr && painter != nullptr) {
            d_ptr->drawSlider(slider, painter, widget);
            return;
        }
    }
    if (control == CC_ComboBox) {
        const auto *combo = qstyleoption_cast<
            const QStyleOptionComboBox *>(option);
        if (combo != nullptr && painter != nullptr) {
            d_ptr->drawComboBox(combo, painter, widget);
            return;
        }
    }
    if (control == CC_SpinBox) {
        const auto *spinBox = qstyleoption_cast<
            const QStyleOptionSpinBox *>(option);
        if (spinBox != nullptr && painter != nullptr) {
            d_ptr->drawSpinBox(spinBox, painter, widget);
            return;
        }
    }
    if (control == CC_ScrollBar) {
        const auto *scrollBar = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (scrollBar != nullptr && painter != nullptr) {
            d_ptr->drawScrollBar(scrollBar, painter, widget);
            return;
        }
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

QRect ZzFluentStyle::subControlRect(
    ComplexControl control,
    const QStyleOptionComplex *option,
    SubControl subControl,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QRect result = QProxyStyle::subControlRect(
        control,
        option,
        subControl,
        widget);
    if (control == CC_Slider && subControl == SC_SliderHandle) {
        const int length = pixelMetric(PM_SliderLength, option, widget);
        const QPoint center = result.center();
        result.setSize(QSize(length, length));
        result.moveCenter(center);
    }
    if (control == CC_SpinBox) {
        const auto *spinBox = qstyleoption_cast<
            const QStyleOptionSpinBox *>(option);
        if (spinBox != nullptr) {
            return d_ptr->spinBoxSubControlRect(spinBox, subControl);
        }
    }
    if (control == CC_ScrollBar) {
        const auto *scrollBar = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (scrollBar != nullptr) {
            return d_ptr->scrollBarSubControlRect(scrollBar, subControl);
        }
    }
    return result;
}

QStyle::SubControl ZzFluentStyle::hitTestComplexControl(
    ComplexControl control,
    const QStyleOptionComplex *option,
    const QPoint &position,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (control == CC_SpinBox) {
        const auto *spinBox = qstyleoption_cast<
            const QStyleOptionSpinBox *>(option);
        if (spinBox != nullptr) {
            return d_ptr->hitTestSpinBox(spinBox, position);
        }
    }
    if (control == CC_ScrollBar) {
        const auto *scrollBar = qstyleoption_cast<
            const QStyleOptionSlider *>(option);
        if (scrollBar != nullptr) {
            return d_ptr->hitTestScrollBar(scrollBar, position);
        }
    }
    return QProxyStyle::hitTestComplexControl(
        control,
        option,
        position,
        widget);
}

} // namespace ZzFluentUI
