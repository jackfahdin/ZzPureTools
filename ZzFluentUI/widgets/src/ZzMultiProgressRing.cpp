#include <ZzFluentUI/ZzMultiProgressRing.h>

#include "private/ZzMultiProgressRingPrivate.h"
#include "private/ZzGaugeSupportPrivate.h"

#include <QEvent>
#include <QEasingCurve>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointer>
#include <QSizePolicy>
#include <QStyleOption>
#include <QVariantAnimation>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ZzFluentUI {
namespace {

constexpr qreal FullCircle = 360.0;

QPalette::ColorRole accentRole()
{
    return QPalette::Accent;
}

} // namespace

ZzMultiProgressRingItem::ZzMultiProgressRingItem(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzMultiProgressRingItemPrivate>())
{
}

ZzMultiProgressRingItem::ZzMultiProgressRingItem(const QString &label, qreal value, const QColor &color,
                                                 QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzMultiProgressRingItemPrivate>())
{
    d_ptr->label = label;
    d_ptr->value = qIsFinite(value) ? value : 0.0;
    d_ptr->color = color;
}

ZzMultiProgressRing::ZzMultiProgressRing(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<ZzMultiProgressRingPrivate>())
{

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setAutoFillBackground(false);

    d_ptr->valueAnimation = new QVariantAnimation(this);
    d_ptr->valueAnimation->setStartValue(0.0);
    d_ptr->valueAnimation->setEndValue(1.0);
    d_ptr->valueAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(d_ptr->valueAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const qreal progress = value.toReal();
        for (const ZzMultiProgressRingItem *item : std::as_const(d_ptr->items)) {
            const qreal startValue = d_ptr->animationStartValues.value(item, item->value());
            d_ptr->displayedValues.insert(item, std::lerp(startValue, item->value(), progress));
        }
        update();
    });
    connect(d_ptr->valueAnimation, &QVariantAnimation::finished, this,
            [this] { synchronizeDisplayedValues(); });
}

ZzMultiProgressRing::~ZzMultiProgressRing()
{
    d_ptr->valueAnimation->stop();
    for (ZzMultiProgressRingItem *item : std::as_const(d_ptr->items)) {
        disconnect(item, nullptr, this, nullptr);
    }
    d_ptr->items.clear();
    d_ptr->displayedValues.clear();
    d_ptr->animationStartValues.clear();
}

void ZzMultiProgressRing::setRange(qreal minimum, qreal maximum)
{
    if (!qIsFinite(minimum) || !qIsFinite(maximum)) {
        return;
    }

    const bool lowerChanged = d_ptr->minimum != minimum;
    const bool upperChanged = d_ptr->maximum != maximum;
    if (!lowerChanged && !upperChanged)
        return;
    d_ptr->minimum = minimum;
    d_ptr->maximum = maximum;
    update();
    QPointer<ZzMultiProgressRing> guard(this);
    if (lowerChanged)
        emit minimumChanged(minimum);
    if (!guard || d_ptr->minimum != minimum || d_ptr->maximum != maximum)
        return;
    if (upperChanged)
        emit maximumChanged(maximum);
}

QList<ZzMultiProgressRingItem *> ZzMultiProgressRing::items() const
{
    return d_ptr->items;
}

ZzMultiProgressRingItem *ZzMultiProgressRing::addItem(const QString &label, qreal value, const QColor &color)
{
    auto *item = new ZzMultiProgressRingItem(label, value, color, this);
    QPointer<ZzMultiProgressRingItem> guard(item);
    addItem(item);
    return guard.data();
}

void ZzMultiProgressRing::addItem(ZzMultiProgressRingItem *item)
{
    if (!item || item->d_ptr->adoptionInProgress || d_ptr->items.contains(item) ||
        item->property("_zzGaugePendingDelete").toBool()) {
        return;
    }

    QPointer<ZzMultiProgressRingItem> guard(item);
    QPointer<ZzMultiProgressRing> ownerGuard(this);
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
    updateGeometry();
    update();
    emit itemsChanged();
}

void ZzMultiProgressRing::removeItem(ZzMultiProgressRingItem *item)
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
    updateGeometry();
    update();
    emit itemsChanged();
}

