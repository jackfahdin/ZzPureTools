#include <ZzFluentUI/ZzFluentStyle.h>

#include "private/ZzFluentStylePrivate.h"

#include <QtCore/QThread>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOption>

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
    if (metric == PM_ButtonMargin) {
        return qRound(d_ptr->snapshot->metric(
            ZzMetricToken::HorizontalPadding));
    }
    if (metric == PM_FocusFrameHMargin
        || metric == PM_FocusFrameVMargin) {
        return qRound(d_ptr->snapshot->metric(
            ZzMetricToken::FocusStrokeWidth));
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

int ZzFluentStyle::styleHint(
    StyleHint hint,
    const QStyleOption *option,
    const QWidget *widget,
    QStyleHintReturn *returnData) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (hint == SH_Widget_Animate
        && d_ptr->snapshot->reducedMotion()) {
        return 0;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

QPalette ZzFluentStyle::standardPalette() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    QPalette palette = QProxyStyle::standardPalette();
    palette.setColor(
        QPalette::Window,
        d_ptr->snapshot->color(ZzColorToken::Surface));
    palette.setColor(
        QPalette::Base,
        d_ptr->snapshot->color(ZzColorToken::SurfaceSecondary));
    palette.setColor(
        QPalette::Text,
        d_ptr->snapshot->color(ZzColorToken::TextPrimary));
    palette.setColor(
        QPalette::ButtonText,
        d_ptr->snapshot->color(ZzColorToken::TextPrimary));
    palette.setColor(
        QPalette::Highlight,
        d_ptr->snapshot->color(ZzColorToken::Accent));
    palette.setColor(
        QPalette::HighlightedText,
        d_ptr->snapshot->color(ZzColorToken::AccentText));
    return palette;
}

void ZzFluentStyle::drawPrimitive(
    PrimitiveElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (element == PE_FrameFocusRect
        && option != nullptr
        && painter != nullptr) {
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

} // namespace ZzFluentUI
