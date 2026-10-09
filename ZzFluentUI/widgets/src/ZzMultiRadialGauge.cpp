#include <ZzFluentUI/ZzMultiRadialGauge.h>

#include "private/ZzMultiRadialGaugePrivate.h"
#include "private/ZzGaugeSupportPrivate.h"

#include <QEvent>
#include <QEasingCurve>
#include <QPointer>
#include <QSizePolicy>
#include <QVariantAnimation>

#include <cmath>
#include <utility>
#include <QFontMetricsF>
#include <QPainter>
#include <QPolygonF>
#include <QStyleOption>
#include <QtMath>
#include <algorithm>

namespace ZzFluentUI {
namespace {
constexpr qreal FullCircle = 360.0;
constexpr int MaximumTickCount = 720;
constexpr int MaximumMajorTickCount = 180;
/** @brief 数值徽标背景的圆角半径，单位逻辑像素。 */
constexpr qreal DetailBadgeRadius = 3.0;
QPointF pointAtAngle(const QPointF &center, qreal radius, qreal angle)
{
    const qreal radians = qDegreesToRadians(angle);
    return QPointF(center.x() + radius * std::sin(radians), center.y() - radius * std::cos(radians));
}
QPalette::ColorRole accentRole()
{
    return QPalette::Accent;
}
QColor disabledItemColor(const QColor &color, const QPalette &palette)
{
    if (!color.isValid()) {
        return palette.color(QPalette::Disabled, accentRole());
    }

    const QColor disabledText = palette.color(QPalette::Disabled, QPalette::Text);
    return QColor::fromRgb((color.red() + disabledText.red()) / 2, (color.green() + disabledText.green()) / 2,
                  (color.blue() + disabledText.blue()) / 2, color.alpha());
}
} // namespace

ZzMultiRadialGaugeItem::ZzMultiRadialGaugeItem(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzMultiRadialGaugeItemPrivate>())
{
}

ZzMultiRadialGaugeItem::ZzMultiRadialGaugeItem(const QString &label, qreal value, const QColor &color,
                                               QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzMultiRadialGaugeItemPrivate>())
{
    d_ptr->label = label;
    d_ptr->value = qIsFinite(value) ? value : 0.0;
    d_ptr->color = color;
}

ZzMultiRadialGauge::ZzMultiRadialGauge(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<ZzMultiRadialGaugePrivate>())
{

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setAutoFillBackground(false);

    d_ptr->valueAnimation = new QVariantAnimation(this);
    d_ptr->valueAnimation->setStartValue(0.0);
    d_ptr->valueAnimation->setEndValue(1.0);
    d_ptr->valueAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(d_ptr->valueAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const qreal progress = value.toReal();
        for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
            const qreal startValue = d_ptr->animationStartValues.value(item, item->value());
            d_ptr->displayedValues.insert(item, std::lerp(startValue, item->value(), progress));
        }
        update();
    });
    connect(d_ptr->valueAnimation, &QVariantAnimation::finished, this,
            [this] { synchronizeDisplayedValues(); });
}

ZzMultiRadialGauge::~ZzMultiRadialGauge()
{
    d_ptr->valueAnimation->stop();
    for (ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
        disconnect(item, nullptr, this, nullptr);
    }
    d_ptr->items.clear();
    d_ptr->displayedValues.clear();
    d_ptr->animationStartValues.clear();
}

void ZzMultiRadialGauge::setRange(qreal minimum, qreal maximum)
{
    if (!qIsFinite(minimum) || !qIsFinite(maximum)) {
        return;
    }
    maximum = qMax(minimum, maximum);
    const bool minimumChanged = !qFuzzyCompare(d_ptr->minimum + 1.0, minimum + 1.0);
    const bool maximumChanged = !qFuzzyCompare(d_ptr->maximum + 1.0, maximum + 1.0);
    if (!minimumChanged && !maximumChanged) {
        return;
    }
    d_ptr->minimum = minimum;
    d_ptr->maximum = maximum;
    update();
    QPointer<ZzMultiRadialGauge> guard(this);
    if (minimumChanged) {
        emit this->minimumChanged(minimum);
    }
    if (maximumChanged) {
        if (!guard || d_ptr->minimum != minimum || d_ptr->maximum != maximum)
            return;
        emit this->maximumChanged(maximum);
    }
}

QList<ZzMultiRadialGaugeItem *> ZzMultiRadialGauge::items() const
{
    return d_ptr->items;
}

ZzMultiRadialGaugeItem *ZzMultiRadialGauge::addItem(const QString &label, qreal value, const QColor &color)
{
    auto *item = new ZzMultiRadialGaugeItem(label, value, color, this);
    QPointer<ZzMultiRadialGaugeItem> guard(item);
    addItem(item);
    return guard.data();
}