void ZzMultiProgressRing::clearItems()
{
    if (d_ptr->items.isEmpty()) {
        return;
    }

    d_ptr->valueAnimation->stop();
    const auto oldItems = d_ptr->items;
    d_ptr->items.clear();
    d_ptr->displayedValues.clear();
    d_ptr->animationStartValues.clear();
    for (ZzMultiProgressRingItem *item : oldItems) {
        disconnect(item, nullptr, this, nullptr);
        item->setProperty("_zzGaugePendingDelete", true);
        item->deleteLater();
    }
    updateGeometry();
    update();
    emit itemsChanged();
}

QSize ZzMultiProgressRing::sizeHint() const
{
    return QSize(220, 220);
}

QSize ZzMultiProgressRing::minimumSizeHint() const
{
    return QSize(80, 80);
}

qreal ZzMultiProgressRing::itemFraction(qreal value) const
{
    return zzGaugeFraction(value, d_ptr->minimum, d_ptr->maximum);
}

qreal ZzMultiProgressRing::displayedValue(const ZzMultiProgressRingItem *item) const
{
    return item ? d_ptr->displayedValues.value(item, item->value()) : 0.0;
}

void ZzMultiProgressRing::connectItem(ZzMultiProgressRingItem *item)
{
    connect(item, &ZzMultiProgressRingItem::valueChanged, this, [this](qreal) {
        startValueAnimation();
        emit itemsChanged();
    });
    connect(item, &ZzMultiProgressRingItem::labelChanged, this, [this](const QString &) {
        update();
        emit itemsChanged();
    });
    connect(item, &ZzMultiProgressRingItem::colorChanged, this, [this](const QColor &) {
        update();
        emit itemsChanged();
    });
    connect(item, &QObject::destroyed, this, [this, item] {
        if (d_ptr->items.removeOne(item)) {
            d_ptr->displayedValues.remove(item);
            d_ptr->animationStartValues.remove(item);
            if (d_ptr->items.isEmpty())
                d_ptr->valueAnimation->stop();
            updateGeometry();
            update();
            emit itemsChanged();
        }
    });
}

void ZzMultiProgressRing::startValueAnimation()
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

void ZzMultiProgressRing::synchronizeDisplayedValues()
{
    d_ptr->valueAnimation->stop();
    for (const ZzMultiProgressRingItem *item : std::as_const(d_ptr->items)) {
        d_ptr->displayedValues.insert(item, item->value());
    }
    d_ptr->animationStartValues.clear();
    update();
}

ZzMultiProgressRingItem::~ZzMultiProgressRingItem() = default;

