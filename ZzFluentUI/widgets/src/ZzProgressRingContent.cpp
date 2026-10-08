#include <ZzFluentUI/ZzProgressRing.h>

#include <algorithm>
#include <cmath>

#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QResizeEvent>

#include "private/ZzProgressRingPrivate.h"

namespace ZzFluentUI {

#define ZZ_RING_PROPERTY(Type, Getter, Setter, Member) \
    Type ZzProgressRing::Getter() const { return d_ptr->Member; } \
    void ZzProgressRing::Setter(Type value) \
    { \
        if (d_ptr->Member == value) return; \
        d_ptr->Member = value; \
        update(); \
        Q_EMIT Getter##Changed(value); \
    }

ZZ_RING_PROPERTY(QString, title, setTitle, title)
ZZ_RING_PROPERTY(QFont, titleFont, setTitleFont, customTitleFont)
ZZ_RING_PROPERTY(QFont, valueFont, setValueFont, customValueFont)
ZZ_RING_PROPERTY(QColor, titleColor, setTitleColor, titleColor)
ZZ_RING_PROPERTY(QColor, valueColor, setValueColor, valueColor)
ZZ_RING_PROPERTY(QColor, ringColor, setRingColor, ringColor)
ZZ_RING_PROPERTY(QColor, trackColor, setTrackColor, trackColor)
#undef ZZ_RING_PROPERTY

int ZzProgressRing::textSpacing() const noexcept { return d_ptr->textSpacing; }

void ZzProgressRing::setTextSpacing(int spacing)
{
    spacing = std::clamp(spacing, 0, 100);
    if (d_ptr->textSpacing == spacing) return;
    d_ptr->textSpacing = spacing;
    update();
    Q_EMIT textSpacingChanged(spacing);
}

QRectF ZzProgressRing::centerContentRect() const
{
    const QRectF content(contentsRect());
    const qreal side = std::min(content.width(), content.height());
    const qreal inset = std::min(side / 2.0, d_ptr->thickness + 6.0);
    QRectF result(0, 0, std::max(0.0, side - 2 * inset), std::max(0.0, side - 2 * inset));
    result.moveCenter(content.center());
    return result;
}

QWidget *ZzProgressRing::centerWidget() const noexcept { return d_ptr->centerWidget.data(); }

void ZzProgressRing::releaseCenterWidget()
{
    QObject::disconnect(d_ptr->centerDestroyedConnection);
    if (d_ptr->centerWidget) d_ptr->centerWidget->removeEventFilter(this);
    d_ptr->centerWidget = nullptr;
    const quint64 revision = ++d_ptr->centerRevision;
    update();
    // setParent and QObject::destroyed both run before Qt has finished updating
    // the parent/child relationship. Wait before slots can reparent the child
    // or delete the ring, and suppress a notification superseded by a new center.
    QTimer::singleShot(0, this, [this, revision] {
        if (d_ptr->centerRevision == revision) Q_EMIT centerWidgetChanged(nullptr);
    });
}

void ZzProgressRing::setCenterWidget(QWidget *widget)
{
    if (widget == this || widget == centerWidget()
        || (widget && widget->isAncestorOf(this))
        || (widget && centerWidget() && centerWidget()->isAncestorOf(widget))) return;

    QPointer<ZzProgressRing> guard(this);
    QPointer<QWidget> candidate(widget);
    QPointer<QWidget> previous = d_ptr->centerWidget;
    QObject::disconnect(d_ptr->centerDestroyedConnection);
    if (previous) previous->removeEventFilter(this);
    d_ptr->centerWidget = nullptr;
    const quint64 revision = ++d_ptr->centerRevision;
    delete previous.data();
    if (!guard || d_ptr->centerRevision != revision) return;

    d_ptr->centerWidget = candidate;
    if (candidate) {
        candidate->installEventFilter(this);
        d_ptr->centerDestroyedConnection = connect(candidate, &QObject::destroyed, this,
            [this, revision] {
                if (d_ptr->centerRevision == revision) releaseCenterWidget();
            });
        candidate->setParent(this);
        if (!guard || d_ptr->centerRevision != revision) return;
        updateCenterWidgetGeometry();
        if (!guard || d_ptr->centerRevision != revision) return;
    }
    update();
    Q_EMIT centerWidgetChanged(d_ptr->centerWidget.data());
}

QWidget *ZzProgressRing::takeCenterWidget()
{
    QPointer<QWidget> widget = d_ptr->centerWidget;
    if (!widget) return nullptr;
    QPointer<ZzProgressRing> guard(this);
    QObject::disconnect(d_ptr->centerDestroyedConnection);
    widget->removeEventFilter(this);
    d_ptr->centerWidget = nullptr;
    const quint64 revision = ++d_ptr->centerRevision;
    widget->hide();
    if (guard && widget && d_ptr->centerWidget != widget
        && widget->parentWidget() == this) widget->setParent(nullptr);
    if (guard && d_ptr->centerRevision == revision) {
        update();
        Q_EMIT centerWidgetChanged(nullptr);
    }
    // Reentrant slots may delete or re-adopt the released widget.
    return widget && !widget->parentWidget() ? widget.data() : nullptr;
}

void ZzProgressRing::updateCenterWidgetGeometry()
{
    QPointer<QWidget> widget = d_ptr->centerWidget;
    if (!widget) return;
    QPointer<ZzProgressRing> guard(this);
    const quint64 revision = d_ptr->centerRevision;
    const QRectF content = centerContentRect();
    const int left = qCeil(content.left());
    const int top = qCeil(content.top());
    widget->setGeometry(left, top,
        std::max(0, qFloor(content.right()) - left),
        std::max(0, qFloor(content.bottom()) - top));
    if (!guard || !widget || d_ptr->centerRevision != revision) return;
    widget->setVisible(isTextVisible() && !content.isEmpty());
    if (!guard || !widget || d_ptr->centerRevision != revision) return;
    widget->raise();
}

void ZzProgressRing::setTextVisible(bool visible)
{
    QProgressBar::setTextVisible(visible);
    updateCenterWidgetGeometry();
}

void ZzProgressRing::resizeEvent(QResizeEvent *event)
{
    QProgressBar::resizeEvent(event);
    updateCenterWidgetGeometry();
}

bool ZzProgressRing::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ParentChange && watched == d_ptr->centerWidget.data()
        && d_ptr->centerWidget->parentWidget() != this) {
        releaseCenterWidget();
    }
    return QProgressBar::eventFilter(watched, event);
}

} // namespace ZzFluentUI