void ZzMultiRadialGauge::addItem(ZzMultiRadialGaugeItem *item)
{
    if (!item || item->d_ptr->adoptionInProgress || d_ptr->items.contains(item) ||
        item->property("_zzGaugePendingDelete").toBool()) {
        return;
    }
    QPointer<ZzMultiRadialGaugeItem> guard(item);
    QPointer<ZzMultiRadialGauge> ownerGuard(this);
    item->d_ptr->adoptionInProgress = true;
    item->setParent(this);
    if (!guard)
        return;
    item->d_ptr->adoptionInProgress = false;
    if (!ownerGuard || item->parent() != this || d_ptr->items.contains(item) ||
        item->property("_zzGaugePendingDelete").toBool())
        return;
    d_ptr->items.append(item);
    d_ptr->displayedValues.insert(item, item->value());
    connectItem(item);
    update();
    emit itemsChanged();
}

void ZzMultiRadialGauge::removeItem(ZzMultiRadialGaugeItem *item)
{
    if (!item || !d_ptr->items.removeOne(item)) {
        return;
    }
    disconnect(item, nullptr, this, nullptr);
    d_ptr->displayedValues.remove(item);
    d_ptr->animationStartValues.remove(item);
    if (d_ptr->items.isEmpty())
        d_ptr->valueAnimation->stop();
    item->setProperty("_zzGaugePendingDelete", true);
    item->deleteLater();
    update();
    emit itemsChanged();
}

void ZzMultiRadialGauge::clearItems()
{
    if (d_ptr->items.isEmpty()) {
        return;
    }
    d_ptr->valueAnimation->stop();
    const auto oldItems = d_ptr->items;
    d_ptr->items.clear();
    d_ptr->displayedValues.clear();
    d_ptr->animationStartValues.clear();
    for (ZzMultiRadialGaugeItem *item : oldItems) {
        disconnect(item, nullptr, this, nullptr);
        item->setProperty("_zzGaugePendingDelete", true);
        item->deleteLater();
    }
    update();
    emit itemsChanged();
}

QSize ZzMultiRadialGauge::sizeHint() const
{
    return QSize(320, 260);
}

QSize ZzMultiRadialGauge::minimumSizeHint() const
{
    return QSize(120, 100);
}

qreal ZzMultiRadialGauge::sweepAngle() const
{
    qreal sweep = std::fmod(d_ptr->maximumAngle - d_ptr->minimumAngle, FullCircle);
    if (sweep <= 0.0) {
        sweep += FullCircle;
    }
    return sweep;
}

qreal ZzMultiRadialGauge::valueFraction(qreal value) const
{
    return zzGaugeFraction(value, d_ptr->minimum, d_ptr->maximum);
}

qreal ZzMultiRadialGauge::displayedValue(const ZzMultiRadialGaugeItem *item) const
{
    return item ? d_ptr->displayedValues.value(item, item->value()) : 0.0;
}

void ZzMultiRadialGauge::connectItem(ZzMultiRadialGaugeItem *item)
{
    connect(item, &ZzMultiRadialGaugeItem::valueChanged, this, [this](qreal) { startValueAnimation(); });
    connect(item, &ZzMultiRadialGaugeItem::itemChanged, this, [this] {
        update();
        emit itemsChanged();
    });
    connect(item, &QObject::destroyed, this, [this, item] {
        if (d_ptr->items.removeOne(item)) {
            d_ptr->displayedValues.remove(item);
            d_ptr->animationStartValues.remove(item);
            if (d_ptr->items.isEmpty())
                d_ptr->valueAnimation->stop();
            update();
            emit itemsChanged();
        }
    });
}

void ZzMultiRadialGauge::startValueAnimation()
{
    if (!d_ptr->valueAnimation || d_ptr->valueAnimationDuration == 0 || !zzGaugeMotionAllowed(this)) {
        synchronizeDisplayedValues();
        return;
    }
    d_ptr->valueAnimation->stop();
    d_ptr->animationStartValues = d_ptr->displayedValues;
    d_ptr->valueAnimation->setDuration(d_ptr->valueAnimationDuration);
    d_ptr->valueAnimation->start();
}

void ZzMultiRadialGauge::synchronizeDisplayedValues()
{
    d_ptr->valueAnimation->stop();
    for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
        d_ptr->displayedValues.insert(item, item->value());
    }
    d_ptr->animationStartValues.clear();
    update();
}

ZzMultiRadialGaugeItem::~ZzMultiRadialGaugeItem() = default;

bool ZzMultiRadialGauge::event(QEvent *event)
{
    QPointer<ZzMultiRadialGauge> guard(this);
    const bool result = QWidget::event(event);
    if (!guard || !d_ptr || !d_ptr->valueAnimation)
        return result;
    if ((event->type() == QEvent::Hide || event->type() == QEvent::EnabledChange ||
         event->type() == QEvent::StyleChange) &&
        !zzGaugeMotionAllowed(this)) {
        synchronizeDisplayedValues();
    }
    if (event->type() == QEvent::ChildRemoved) {
        const auto *removed = static_cast<QChildEvent *>(event);
        for (auto *item : std::as_const(d_ptr->items)) {
            if (item == removed->child()) {
                d_ptr->items.removeOne(item);
                d_ptr->displayedValues.remove(item);
                d_ptr->animationStartValues.remove(item);
                disconnect(item, nullptr, this, nullptr);
                if (d_ptr->items.isEmpty())
                    d_ptr->valueAnimation->stop();
                update();
                // ChildRemoved 发生时 QObject 尚未完成 setParent；外部槽必须延后执行。
                QMetaObject::invokeMethod(this, [this] { emit itemsChanged(); }, Qt::QueuedConnection);
                break;
            }
        }
    }
    return result;
}

 // namespace

void ZzMultiRadialGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStyleOption option;
    option.initFrom(this);
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QPalette::ColorGroup colorGroup =
        enabled ? (option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive)
                : QPalette::Disabled;
    const QColor accentColor = option.palette.color(colorGroup, accentRole());
    const QColor resolvedTrackColor = enabled && d_ptr->trackColor.isValid()
                                          ? d_ptr->trackColor
                                          : option.palette.color(colorGroup, QPalette::Mid);
    QColor resolvedTickColor = enabled && d_ptr->tickColor.isValid()
                                   ? d_ptr->tickColor
                                   : option.palette.color(colorGroup, QPalette::Text);
    QColor resolvedLabelColor = enabled && d_ptr->labelColor.isValid()
                                    ? d_ptr->labelColor
                                    : option.palette.color(colorGroup, QPalette::Text);
    const QColor resolvedTitleColor = enabled && d_ptr->titleColor.isValid()
                                          ? d_ptr->titleColor
                                          : option.palette.color(colorGroup, QPalette::Text);
    if (!d_ptr->tickColor.isValid() && !zzGaugeHighContrast(this)) {
        resolvedTickColor.setAlpha(enabled ? 150 : 80);
    }
    if (!d_ptr->labelColor.isValid() && !zzGaugeHighContrast(this)) {
        resolvedLabelColor.setAlpha(enabled ? 180 : 90);
    }

    const qreal side = qMin(width(), height());
    const QPointF center = QRectF(rect()).center();
    const qreal maximumLineWidth = qMax(d_ptr->trackWidth, d_ptr->progressWidth);
    const qreal radius = qMax(1.0, side * 0.5 - d_ptr->scalePadding - maximumLineWidth * 0.5 - 1.0);
    const QRectF scaleRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    const qreal sweep = sweepAngle();
    const qreal painterStartAngle = 90.0 - d_ptr->minimumAngle;

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    if (d_ptr->trackVisible) {
        painter.setPen(QPen(resolvedTrackColor, d_ptr->trackWidth, Qt::SolidLine, d_ptr->trackCapStyle));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(scaleRect, qRound(painterStartAngle * 16.0), qRound(-sweep * 16.0));
    }

    const auto itemColor = [&](const ZzMultiRadialGaugeItem *item) {
        if (!item) {
            return accentColor;
        }
        if (!enabled) {
            return disabledItemColor(item->color(), option.palette);
        }
        return item->color().isValid() ? item->color() : accentColor;
    };

    if (d_ptr->progressVisible) {
        const auto drawProgress = [&](const ZzMultiRadialGaugeItem *item, qreal itemRadius) {
            const qreal fraction = valueFraction(displayedValue(item));
            if (fraction <= 0.0 || itemRadius <= d_ptr->progressWidth * 0.5) {
                return;
            }
            const QRectF itemRect(center.x() - itemRadius, center.y() - itemRadius, itemRadius * 2.0,
                                  itemRadius * 2.0);
            painter.setPen(
                QPen(itemColor(item), d_ptr->progressWidth, Qt::SolidLine, d_ptr->progressCapStyle));
            painter.drawArc(itemRect, qRound(painterStartAngle * 16.0), qRound(-sweep * fraction * 16.0));
        };

        if (d_ptr->progressOverlap) {
            QList<const ZzMultiRadialGaugeItem *> sortedItems;
            sortedItems.reserve(d_ptr->items.size());
            for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
                if (item && item->isVisible()) {
                    sortedItems.append(item);
                }
            }
            std::stable_sort(sortedItems.begin(), sortedItems.end(),
                             [this](const ZzMultiRadialGaugeItem *left, const ZzMultiRadialGaugeItem *right) {
                                 return valueFraction(displayedValue(left)) >
                                        valueFraction(displayedValue(right));
                             });
            for (const ZzMultiRadialGaugeItem *item : std::as_const(sortedItems)) {
                drawProgress(item, radius);
            }
        } else {
            for (qsizetype index = 0; index < d_ptr->items.size(); ++index) {
                const ZzMultiRadialGaugeItem *item = d_ptr->items.at(index);
                const qreal itemRadius =
                    radius - static_cast<qreal>(index) * (d_ptr->progressWidth + d_ptr->progressSpacing);
                if (!item || !item->isVisible() || itemRadius <= d_ptr->progressWidth * 0.5) {
                    continue;
                }
                drawProgress(item, itemRadius);
            }
        }
    }

    const int majorTickCount = qBound(2, d_ptr->majorTickCount, MaximumMajorTickCount);
    const int majorIntervalCount = majorTickCount - 1;
    const int maximumMinorTickCount = qMax(0, MaximumTickCount / majorIntervalCount - 1);
    const int minorTickCount = qMin(d_ptr->minorTickCount, maximumMinorTickCount);
    const qreal tickOuterRadius = qMax(0.0, radius - maximumLineWidth * 0.5 - d_ptr->tickPadding);

    const auto drawTick = [&](qreal fraction, qreal length, qreal width) {
        const qreal angle = d_ptr->minimumAngle + sweep * fraction;
        const qreal innerRadius = qMax(0.0, tickOuterRadius - length);
        painter.setPen(QPen(resolvedTickColor, width, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(pointAtAngle(center, innerRadius, angle),
                         pointAtAngle(center, tickOuterRadius, angle));
    };

    if (d_ptr->tickLength > 0.0 && minorTickCount > 0) {
        for (int interval = 0; interval < majorIntervalCount; ++interval) {
            for (int minorIndex = 1; minorIndex <= minorTickCount; ++minorIndex) {
                const qreal position = static_cast<qreal>(minorIndex) / (minorTickCount + 1);
                drawTick((interval + position) / majorIntervalCount, d_ptr->tickLength, d_ptr->tickWidth);
            }
        }
    }
    if (d_ptr->majorTickLength > 0.0) {
        for (int index = 0; index < majorTickCount; ++index) {
            drawTick(static_cast<qreal>(index) / majorIntervalCount, d_ptr->majorTickLength,
                     d_ptr->majorTickWidth);
        }
    }

    if (d_ptr->labelsVisible) {
        QFont labelFont = font();
        labelFont.setPixelSize(qMin(d_ptr->labelFontPixelSize, qMax(1, qRound(side * 0.12))));
        painter.setFont(labelFont);
        painter.setPen(resolvedLabelColor);
        const QFontMetricsF metrics(labelFont);
        const qreal labelRadius = qMax(0.0, tickOuterRadius - d_ptr->majorTickLength - d_ptr->labelPadding);
        for (int index = 0; index < majorTickCount; ++index) {
            const qreal fraction = static_cast<qreal>(index) / majorIntervalCount;
            const qreal value = std::lerp(d_ptr->minimum, d_ptr->maximum, fraction);
            const QPointF labelCenter =
                pointAtAngle(center, labelRadius, d_ptr->minimumAngle + sweep * fraction);
            const QString text = QString::number(value, 'g', 4);
            const QRectF bounds = metrics.boundingRect(text);
            const QRectF textRect(labelCenter.x() - bounds.width() * 0.5 - 2.0,
                                  labelCenter.y() - metrics.height() * 0.5, bounds.width() + 4.0,
                                  metrics.height());
            painter.drawText(textRect, Qt::AlignCenter, text);
        }
    }

    const QPointF needleCenter =
        center + QPointF(d_ptr->needleOffset.x() * radius, d_ptr->needleOffset.y() * radius);
    if (d_ptr->needleStyle != NoNeedle) {
        for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
            if (!item || !item->isVisible()) {
                continue;
            }
            const qreal angle = d_ptr->minimumAngle + sweep * valueFraction(displayedValue(item));
            const qreal radians = qDegreesToRadians(angle);
            const QPointF direction(std::sin(radians), -std::cos(radians));
            const QPointF normal(-direction.y(), direction.x());
            const QPointF tip = needleCenter + direction * (radius * d_ptr->needleLength);
            const QColor color = itemColor(item);
            if (d_ptr->needleStyle == LineNeedle) {
                const QPointF tail = needleCenter - direction * qMax(2.0, d_ptr->needleWidth * 1.8);
                painter.setPen(QPen(color, d_ptr->needleWidth, Qt::SolidLine, Qt::RoundCap));
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(tail, tip);
            } else {
                const QPointF tail = needleCenter - direction * qMax(2.0, d_ptr->needleWidth);
                const qreal halfWidth = d_ptr->needleWidth * 0.5;
                QPolygonF needle;
                needle << tip << needleCenter + normal * halfWidth << tail
                       << needleCenter - normal * halfWidth;
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawPolygon(needle);
            }
        }
    }

    if (d_ptr->hubVisible && d_ptr->hubRadius > 0.0) {
        const QColor color = enabled && d_ptr->hubColor.isValid() ? d_ptr->hubColor : accentColor;
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(needleCenter, d_ptr->hubRadius, d_ptr->hubRadius);
    }

    QFont titleFont = font();
    titleFont.setPixelSize(qMin(d_ptr->titleFontPixelSize, qMax(1, qRound(side * 0.12))));
    QFont detailFont = font();
    detailFont.setPixelSize(qMin(d_ptr->detailFontPixelSize, qMax(1, qRound(side * 0.12))));
    detailFont.setWeight(QFont::DemiBold);
    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF detailMetrics(detailFont);

    for (const ZzMultiRadialGaugeItem *item : std::as_const(d_ptr->items)) {
        if (!item || !item->isVisible()) {
            continue;
        }
        const QColor color = itemColor(item);
        if (d_ptr->titleVisible && !item->label().isEmpty()) {
            const QPointF titleCenter =
                center + QPointF(item->titleOffset().x() * radius, item->titleOffset().y() * radius);
            const qreal width = qMax(40.0, titleMetrics.horizontalAdvance(item->label()) + 8.0);
            const QRectF titleRect(titleCenter.x() - width * 0.5,
                                   titleCenter.y() - titleMetrics.height() * 0.5, width,
                                   titleMetrics.height());
            painter.setFont(titleFont);
            painter.setPen(resolvedTitleColor);
            painter.drawText(titleRect, Qt::AlignCenter, item->label());
        }

        if (d_ptr->detailVisible) {
            const QString valueText =
                QString::number(displayedValue(item), 'f', d_ptr->valueDecimals) + d_ptr->valueSuffix;
            const QPointF detailCenter =
                center + QPointF(item->detailOffset().x() * radius, item->detailOffset().y() * radius);
            const qreal textWidth = detailMetrics.horizontalAdvance(valueText);
            const qreal detailWidth = textWidth + d_ptr->detailBadgePadding * 2.0;
            const qreal detailHeight = detailMetrics.height() + 2.0;
            const QRectF detailRect(detailCenter.x() - detailWidth * 0.5,
                                    detailCenter.y() - detailHeight * 0.5, detailWidth, detailHeight);
            painter.setFont(detailFont);
            if (d_ptr->detailBadgeVisible) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawRoundedRect(detailRect, DetailBadgeRadius, DetailBadgeRadius);
                painter.setPen(d_ptr->detailTextColor.isValid() ? d_ptr->detailTextColor
                                                                : zzGaugeContrastingText(color));
            } else {
                painter.setPen(d_ptr->detailTextColor.isValid() ? d_ptr->detailTextColor : color);
            }
            painter.drawText(detailRect, Qt::AlignCenter, valueText);
        }
    }
}

 // namespace

