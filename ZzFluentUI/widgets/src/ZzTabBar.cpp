#include <ZzFluentUI/ZzTabBar.h>

#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolButton>

#include "private/ZzTabBarPrivate.h"

#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzFluentUI {

ZzTabBar::ZzTabBar(QWidget *parent)
    : QTabBar(parent)
    , d_ptr(std::make_unique<ZzTabBarPrivate>(this))
{
    setAcceptDrops(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    d_ptr->newTabButton = new QToolButton(this);
    d_ptr->newTabButton->setObjectName(QStringLiteral("zzTabNewButton"));
    d_ptr->newTabButton->setText(QStringLiteral("+"));
    d_ptr->newTabButton->setAutoRaise(true);
    d_ptr->newTabButton->setToolTip(QStringLiteral("新建标签页"));
    d_ptr->newTabButton->setAccessibleName(QStringLiteral("新建标签页"));
    connect(d_ptr->newTabButton, &QToolButton::clicked, this, &ZzTabBar::newTabRequested);
}

QWidget *ZzTabBar::newTabButton() const noexcept { return d_ptr->newTabButton; }

ZzTabBar::~ZzTabBar() = default;

bool ZzTabBar::isTearOffEnabled() const noexcept
{
    return d_ptr->tearOffEnabled;
}

void ZzTabBar::setTearOffEnabled(bool enabled)
{
    if (d_ptr->tearOffEnabled == enabled) {
        return;
    }
    d_ptr->tearOffEnabled = enabled;
    Q_EMIT tearOffEnabledChanged(enabled);
}

bool ZzTabBar::isTabTransferEnabled() const noexcept
{
    return d_ptr->tabTransferEnabled;
}

void ZzTabBar::setTabTransferEnabled(bool enabled)
{
    if (d_ptr->tabTransferEnabled == enabled) {
        return;
    }
    d_ptr->tabTransferEnabled = enabled;
    if (!enabled && d_ptr->dropIndex >= 0) {
        d_ptr->dropIndex = -1;
        update();
    }
    Q_EMIT tabTransferEnabledChanged(enabled);
}

void ZzTabBar::mousePressEvent(QMouseEvent *event)
{
    QTabBar::mousePressEvent(event);
    d_ptr->clearPressState();
    if (event == nullptr || event->button() != Qt::LeftButton
        || !isMovable() || d_ptr->host.isNull()) {
        return;
    }

    const QPoint position = event->position().toPoint();
    const int index = tabAt(position);
    if (index < 0 || index >= count()) {
        return;
    }
    d_ptr->pressedIndex = index;
    d_ptr->pressedPage = d_ptr->host->widget(index);
    d_ptr->pressPosition = position;
}

void ZzTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event != nullptr
        && (event->buttons() & Qt::LeftButton) != Qt::NoButton
        && d_ptr->pressedIndex >= 0
        && !d_ptr->pressedPage.isNull()
        && !d_ptr->dragging
        && (event->position().toPoint() - d_ptr->pressPosition)
               .manhattanLength()
            >= QApplication::startDragDistance()) {
        event->accept();
        d_ptr->startDrag();
        return;
    }
    QTabBar::mouseMoveEvent(event);
}

void ZzTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event);
    if (!d_ptr->dragging) {
        d_ptr->clearPressState();
    }
}

void ZzTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    if (event != nullptr
        && d_ptr->validPayload(event->mimeData()) != nullptr) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
        d_ptr->dropIndex = d_ptr->insertionIndex(event->position().toPoint());
        update();
        return;
    }
    if (event != nullptr) {
        event->ignore();
    }
}

void ZzTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    if (event != nullptr
        && d_ptr->validPayload(event->mimeData()) != nullptr) {
        const int nextIndex = d_ptr->insertionIndex(
            event->position().toPoint());
        if (d_ptr->dropIndex != nextIndex) {
            d_ptr->dropIndex = nextIndex;
            update();
        }
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }
    if (event != nullptr) {
        event->ignore();
    }
}

void ZzTabBar::dropEvent(QDropEvent *event)
{
    const ZzTabMimeData *payload = event == nullptr
        ? nullptr
        : d_ptr->validPayload(event->mimeData());
    const int targetIndex = d_ptr->dropIndex >= 0
        ? d_ptr->dropIndex
        : (event == nullptr
               ? -1
               : d_ptr->insertionIndex(event->position().toPoint()));
    d_ptr->dropIndex = -1;
    update();

    if (event == nullptr || payload == nullptr || d_ptr->host.isNull()
        || payload->source.isNull() || payload->page.isNull()) {
        if (event != nullptr) {
            event->ignore();
        }
        return;
    }

    const int currentSourceIndex = payload->source->indexOf(payload->page);
    if (currentSourceIndex < 0
        || !payload->source->transferTabTo(
            d_ptr->host,
            currentSourceIndex,
            targetIndex)) {
        event->ignore();
        return;
    }

    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void ZzTabBar::dragLeaveEvent(QDragLeaveEvent *event)
{
    d_ptr->dropIndex = -1;
    update();
    if (event != nullptr) {
        event->accept();
    }
}

void ZzTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);
    const QRect indicator = d_ptr->insertionIndicatorRect();
    if (indicator.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().highlight());
    painter.drawRect(indicator);
}

} // namespace ZzFluentUI
