#include "ZzSidePanePrivate.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzPanelStack.h>
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
    panelStack = new ZzPanelStack(contentHost);
    panelStack->setObjectName(QStringLiteral("zzSidePanePanelStack"));
    panelStack->setHeadersVisible(false);
    resizeHandle = new QWidget(q_ptr);
    resizeHandle->setObjectName(QStringLiteral("zzSidePaneResizeHandle"));
    resizeHandle->setFixedWidth(zzSidePaneHandleWidth);
    resizeHandle->setCursor(Qt::SizeHorCursor);
    resizeHandle->installEventFilter(q_ptr);

    auto *contentLayout = new QVBoxLayout(contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(panelStack);

    auto *layout = new QHBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    currentPanelConnection = QObject::connect(
        panelStack,
        &ZzPanelStack::currentPanelChanged,
        q_ptr,
        [this](QWidget *) { syncCurrentWidget(); });
    setEdge(edge);
    applyExpandedWidth();
}

ZzSidePanePrivate::~ZzSidePanePrivate()
{
    QObject::disconnect(currentPanelConnection);
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
    if (panelStack->panels().contains(widget)) {
        return panelStack->setPanelTitle(widget, title)
            && setCurrentWidget(widget);
    }
    if (widget->parent() != nullptr) {
        return false;
    }
    QPointer<ZzSidePane> paneGuard(q_ptr);
    QPointer<QWidget> widgetGuard(widget);
    if (!panelStack->addPanel(widget, title)
        || paneGuard.isNull()
        || widgetGuard.isNull()
        || !panelStack->panels().contains(widgetGuard.data())) {
        return false;
    }
    if (mode == ZzSidePaneMode::Stacked) {
        if (!stackedVisible.contains(widgetGuard)) {
            stackedVisible.append(widgetGuard);
        }
    } else {
        stackedVisible = {widgetGuard};
    }
    return !widgetGuard.isNull() && setCurrentWidget(widgetGuard.data());
}

QWidget *ZzSidePanePrivate::takeWidget(QWidget *widget)
{
    if (widget == nullptr || !panelStack->panels().contains(widget)) {
        return nullptr;
    }
    if (mode == ZzSidePaneMode::Single
        && panelStack->currentPanel() == widget) {
        for (QWidget *const candidate : panelStack->panels()) {
            if (candidate != widget) {
                QPointer<ZzSidePane> paneGuard(q_ptr);
                panelStack->setPanelVisible(candidate, true);
                if (paneGuard.isNull()) {
                    return nullptr;
                }
                break;
            }
        }
    }
    QPointer<QWidget> widgetGuard(widget);
    QPointer<ZzSidePane> paneGuard(q_ptr);
    QWidget *const result = panelStack->takePanel(widget);
    if (paneGuard.isNull()) {
        return nullptr;
    }
    sanitizeStackedVisible();
    if (widgetGuard.isNull() || result == nullptr
        || panelStack->panels().contains(widgetGuard.data())) {
        return nullptr;
    }
    return result;
}

bool ZzSidePanePrivate::setCurrentWidget(QWidget *widget)
{
    if (widget == nullptr || !panelStack->panels().contains(widget)) {
        return false;
    }
    QPointer<ZzSidePane> paneGuard(q_ptr);
    QPointer<QWidget> widgetGuard(widget);
    if (mode == ZzSidePaneMode::Single) {
        const QList<QWidget *> pages = panelStack->panels();
        for (QWidget *const page : pages) {
            if (page != nullptr
                && !panelStack->setPanelVisible(page, page == widgetGuard.data())) {
                return false;
            }
            if (paneGuard.isNull() || widgetGuard.isNull()) {
                return false;
            }
        }
        stackedVisible = {widgetGuard};
    } else if (!panelStack->setPanelVisible(widget, true)) {
        return false;
    }
    return !paneGuard.isNull() && !widgetGuard.isNull()
        && panelStack->setCurrentPanel(widgetGuard.data());
}

bool ZzSidePanePrivate::setWidgetVisible(QWidget *widget, bool visible)
{
    if (widget == nullptr || !panelStack->panels().contains(widget)) {
        return false;
    }
    if (mode == ZzSidePaneMode::Single && visible) {
        return setCurrentWidget(widget);
    }
    QPointer<ZzSidePane> paneGuard(q_ptr);
    QPointer<QWidget> widgetGuard(widget);
    if (!panelStack->setPanelVisible(widget, visible)
        || paneGuard.isNull() || widgetGuard.isNull()) {
        return false;
    }
    if (mode == ZzSidePaneMode::Stacked) {
        sanitizeStackedVisible();
        if (visible && !stackedVisible.contains(widgetGuard)) {
            stackedVisible.append(widgetGuard);
        } else if (!visible) {
            stackedVisible.removeAll(widgetGuard);
        }
    }
    return true;
}

void ZzSidePanePrivate::setMode(ZzSidePaneMode newMode)
{
    if (mode == newMode) {
        return;
    }
    if (mode == ZzSidePaneMode::Stacked) {
        stackedVisible.clear();
        for (QWidget *const widget : panelStack->visiblePanels()) {
            stackedVisible.append(widget);
        }
    }
    mode = newMode;
    QPointer<ZzSidePane> paneGuard(q_ptr);
    if (mode == ZzSidePaneMode::Single) {
        if (QWidget *const current = panelStack->currentPanel(); current != nullptr) {
            for (QWidget *const page : panelStack->panels()) {
                if (page != nullptr) {
                    panelStack->setPanelVisible(page, page == current);
                    if (paneGuard.isNull()) {
                        return;
                    }
                }
            }
            panelStack->setCurrentPanel(current);
            if (paneGuard.isNull()) {
                return;
            }
        }
    } else {
        sanitizeStackedVisible();
        for (const QPointer<QWidget> &widget : std::as_const(stackedVisible)) {
            if (widget != nullptr) {
                panelStack->setPanelVisible(widget.data(), true);
                if (paneGuard.isNull()) {
                    return;
                }
            }
        }
    }
    Q_EMIT q_ptr->modeChanged(mode);
}

void ZzSidePanePrivate::syncCurrentWidget()
{
    QWidget *const current = panelStack->currentPanel();
    sanitizeStackedVisible();
    if (lastNotifiedCurrent.data() == current) {
        return;
    }
    lastNotifiedCurrent = current;
    QPointer<ZzSidePane> paneGuard(q_ptr);
    Q_EMIT q_ptr->currentWidgetChanged(current);
    if (paneGuard.isNull()) {
        return;
    }
}

void ZzSidePanePrivate::sanitizeStackedVisible()
{
    const QList<QWidget *> panels = panelStack->panels();
    stackedVisible.erase(
        std::remove_if(
            stackedVisible.begin(),
            stackedVisible.end(),
            [&panels](const QPointer<QWidget> &widget) {
                return widget.isNull() || !panels.contains(widget.data());
            }),
        stackedVisible.end());
}

int ZzSidePanePrivate::clampWidth(int width) const noexcept
{
    return std::clamp(width, minimumWidth, maximumWidth);
}

void ZzSidePanePrivate::applyExpandedWidth()
{
    q_ptr->setMinimumWidth(minimumWidth);
    q_ptr->setMaximumWidth(expandedWidth);
    q_ptr->resize(expandedWidth, q_ptr->height());
    q_ptr->updateGeometry();
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