QString ZzMultiRadialGaugeItem::label() const
{
    return d_ptr->label;
}

void ZzMultiRadialGaugeItem::setLabel(QString value)
{
    if (d_ptr->label == value)
        return;
    d_ptr->label = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit labelChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiRadialGaugeItem::value() const
{
    return d_ptr->value;
}

void ZzMultiRadialGaugeItem::setValue(qreal value)
{
    if (!qIsFinite(value) || qFuzzyCompare(d_ptr->value + 1.0, value + 1.0)) {
        return;
    }

    d_ptr->value = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit valueChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

QColor ZzMultiRadialGaugeItem::color() const
{
    return d_ptr->color;
}

void ZzMultiRadialGaugeItem::setColor(QColor value)
{
    if (d_ptr->color == value)
        return;
    d_ptr->color = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit colorChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

bool ZzMultiRadialGaugeItem::isVisible() const
{
    return d_ptr->visible;
}

void ZzMultiRadialGaugeItem::setVisible(bool value)
{
    if (d_ptr->visible == value)
        return;
    d_ptr->visible = value;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit visibleChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

QPointF ZzMultiRadialGaugeItem::titleOffset() const
{
    return d_ptr->titleOffset;
}

void ZzMultiRadialGaugeItem::setTitleOffset(QPointF offset)
{
    if (!qIsFinite(offset.x()) || !qIsFinite(offset.y()) || d_ptr->titleOffset == offset) {
        return;
    }

    d_ptr->titleOffset = offset;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit titleOffsetChanged(offset);
    if (!guard)
        return;
    emit itemChanged();
}

QPointF ZzMultiRadialGaugeItem::detailOffset() const
{
    return d_ptr->detailOffset;
}

void ZzMultiRadialGaugeItem::setDetailOffset(QPointF offset)
{
    if (!qIsFinite(offset.x()) || !qIsFinite(offset.y()) || d_ptr->detailOffset == offset) {
        return;
    }

    d_ptr->detailOffset = offset;
    QPointer<ZzMultiRadialGaugeItem> guard(this);
    emit detailOffsetChanged(offset);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiRadialGauge::minimum() const
{
    return d_ptr->minimum;
}

void ZzMultiRadialGauge::setMinimum(qreal minimum)
{
    if (!qIsFinite(minimum)) {
        return;
    }
    setRange(minimum, qMax(minimum, d_ptr->maximum));
}

qreal ZzMultiRadialGauge::maximum() const
{
    return d_ptr->maximum;
}

void ZzMultiRadialGauge::setMaximum(qreal maximum)
{
    if (!qIsFinite(maximum)) {
        return;
    }
    setRange(qMin(d_ptr->minimum, maximum), maximum);
}

qreal ZzMultiRadialGauge::minimumAngle() const
{
    return d_ptr->minimumAngle;
}

void ZzMultiRadialGauge::setMinimumAngle(qreal angle)
{
    if (!qIsFinite(angle))
        return;
    angle = qBound(-FullCircle, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->minimumAngle + 1.0, angle + 1.0)) {
        return;
    }
    d_ptr->minimumAngle = angle;
    update();
    emit minimumAngleChanged(angle);
}

qreal ZzMultiRadialGauge::maximumAngle() const
{
    return d_ptr->maximumAngle;
}

void ZzMultiRadialGauge::setMaximumAngle(qreal angle)
{
    if (!qIsFinite(angle))
        return;
    angle = qBound(-FullCircle, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->maximumAngle + 1.0, angle + 1.0)) {
        return;
    }
    d_ptr->maximumAngle = angle;
    update();
    emit maximumAngleChanged(angle);
}

int ZzMultiRadialGauge::majorTickCount() const
{
    return d_ptr->majorTickCount;
}

void ZzMultiRadialGauge::setMajorTickCount(int count)
{
    count = qBound(2, count, MaximumMajorTickCount);
    if (d_ptr->majorTickCount == count) {
        return;
    }
    d_ptr->majorTickCount = count;
    update();
    emit majorTickCountChanged(count);
}

int ZzMultiRadialGauge::minorTickCount() const
{
    return d_ptr->minorTickCount;
}

void ZzMultiRadialGauge::setMinorTickCount(int count)
{
    count = qBound(0, count, MaximumTickCount);
    if (d_ptr->minorTickCount == count) {
        return;
    }
    d_ptr->minorTickCount = count;
    update();
    emit minorTickCountChanged(count);
}

bool ZzMultiRadialGauge::isTrackVisible() const
{
    return d_ptr->trackVisible;
}

void ZzMultiRadialGauge::setTrackVisible(bool value)
{
    if (d_ptr->trackVisible == value)
        return;
    d_ptr->trackVisible = value;
    update();
    emit trackVisibleChanged(value);
}

qreal ZzMultiRadialGauge::trackWidth() const
{
    return d_ptr->trackWidth;
}

void ZzMultiRadialGauge::setTrackWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.5, value, 100.0);
    if (d_ptr->trackWidth == value)
        return;
    d_ptr->trackWidth = value;
    updateGeometry();
    update();
    emit trackWidthChanged(value);
}

QColor ZzMultiRadialGauge::trackColor() const
{
    return d_ptr->trackColor;
}

void ZzMultiRadialGauge::setTrackColor(QColor value)
{
    if (d_ptr->trackColor == value)
        return;
    d_ptr->trackColor = value;
    update();
    emit trackColorChanged(value);
}

Qt::PenCapStyle ZzMultiRadialGauge::trackCapStyle() const
{
    return d_ptr->trackCapStyle;
}

void ZzMultiRadialGauge::setTrackCapStyle(Qt::PenCapStyle style)
{
    if (style != Qt::FlatCap && style != Qt::SquareCap && style != Qt::RoundCap) {
        return;
    }
    if (d_ptr->trackCapStyle == style) {
        return;
    }
    d_ptr->trackCapStyle = style;
    update();
    emit trackCapStyleChanged(style);
}

bool ZzMultiRadialGauge::isProgressVisible() const
{
    return d_ptr->progressVisible;
}

void ZzMultiRadialGauge::setProgressVisible(bool value)
{
    if (d_ptr->progressVisible == value)
        return;
    d_ptr->progressVisible = value;
    update();
    emit progressVisibleChanged(value);
}

bool ZzMultiRadialGauge::isProgressOverlap() const
{
    return d_ptr->progressOverlap;
}

void ZzMultiRadialGauge::setProgressOverlap(bool value)
{
    if (d_ptr->progressOverlap == value)
        return;
    d_ptr->progressOverlap = value;
    update();
    emit progressOverlapChanged(value);
}

qreal ZzMultiRadialGauge::progressWidth() const
{
    return d_ptr->progressWidth;
}

void ZzMultiRadialGauge::setProgressWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.5, value, 100.0);
    if (d_ptr->progressWidth == value)
        return;
    d_ptr->progressWidth = value;
    updateGeometry();
    update();
    emit progressWidthChanged(value);
}

