#include "ZzSidePanePrivate.h"

#include <algorithm>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzSidePane.h>

namespace ZzFluentUI {

namespace {

constexpr int zzSidePaneHandleWidth = 4;

} // namespace

ZzSidePanePrivate::ZzSidePanePrivate(
    ZzSidePane *publicObject,
    ZzSidePaneEdge initialEdge)
    : q_ptr(publicObject)
    , edge(initialEdge)
{
    Q_ASSERT(q_ptr != nullptr);
    contentHost = new QWidget(q_ptr);
    titleLabel = new QLabel(contentHost);
    titleLabel->setObjectName(QStringLiteral("zzSidePaneTitleLabel"));
    titleLabel->setAccessibleName(ZzSidePane::tr("侧面板标题"));
    stack = new QStackedWidget(contentHost);
    stack->setObjectName(QStringLiteral("zzSidePanePageStack"));
    resizeHandle = new QWidget(q_ptr);
    resizeHandle->setObjectName(QStringLiteral("zzSidePaneResizeHandle"));
    resizeHandle->setFixedWidth(zzSidePaneHandleWidth);
    resizeHandle->setCursor(Qt::SizeHorCursor);
    resizeHandle->installEventFilter(q_ptr);

    auto *contentLayout = new QVBoxLayout(contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(titleLabel);
    contentLayout->addWidget(stack, 1);

    auto *layout = new QHBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QObject::connect(
        stack,
        &QStackedWidget::currentChanged,
        q_ptr,
        [this](int) { syncCurrentWidget(); });
    setEdge(edge);
    applyExpandedWidth();
}

ZzSidePanePrivate::~ZzSidePanePrivate()
{
    for (const QMetaObject::Connection &connection
         : pageDestroyedConnections) {
        QObject::disconnect(connection);
    }
    pageDestroyedConnections.clear();
}

void ZzSidePanePrivate::setEdge(ZzSidePaneEdge newEdge)
{
    edge = newEdge;
    auto *layout = qobject_cast<QHBoxLayout *>(q_ptr->layout());
    Q_ASSERT(layout != nullptr);
    while (layout->count() > 0) {
        delete layout->takeAt(0);
    }
    if (edge == ZzSidePaneEdge::Left) {
        layout->addWidget(contentHost, 1);
        layout->addWidget(resizeHandle);
    } else {
        layout->addWidget(resizeHandle);
        layout->addWidget(contentHost, 1);
    }
}

bool ZzSidePanePrivate::addWidget(QWidget *widget, const QString &title)
{
    if (widget == nullptr) {
        return false;
    }
    const int currentIndex = stack->indexOf(widget);
    if (currentIndex >= 0) {
        pageTitles.insert(widget, title);
        stack->setCurrentIndex(currentIndex);
        return true;
    }
    if (widget->parent() != nullptr) {
        return false;
    }
    pageTitles.insert(widget, title);
    const int index = stack->addWidget(widget);
    pageDestroyedConnections.insert(
        widget,
        QObject::connect(widget, &QObject::destroyed, q_ptr, [this, widget] {
            pageTitles.remove(widget);
            pageDestroyedConnections.remove(widget);
            syncCurrentWidget();
    }));
    stack->setCurrentIndex(index);
    return true;
}

QWidget *ZzSidePanePrivate::takeWidget(QWidget *widget)
{
    if (widget == nullptr || stack->indexOf(widget) < 0) {
        return nullptr;
    }
    QObject::disconnect(pageDestroyedConnections.take(widget));
    pageTitles.remove(widget);
    stack->removeWidget(widget);
    widget->setParent(nullptr);
    return widget;
}

bool ZzSidePanePrivate::setCurrentWidget(QWidget *widget)
{
    const int index = widget != nullptr ? stack->indexOf(widget) : -1;
    if (index < 0) {
        return false;
    }
    stack->setCurrentIndex(index);
    return true;
}

void ZzSidePanePrivate::syncCurrentWidget()
{
    QWidget *const current = stack->currentWidget();
    titleLabel->setText(pageTitles.value(current));
    if (lastNotifiedCurrent.data() == current) {
        return;
    }
    lastNotifiedCurrent = current;
    Q_EMIT q_ptr->currentWidgetChanged(current);
}

int ZzSidePanePrivate::clampWidth(int width) const noexcept
{
    return std::clamp(width, minimumWidth, maximumWidth);
}

void ZzSidePanePrivate::applyExpandedWidth()
{
    q_ptr->setFixedWidth(expandedWidth);
}

bool ZzSidePanePrivate::handleResizeDrag(
    int globalX,
    bool begin,
    bool finish)
{
    if (begin) {
        resizing = true;
        resizeStartGlobalX = globalX;
        resizeStartWidth = expandedWidth;
        return true;
    }
    if (!resizing) {
        return false;
    }
    const int horizontalDelta = globalX - resizeStartGlobalX;
    const int directionalDelta = edge == ZzSidePaneEdge::Left
        ? horizontalDelta : -horizontalDelta;
    q_ptr->setPaneWidth(resizeStartWidth + directionalDelta);
    if (finish) {
        resizing = false;
    }
    return true;
}

} // namespace ZzFluentUI
