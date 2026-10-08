#include <ZzFluentUI/ZzMultiProgressRing.h>

#include "private/ZzMultiProgressRingPrivate.h"
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

}

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

} // namespace ZzFluentUI