qreal ZzMultiRadialGauge::progressSpacing() const
{
    return d_ptr->progressSpacing;
}

void ZzMultiRadialGauge::setProgressSpacing(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->progressSpacing == value)
        return;
    d_ptr->progressSpacing = value;
    updateGeometry();
    update();
    emit progressSpacingChanged(value);
}

Qt::PenCapStyle ZzMultiRadialGauge::progressCapStyle() const
{
    return d_ptr->progressCapStyle;
}

void ZzMultiRadialGauge::setProgressCapStyle(Qt::PenCapStyle style)
{
    if (style != Qt::FlatCap && style != Qt::SquareCap && style != Qt::RoundCap) {
        return;
    }
    if (d_ptr->progressCapStyle == style) {
        return;
    }
    d_ptr->progressCapStyle = style;
    update();
    emit progressCapStyleChanged(style);
}

qreal ZzMultiRadialGauge::scalePadding() const
{
    return d_ptr->scalePadding;
}

void ZzMultiRadialGauge::setScalePadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 200.0);
    if (d_ptr->scalePadding == value)
        return;
    d_ptr->scalePadding = value;
    updateGeometry();
    update();
    emit scalePaddingChanged(value);
}

qreal ZzMultiRadialGauge::tickLength() const
{
    return d_ptr->tickLength;
}

