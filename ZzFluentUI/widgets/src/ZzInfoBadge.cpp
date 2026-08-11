#include <ZzFluentUI/ZzInfoBadge.h>

#include <algorithm>
#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "private/ZzInfoBadgePrivate.h"

namespace ZzFluentUI {

ZzInfoBadge::ZzInfoBadge(QWidget *parent)
    : QLabel(parent)
    , d_ptr(std::make_unique<ZzInfoBadgePrivate>(this))
{
    d_ptr->refreshPresentation();
}

ZzInfoBadge::~ZzInfoBadge() = default;

ZzInfoBadgeKind ZzInfoBadge::kind() const noexcept
{
    return d_ptr->kind;
}

void ZzInfoBadge::setKind(ZzInfoBadgeKind kind)
{
    if (d_ptr->kind == kind) {
        return;
    }
    d_ptr->kind = kind;
    d_ptr->refreshPresentation();
    Q_EMIT kindChanged(kind);
}

int ZzInfoBadge::value() const noexcept
{
    return d_ptr->value;
}

void ZzInfoBadge::setValue(int value)
{
    const int bounded = std::max(0, value);
    if (d_ptr->value == bounded) {
        return;
    }
    d_ptr->value = bounded;
    d_ptr->refreshPresentation();
    Q_EMIT valueChanged(bounded);
}

int ZzInfoBadge::maximumValue() const noexcept
{
    return d_ptr->maximumValue;
}

void ZzInfoBadge::setMaximumValue(int maximumValue)
{
    const int bounded = std::max(1, maximumValue);
    if (d_ptr->maximumValue == bounded) {
        return;
    }
    d_ptr->maximumValue = bounded;
    d_ptr->refreshPresentation();
    Q_EMIT maximumValueChanged(bounded);
}

ZzMessageSeverity ZzInfoBadge::severity() const noexcept
{
    return d_ptr->severity;
}

void ZzInfoBadge::setSeverity(ZzMessageSeverity severity)
{
    if (d_ptr->severity == severity) {
        return;
    }
    d_ptr->severity = severity;
    d_ptr->refreshPresentation();
    Q_EMIT severityChanged(severity);
}

QIcon ZzInfoBadge::icon() const
{
    return d_ptr->icon;
}

void ZzInfoBadge::setIcon(QIcon icon)
{
    if (d_ptr->icon.cacheKey() == icon.cacheKey()) {
        return;
    }
    d_ptr->icon = std::move(icon);
    d_ptr->refreshPresentation();
    Q_EMIT iconChanged(d_ptr->icon);
}

QSize ZzInfoBadge::sizeHint() const
{
    const auto snapshot = d_ptr->theme.snapshot();
    const int minimumDiameter = qCeil(
        snapshot->metric(ZzMetricToken::BadgeMinDiameter));
    if (d_ptr->kind == ZzInfoBadgeKind::Dot) {
        const int dotDiameter = std::max(1, minimumDiameter / 2);
        return QSize(dotDiameter, dotDiameter);
    }
    if (d_ptr->kind == ZzInfoBadgeKind::Icon) {
        return QSize(minimumDiameter, minimumDiameter);
    }
    const QFontMetrics metrics(snapshot->font(ZzTypographyToken::Caption));
    const int textWidth = metrics.horizontalAdvance(d_ptr->displayText());
    const int padding = qCeil(
        snapshot->metric(ZzMetricToken::HorizontalPadding));
    return QSize(
        std::max(minimumDiameter, textWidth + padding),
        minimumDiameter);
}

QSize ZzInfoBadge::minimumSizeHint() const
{
    return sizeHint();
}

void ZzInfoBadge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    const auto snapshot = d_ptr->theme.snapshot();
    const QSize desired = sizeHint().boundedTo(size());
    const QRect badgeRect(
        (width() - desired.width()) / 2,
        (height() - desired.height()) / 2,
        desired.width(),
        desired.height());
    QPainter painter(this);
    ZzFluentPainter::drawBadgeSurface(
        &painter,
        QRectF(badgeRect),
        *snapshot,
        d_ptr->fillToken());

    if (d_ptr->kind == ZzInfoBadgeKind::Number) {
        painter.setFont(snapshot->font(ZzTypographyToken::Caption));
        painter.setPen(snapshot->color(ZzColorToken::Surface));
        painter.drawText(badgeRect, Qt::AlignCenter, d_ptr->displayText());
    } else if (d_ptr->kind == ZzInfoBadgeKind::Icon
               && !d_ptr->icon.isNull()) {
        const int iconExtent = qCeil(
            snapshot->metric(ZzMetricToken::IconSmall));
        const QRect iconRect(
            badgeRect.center().x() - iconExtent / 2,
            badgeRect.center().y() - iconExtent / 2,
            iconExtent,
            iconExtent);
        d_ptr->icon.paint(
            &painter,
            iconRect,
            Qt::AlignCenter,
            isEnabled() ? QIcon::Normal : QIcon::Disabled,
            QIcon::On);
    }
}

void ZzInfoBadge::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange) {
        d_ptr->theme.refreshFallback();
    }
    if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange
        || event->type() == QEvent::LanguageChange
        || event->type() == QEvent::FontChange
        || event->type() == QEvent::DevicePixelRatioChange) {
        d_ptr->refreshPresentation();
    }
}

} // namespace ZzFluentUI
