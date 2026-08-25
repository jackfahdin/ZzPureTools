#include <ZzFluentUI/ZzAnnotatedScrollBar.h>

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEvent>
#include <QtGui/QHelpEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolTip>

#include "private/ZzAnnotatedScrollBarPrivate.h"

namespace ZzFluentUI {

ZzAnnotatedScrollBar::ZzAnnotatedScrollBar(QWidget *parent)
    : ZzScrollBar(parent)
    , d_ptr(std::make_unique<ZzAnnotatedScrollBarPrivate>(this))
{
}

ZzAnnotatedScrollBar::ZzAnnotatedScrollBar(
    Qt::Orientation orientation,
    QWidget *parent)
    : ZzScrollBar(orientation, parent)
    , d_ptr(std::make_unique<ZzAnnotatedScrollBarPrivate>(this))
{
}

ZzAnnotatedScrollBar::~ZzAnnotatedScrollBar() = default;

QAbstractItemModel *ZzAnnotatedScrollBar::markerModel() const noexcept
{
    return d_ptr->model.data();
}

void ZzAnnotatedScrollBar::setMarkerModel(QAbstractItemModel *model)
{
    d_ptr->setModel(model);
}

bool ZzAnnotatedScrollBar::markersInteractive() const noexcept
{
    return d_ptr->interactive;
}

void ZzAnnotatedScrollBar::setMarkersInteractive(bool interactive)
{
    d_ptr->interactive = interactive;
}

void ZzAnnotatedScrollBar::initMarkerStyleOption(
    QStyleOptionSlider *option) const
{
    Q_ASSERT(option != nullptr);
    initStyleOption(option);
}

QModelIndex ZzAnnotatedScrollBar::markerAt(const QPoint &point) const
{
    d_ptr->ensurePixelBuckets();
    const QRect groove = d_ptr->grooveRect();
    if (!groove.contains(point)) {
        return {};
    }
    const int primary = orientation() == Qt::Vertical ? point.y() : point.x();
    const auto iterator = std::lower_bound(
        d_ptr->pixelBuckets.cbegin(),
        d_ptr->pixelBuckets.cend(),
        primary,
        [](const ZzPixelBucket &bucket, int pixel) {
            return bucket.pixel < pixel;
        });
    const auto matches = [primary](const ZzPixelBucket &bucket) {
        return qAbs(bucket.pixel - primary) <= 1;
    };
    if (iterator != d_ptr->pixelBuckets.cend() && matches(*iterator)) {
        return d_ptr->markers.at(iterator->markerIndex).source;
    }
    if (iterator != d_ptr->pixelBuckets.cbegin()) {
        const auto previous = std::prev(iterator);
        if (matches(*previous)) {
            return d_ptr->markers.at(previous->markerIndex).source;
        }
    }
    return {};
}

void ZzAnnotatedScrollBar::paintEvent(QPaintEvent *event)
{
    ZzScrollBar::paintEvent(event);
    d_ptr->ensurePixelBuckets();
    if (d_ptr->pixelBuckets.isEmpty()) {
        return;
    }

    QPainter painter(this);
    const QRect groove = d_ptr->grooveRect();
    for (const ZzPixelBucket &bucket : d_ptr->pixelBuckets) {
        const ZzMarker &marker = d_ptr->markers.at(bucket.markerIndex);
        painter.fillRect(
            orientation() == Qt::Vertical
                ? QRect(groove.left(), bucket.pixel, groove.width(), 1)
                : QRect(bucket.pixel, groove.top(), 1, groove.height()),
            marker.color);
    }
}

void ZzAnnotatedScrollBar::mousePressEvent(QMouseEvent *event)
{
    if (event != nullptr && event->button() == Qt::LeftButton && d_ptr->interactive) {
        const QModelIndex source = markerAt(event->position().toPoint());
        if (source.isValid()) {
            for (const ZzMarker &marker : d_ptr->markers) {
                if (marker.source == source) {
                    setValue(minimum() + qRound(
                        marker.position
                        * static_cast<qreal>(maximum() - minimum())));
                    Q_EMIT markerActivated(source);
                    event->accept();
                    return;
                }
            }
        }
    }
    ZzScrollBar::mousePressEvent(event);
}

bool ZzAnnotatedScrollBar::event(QEvent *event)
{
    if (event != nullptr && event->type() == QEvent::ToolTip) {
        const auto *helpEvent = static_cast<QHelpEvent *>(event);
        const QModelIndex source = markerAt(helpEvent->pos());
        if (source.isValid()) {
            const QString toolTip = source.data(Qt::ToolTipRole).toString();
            if (!toolTip.isEmpty()) {
                QToolTip::showText(helpEvent->globalPos(), toolTip, this);
                return true;
            }
        }
    }
    return ZzScrollBar::event(event);
}

void ZzAnnotatedScrollBar::resizeEvent(QResizeEvent *event)
{
    ZzScrollBar::resizeEvent(event);
    d_ptr->rebuildPixelBuckets();
}

void ZzAnnotatedScrollBar::changeEvent(QEvent *event)
{
    ZzScrollBar::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::LayoutDirectionChange:
        d_ptr->rebuildPixelBuckets();
        break;
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        d_ptr->theme.refreshFallback();
        d_ptr->rebuildMarkerCache();
        break;
    default:
        break;
    }
}

void ZzAnnotatedScrollBar::sliderChange(SliderChange change)
{
    ZzScrollBar::sliderChange(change);
    if (change == SliderOrientationChange) {
        d_ptr->rebuildPixelBuckets();
    }
}

} // namespace ZzFluentUI