void ZzMultiRadialGauge::setTickLength(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->tickLength == value)
        return;
    d_ptr->tickLength = value;
    updateGeometry();
    update();
    emit tickLengthChanged(value);
}

qreal ZzMultiRadialGauge::tickWidth() const
{
    return d_ptr->tickWidth;
}

void ZzMultiRadialGauge::setTickWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.1, value, 50.0);
    if (d_ptr->tickWidth == value)
        return;
    d_ptr->tickWidth = value;
    updateGeometry();
    update();
    emit tickWidthChanged(value);
}

qreal ZzMultiRadialGauge::majorTickLength() const
{
    return d_ptr->majorTickLength;
}

void ZzMultiRadialGauge::setMajorTickLength(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->majorTickLength == value)
        return;
    d_ptr->majorTickLength = value;
    updateGeometry();
    update();
    emit majorTickLengthChanged(value);
}

qreal ZzMultiRadialGauge::majorTickWidth() const
{
    return d_ptr->majorTickWidth;
}

void ZzMultiRadialGauge::setMajorTickWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.1, value, 50.0);
    if (d_ptr->majorTickWidth == value)
        return;
    d_ptr->majorTickWidth = value;
    updateGeometry();
    update();
    emit majorTickWidthChanged(value);
}

qreal ZzMultiRadialGauge::tickPadding() const
{
    return d_ptr->tickPadding;
}

