#include <ZzFluentUI/ZzMultiRadialGauge.h>

#include "private/ZzMultiRadialGaugePrivate.h"
#include "private/ZzGaugeSupport_p.h"

#include <QEvent>
#include <QEasingCurve>
#include <QPointer>
#include <QSizePolicy>
#include <QVariantAnimation>

#include <cmath>
#include <utility>

namespace ZzFluentUI {
namespace {
constexpr qreal FullCircle = 360.0;

}

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

} // namespace ZzFluentUI