bool ZzMultiProgressRing::event(QEvent *event)
{
    QPointer<ZzMultiProgressRing> guard(this);
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


void ZzMultiProgressRing::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStyleOption option;
    option.initFrom(this);
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QPalette::ColorGroup colorGroup =
        enabled ? (option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive)
                : QPalette::Disabled;
    const QColor fallbackRingColor = option.palette.color(colorGroup, accentRole());
    const QColor resolvedTrackColor = enabled && d_ptr->trackColor.isValid()
                                          ? d_ptr->trackColor
                                          : option.palette.color(colorGroup, QPalette::Mid);
    const QColor resolvedLabelColor = enabled && d_ptr->labelColor.isValid()
                                          ? d_ptr->labelColor
                                          : option.palette.color(colorGroup, QPalette::Text);

    const qreal side = qMin(width(), height());
    const QPointF center = QRectF(rect()).center();
    const qreal outerRadius = side * 0.5 - d_ptr->ringPadding - d_ptr->ringWidth * 0.5 - 1.0;
    const qreal painterStartAngle = 90.0 - d_ptr->startAngle;

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    qreal innermostRadius = outerRadius;
    for (qsizetype index = 0; index < d_ptr->items.size(); ++index) {
        const ZzMultiProgressRingItem *item = d_ptr->items.at(index);
        const qreal radius =
            outerRadius - static_cast<qreal>(index) * (d_ptr->ringWidth + d_ptr->ringSpacing);
        if (!item || radius <= d_ptr->ringWidth * 0.5) {
            break;
        }
        innermostRadius = radius;

        const QRectF ringRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
        if (d_ptr->trackVisible && d_ptr->sweepAngle > 0.0) {
            painter.setPen(QPen(resolvedTrackColor, d_ptr->ringWidth, Qt::SolidLine, d_ptr->capStyle));
            painter.drawArc(ringRect, qRound(painterStartAngle * 16.0), qRound(-d_ptr->sweepAngle * 16.0));
        }

        const qreal fraction = itemFraction(displayedValue(item));
        if (fraction <= 0.0 || d_ptr->sweepAngle <= 0.0) {
            continue;
        }

        const QColor ringColor = enabled && item->color().isValid() ? item->color() : fallbackRingColor;
        painter.setPen(QPen(ringColor, d_ptr->ringWidth, Qt::SolidLine, d_ptr->capStyle));
        painter.drawArc(ringRect, qRound(painterStartAngle * 16.0),
                        qRound(-d_ptr->sweepAngle * fraction * 16.0));
    }

    if (!d_ptr->detailsVisible || d_ptr->items.isEmpty()) {
        return;
    }

    QFont labelFont = font();
    labelFont.setPixelSize(d_ptr->labelFontPixelSize > 0 ? d_ptr->labelFontPixelSize
                                                         : qBound(9, qRound(side * 0.045), 13));
    QFont valueFont = font();
    valueFont.setPixelSize(d_ptr->valueFontPixelSize > 0 ? d_ptr->valueFontPixelSize
                                                         : qBound(9, qRound(side * 0.045), 13));
    valueFont.setWeight(QFont::DemiBold);

    const QFontMetricsF labelMetrics(labelFont);
    const QFontMetricsF valueMetrics(valueFont);
    const qreal badgeHeight = valueMetrics.height() + 2.0;
    const qreal rowSpacing = qMax(3.0, side * 0.012);
    const qreal rowHeight = labelMetrics.height() + 2.0 + badgeHeight;
    const qreal totalHeight = static_cast<qreal>(d_ptr->items.size()) * rowHeight +
                              static_cast<qreal>(qMax<qsizetype>(0, d_ptr->items.size() - 1)) * rowSpacing;
    qreal rowTop = center.y() - totalHeight * 0.5;
    const qreal contentHalfWidth = qMax(16.0, innermostRadius - d_ptr->ringWidth * 0.5 - 8.0);

    for (const ZzMultiProgressRingItem *item : std::as_const(d_ptr->items)) {
        if (!item) {
            continue;
        }

        const QColor itemColor = enabled && item->color().isValid() ? item->color() : fallbackRingColor;
        painter.setFont(labelFont);
        painter.setPen(resolvedLabelColor);
        painter.drawText(
            QRectF(center.x() - contentHalfWidth, rowTop, contentHalfWidth * 2.0, labelMetrics.height()),
            Qt::AlignCenter, item->label());

        const QString valueText =
            QString::number(displayedValue(item), 'f', d_ptr->valueDecimals) + d_ptr->valueSuffix;
        const qreal valueTop = rowTop + labelMetrics.height() + 2.0;
        painter.setFont(valueFont);
        if (d_ptr->valueBadgeVisible) {
            const qreal badgeWidth =
                qMin(contentHalfWidth * 2.0, valueMetrics.horizontalAdvance(valueText) + 16.0);
            const QRectF badgeRect(center.x() - badgeWidth * 0.5, valueTop, badgeWidth, badgeHeight);
            painter.setPen(QPen(itemColor, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(badgeRect, badgeRect.height() * 0.5, badgeRect.height() * 0.5);
            painter.setPen(itemColor);
            painter.drawText(badgeRect, Qt::AlignCenter, valueText);
        } else {
            painter.setPen(itemColor);
            painter.drawText(
                QRectF(center.x() - contentHalfWidth, valueTop, contentHalfWidth * 2.0, badgeHeight),
                Qt::AlignCenter, valueText);
        }

        rowTop += rowHeight + rowSpacing;
    }
}

QString ZzMultiProgressRingItem::label() const
{
    return d_ptr->label;
}

void ZzMultiProgressRingItem::setLabel(QString label)
{
    if (d_ptr->label == label) {
        return;
    }

    d_ptr->label = std::move(label);
    QPointer<ZzMultiProgressRingItem> guard(this);
    emit labelChanged(d_ptr->label);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiProgressRingItem::value() const
{
    return d_ptr->value;
}

void ZzMultiProgressRingItem::setValue(qreal value)
{
    if (!qIsFinite(value) || qFuzzyCompare(d_ptr->value + 1.0, value + 1.0)) {
        return;
    }

    d_ptr->value = value;
    QPointer<ZzMultiProgressRingItem> guard(this);
    emit valueChanged(value);
    if (!guard)
        return;
    emit itemChanged();
}

QColor ZzMultiProgressRingItem::color() const
{
    return d_ptr->color;
}

void ZzMultiProgressRingItem::setColor(QColor color)
{
    if (d_ptr->color == color) {
        return;
    }

    d_ptr->color = color;
    QPointer<ZzMultiProgressRingItem> guard(this);
    emit colorChanged(color);
    if (!guard)
        return;
    emit itemChanged();
}

qreal ZzMultiProgressRing::minimum() const
{
    return d_ptr->minimum;
}

void ZzMultiProgressRing::setMinimum(qreal minimum)
{
    if (!qIsFinite(minimum) || qFuzzyCompare(d_ptr->minimum + 1.0, minimum + 1.0)) {
        return;
    }

    d_ptr->minimum = minimum;
    update();
    emit minimumChanged(minimum);
}

qreal ZzMultiProgressRing::maximum() const
{
    return d_ptr->maximum;
}

void ZzMultiProgressRing::setMaximum(qreal maximum)
{
    if (!qIsFinite(maximum) || qFuzzyCompare(d_ptr->maximum + 1.0, maximum + 1.0)) {
        return;
    }

    d_ptr->maximum = maximum;
    update();
    emit maximumChanged(maximum);
}

qreal ZzMultiProgressRing::startAngle() const
{
    return d_ptr->startAngle;
}

void ZzMultiProgressRing::setStartAngle(qreal angle)
{
    if (!qIsFinite(angle)) {
        return;
    }

    angle = std::fmod(angle, FullCircle);
    if (qFuzzyCompare(d_ptr->startAngle + 1.0, angle + 1.0)) {
        return;
    }

    d_ptr->startAngle = angle;
    update();
    emit startAngleChanged(angle);
}

qreal ZzMultiProgressRing::sweepAngle() const
{
    return d_ptr->sweepAngle;
}

void ZzMultiProgressRing::setSweepAngle(qreal angle)
{
    if (!qIsFinite(angle)) {
        return;
    }

    angle = qBound(0.0, angle, FullCircle);
    if (qFuzzyCompare(d_ptr->sweepAngle + 1.0, angle + 1.0)) {
        return;
    }

    d_ptr->sweepAngle = angle;
    update();
    emit sweepAngleChanged(angle);
}

qreal ZzMultiProgressRing::ringWidth() const
{
    return d_ptr->ringWidth;
}

void ZzMultiProgressRing::setRingWidth(qreal width)
{
    if (!qIsFinite(width)) {
        return;
    }

    width = qBound(0.5, width, 100.0);
    if (qFuzzyCompare(d_ptr->ringWidth, width)) {
        return;
    }

    d_ptr->ringWidth = width;
    updateGeometry();
    update();
    emit ringWidthChanged(width);
}

qreal ZzMultiProgressRing::ringSpacing() const
{
    return d_ptr->ringSpacing;
}

void ZzMultiProgressRing::setRingSpacing(qreal spacing)
{
    if (!qIsFinite(spacing)) {
        return;
    }

    spacing = qBound(0.0, spacing, 100.0);
    if (qFuzzyCompare(d_ptr->ringSpacing + 1.0, spacing + 1.0)) {
        return;
    }

    d_ptr->ringSpacing = spacing;
    updateGeometry();
    update();
    emit ringSpacingChanged(spacing);
}

qreal ZzMultiProgressRing::ringPadding() const
{
    return d_ptr->ringPadding;
}

void ZzMultiProgressRing::setRingPadding(qreal padding)
{
    if (!qIsFinite(padding)) {
        return;
    }

    padding = qBound(0.0, padding, 200.0);
    if (qFuzzyCompare(d_ptr->ringPadding + 1.0, padding + 1.0)) {
        return;
    }

    d_ptr->ringPadding = padding;
    updateGeometry();
    update();
    emit ringPaddingChanged(padding);
}

Qt::PenCapStyle ZzMultiProgressRing::capStyle() const
{
    return d_ptr->capStyle;
}

void ZzMultiProgressRing::setCapStyle(Qt::PenCapStyle style)
{
    if (style != Qt::FlatCap && style != Qt::SquareCap && style != Qt::RoundCap) {
        return;
    }
    if (d_ptr->capStyle == style) {
        return;
    }

    d_ptr->capStyle = style;
    update();
    emit capStyleChanged(style);
}

bool ZzMultiProgressRing::isTrackVisible() const
{
    return d_ptr->trackVisible;
}

void ZzMultiProgressRing::setTrackVisible(bool value)
{
    if (d_ptr->trackVisible == value)
        return;
    d_ptr->trackVisible = value;
    update();
    emit trackVisibleChanged(value);
}

QColor ZzMultiProgressRing::trackColor() const
{
    return d_ptr->trackColor;
}

void ZzMultiProgressRing::setTrackColor(QColor value)
{
    if (d_ptr->trackColor == value)
        return;
    d_ptr->trackColor = value;
    update();
    emit trackColorChanged(value);
}

bool ZzMultiProgressRing::areDetailsVisible() const
{
    return d_ptr->detailsVisible;
}

void ZzMultiProgressRing::setDetailsVisible(bool value)
{
    if (d_ptr->detailsVisible == value)
        return;
    d_ptr->detailsVisible = value;
    update();
    emit detailsVisibleChanged(value);
}

bool ZzMultiProgressRing::isValueBadgeVisible() const
{
    return d_ptr->valueBadgeVisible;
}

void ZzMultiProgressRing::setValueBadgeVisible(bool value)
{
    if (d_ptr->valueBadgeVisible == value)
        return;
    d_ptr->valueBadgeVisible = value;
    update();
    emit valueBadgeVisibleChanged(value);
}

QColor ZzMultiProgressRing::labelColor() const
{
    return d_ptr->labelColor;
}

void ZzMultiProgressRing::setLabelColor(QColor value)
{
    if (d_ptr->labelColor == value)
        return;
    d_ptr->labelColor = value;
    update();
    emit labelColorChanged(value);
}

QString ZzMultiProgressRing::valueSuffix() const
{
    return d_ptr->valueSuffix;
}

void ZzMultiProgressRing::setValueSuffix(QString value)
{
    if (d_ptr->valueSuffix == value)
        return;
    d_ptr->valueSuffix = value;
    update();
    emit valueSuffixChanged(value);
}

int ZzMultiProgressRing::valueDecimals() const
{
    return d_ptr->valueDecimals;
}

void ZzMultiProgressRing::setValueDecimals(int decimals)
{
    decimals = qBound(0, decimals, 6);
    if (d_ptr->valueDecimals == decimals) {
        return;
    }

    d_ptr->valueDecimals = decimals;
    update();
    emit valueDecimalsChanged(decimals);
}

int ZzMultiProgressRing::labelFontPixelSize() const
{
    return d_ptr->labelFontPixelSize;
}

void ZzMultiProgressRing::setLabelFontPixelSize(int size)
{
    size = qBound(0, size, 200);
    if (d_ptr->labelFontPixelSize == size) {
        return;
    }

    d_ptr->labelFontPixelSize = size;
    update();
    emit labelFontPixelSizeChanged(size);
}

int ZzMultiProgressRing::valueFontPixelSize() const
{
    return d_ptr->valueFontPixelSize;
}

void ZzMultiProgressRing::setValueFontPixelSize(int size)
{
    size = qBound(0, size, 200);
    if (d_ptr->valueFontPixelSize == size) {
        return;
    }

    d_ptr->valueFontPixelSize = size;
    update();
    emit valueFontPixelSizeChanged(size);
}

int ZzMultiProgressRing::valueAnimationDuration() const
{
    return d_ptr->valueAnimationDuration;
}

void ZzMultiProgressRing::setValueAnimationDuration(int duration)
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