void ZzMultiRadialGauge::setTickPadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->tickPadding == value)
        return;
    d_ptr->tickPadding = value;
    updateGeometry();
    update();
    emit tickPaddingChanged(value);
}

QColor ZzMultiRadialGauge::tickColor() const
{
    return d_ptr->tickColor;
}

void ZzMultiRadialGauge::setTickColor(QColor value)
{
    if (d_ptr->tickColor == value)
        return;
    d_ptr->tickColor = value;
    update();
    emit tickColorChanged(value);
}

bool ZzMultiRadialGauge::areLabelsVisible() const
{
    return d_ptr->labelsVisible;
}

void ZzMultiRadialGauge::setLabelsVisible(bool value)
{
    if (d_ptr->labelsVisible == value)
        return;
    d_ptr->labelsVisible = value;
    update();
    emit labelsVisibleChanged(value);
}

qreal ZzMultiRadialGauge::labelPadding() const
{
    return d_ptr->labelPadding;
}

void ZzMultiRadialGauge::setLabelPadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->labelPadding == value)
        return;
    d_ptr->labelPadding = value;
    updateGeometry();
    update();
    emit labelPaddingChanged(value);
}

int ZzMultiRadialGauge::labelFontPixelSize() const
{
    return d_ptr->labelFontPixelSize;
}

void ZzMultiRadialGauge::setLabelFontPixelSize(int size)
{
    size = qBound(1, size, 200);
    if (d_ptr->labelFontPixelSize == size) {
        return;
    }
    d_ptr->labelFontPixelSize = size;
    update();
    emit labelFontPixelSizeChanged(size);
}

QColor ZzMultiRadialGauge::labelColor() const
{
    return d_ptr->labelColor;
}

void ZzMultiRadialGauge::setLabelColor(QColor value)
{
    if (d_ptr->labelColor == value)
        return;
    d_ptr->labelColor = value;
    update();
    emit labelColorChanged(value);
}

ZzMultiRadialGauge::ZzNeedleStyle ZzMultiRadialGauge::needleStyle() const
{
    return d_ptr->needleStyle;
}

void ZzMultiRadialGauge::setNeedleStyle(ZzNeedleStyle style)
{
    if (style < NoNeedle || style > TriangleNeedle || d_ptr->needleStyle == style) {
        return;
    }
    d_ptr->needleStyle = style;
    update();
    emit needleStyleChanged(style);
}

qreal ZzMultiRadialGauge::needleWidth() const
{
    return d_ptr->needleWidth;
}

void ZzMultiRadialGauge::setNeedleWidth(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.5, value, 100.0);
    if (d_ptr->needleWidth == value)
        return;
    d_ptr->needleWidth = value;
    updateGeometry();
    update();
    emit needleWidthChanged(value);
}

qreal ZzMultiRadialGauge::needleLength() const
{
    return d_ptr->needleLength;
}

void ZzMultiRadialGauge::setNeedleLength(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.05, value, 1.2);
    if (d_ptr->needleLength == value)
        return;
    d_ptr->needleLength = value;
    updateGeometry();
    update();
    emit needleLengthChanged(value);
}

QPointF ZzMultiRadialGauge::needleOffset() const
{
    return d_ptr->needleOffset;
}

void ZzMultiRadialGauge::setNeedleOffset(QPointF offset)
{
    if (!qIsFinite(offset.x()) || !qIsFinite(offset.y()) || d_ptr->needleOffset == offset) {
        return;
    }
    d_ptr->needleOffset = offset;
    update();
    emit needleOffsetChanged(offset);
}

bool ZzMultiRadialGauge::isHubVisible() const
{
    return d_ptr->hubVisible;
}

void ZzMultiRadialGauge::setHubVisible(bool value)
{
    if (d_ptr->hubVisible == value)
        return;
    d_ptr->hubVisible = value;
    update();
    emit hubVisibleChanged(value);
}

qreal ZzMultiRadialGauge::hubRadius() const
{
    return d_ptr->hubRadius;
}

void ZzMultiRadialGauge::setHubRadius(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->hubRadius == value)
        return;
    d_ptr->hubRadius = value;
    updateGeometry();
    update();
    emit hubRadiusChanged(value);
}

QColor ZzMultiRadialGauge::hubColor() const
{
    return d_ptr->hubColor;
}

void ZzMultiRadialGauge::setHubColor(QColor value)
{
    if (d_ptr->hubColor == value)
        return;
    d_ptr->hubColor = value;
    update();
    emit hubColorChanged(value);
}

bool ZzMultiRadialGauge::isTitleVisible() const
{
    return d_ptr->titleVisible;
}

void ZzMultiRadialGauge::setTitleVisible(bool value)
{
    if (d_ptr->titleVisible == value)
        return;
    d_ptr->titleVisible = value;
    update();
    emit titleVisibleChanged(value);
}

bool ZzMultiRadialGauge::isDetailVisible() const
{
    return d_ptr->detailVisible;
}

void ZzMultiRadialGauge::setDetailVisible(bool value)
{
    if (d_ptr->detailVisible == value)
        return;
    d_ptr->detailVisible = value;
    update();
    emit detailVisibleChanged(value);
}

bool ZzMultiRadialGauge::isDetailBadgeVisible() const
{
    return d_ptr->detailBadgeVisible;
}

void ZzMultiRadialGauge::setDetailBadgeVisible(bool value)
{
    if (d_ptr->detailBadgeVisible == value)
        return;
    d_ptr->detailBadgeVisible = value;
    update();
    emit detailBadgeVisibleChanged(value);
}

int ZzMultiRadialGauge::titleFontPixelSize() const
{
    return d_ptr->titleFontPixelSize;
}

void ZzMultiRadialGauge::setTitleFontPixelSize(int size)
{
    size = qBound(1, size, 200);
    if (d_ptr->titleFontPixelSize == size) {
        return;
    }
    d_ptr->titleFontPixelSize = size;
    update();
    emit titleFontPixelSizeChanged(size);
}

int ZzMultiRadialGauge::detailFontPixelSize() const
{
    return d_ptr->detailFontPixelSize;
}

void ZzMultiRadialGauge::setDetailFontPixelSize(int size)
{
    size = qBound(1, size, 200);
    if (d_ptr->detailFontPixelSize == size) {
        return;
    }
    d_ptr->detailFontPixelSize = size;
    update();
    emit detailFontPixelSizeChanged(size);
}

QColor ZzMultiRadialGauge::titleColor() const
{
    return d_ptr->titleColor;
}

void ZzMultiRadialGauge::setTitleColor(QColor value)
{
    if (d_ptr->titleColor == value)
        return;
    d_ptr->titleColor = value;
    update();
    emit titleColorChanged(value);
}

QColor ZzMultiRadialGauge::detailTextColor() const
{
    return d_ptr->detailTextColor;
}

void ZzMultiRadialGauge::setDetailTextColor(QColor value)
{
    if (d_ptr->detailTextColor == value)
        return;
    d_ptr->detailTextColor = value;
    update();
    emit detailTextColorChanged(value);
}

qreal ZzMultiRadialGauge::detailBadgePadding() const
{
    return d_ptr->detailBadgePadding;
}

void ZzMultiRadialGauge::setDetailBadgePadding(qreal value)
{
    if (!qIsFinite(value))
        return;
    value = qBound<qreal>(0.0, value, 100.0);
    if (d_ptr->detailBadgePadding == value)
        return;
    d_ptr->detailBadgePadding = value;
    updateGeometry();
    update();
    emit detailBadgePaddingChanged(value);
}

QString ZzMultiRadialGauge::valueSuffix() const
{
    return d_ptr->valueSuffix;
}

void ZzMultiRadialGauge::setValueSuffix(QString value)
{
    if (d_ptr->valueSuffix == value)
        return;
    d_ptr->valueSuffix = value;
    update();
    emit valueSuffixChanged(value);
}

int ZzMultiRadialGauge::valueDecimals() const
{
    return d_ptr->valueDecimals;
}

void ZzMultiRadialGauge::setValueDecimals(int decimals)
{
    decimals = qBound(0, decimals, 6);
    if (d_ptr->valueDecimals == decimals) {
        return;
    }
    d_ptr->valueDecimals = decimals;
    update();
    emit valueDecimalsChanged(decimals);
}

int ZzMultiRadialGauge::valueAnimationDuration() const
{
    return d_ptr->valueAnimationDuration;
}

void ZzMultiRadialGauge::setValueAnimationDuration(int duration)
{
    duration = qBound(0, duration, 5000);
    if (d_ptr->valueAnimationDuration == duration) {
        return;
    }
    d_ptr->valueAnimationDuration = duration;
    if (duration == 0) {
        d_ptr->valueAnimation->stop();
        synchronizeDisplayedValues();
    }
    emit valueAnimationDurationChanged(duration);
}

} // namespace ZzFluentUI
