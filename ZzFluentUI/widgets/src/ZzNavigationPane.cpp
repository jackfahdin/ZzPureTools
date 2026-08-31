#include <ZzFluentUI/ZzNavigationPane.h>

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEvent>

#include "private/ZzNavigationPanePrivate.h"

namespace ZzFluentUI {

namespace {

[[nodiscard]] ZzNavigationDisplayMode zzNormalizedDisplayMode(
    ZzNavigationDisplayMode mode)
{
    switch (mode) {
    case ZzNavigationDisplayMode::Regular:
    case ZzNavigationDisplayMode::Compact:
    case ZzNavigationDisplayMode::Adaptive:
        return mode;
    }
    return ZzNavigationDisplayMode::Adaptive;
}

} // namespace

ZzNavigationPane::ZzNavigationPane(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzNavigationPanePrivate>(this))
{
}

ZzNavigationPane::~ZzNavigationPane() = default;

void ZzNavigationPane::setModel(QAbstractItemModel *model)
{
    d_ptr->setModel(model);
}

QAbstractItemModel *ZzNavigationPane::model() const noexcept
{
    return d_ptr->sourceModel.data();
}

void ZzNavigationPane::setDisplayMode(ZzNavigationDisplayMode mode)
{
    const auto normalized = zzNormalizedDisplayMode(mode);
    if (d_ptr->displayMode == normalized) {
        return;
    }
    d_ptr->displayMode = normalized;
    d_ptr->syncDisplayMode();
    Q_EMIT displayModeChanged(normalized);
}

ZzNavigationDisplayMode ZzNavigationPane::displayMode() const noexcept
{
    return d_ptr->displayMode;
}

bool ZzNavigationPane::isCompact() const noexcept
{
    return d_ptr->compact;
}

void ZzNavigationPane::setAdaptiveThreshold(int logicalWidth)
{
    const int normalized = std::clamp(logicalWidth, 480, 4096);
    if (d_ptr->adaptiveThreshold == normalized) {
        return;
    }
    d_ptr->adaptiveThreshold = normalized;
    d_ptr->syncDisplayMode();
    Q_EMIT adaptiveThresholdChanged(normalized);
}

int ZzNavigationPane::adaptiveThreshold() const noexcept
{
    return d_ptr->adaptiveThreshold;
}

void ZzNavigationPane::setCurrentSourceIndex(const QModelIndex &index)
{
    d_ptr->setCurrentSourceIndex(index);
}

QModelIndex ZzNavigationPane::currentSourceIndex() const
{
    return d_ptr->currentSourceIndex;
}

void ZzNavigationPane::setTreeMode(bool enabled)
{
    d_ptr->setTreeMode(enabled);
}

bool ZzNavigationPane::isTreeMode() const noexcept
{
    return d_ptr->treeMode;
}

QTreeView *ZzNavigationPane::treeView() const noexcept
{
    return d_ptr->treeView;
}

bool ZzNavigationPane::event(QEvent *event)
{
    const bool handled = QWidget::event(event);
    if (event != nullptr
        && (event->type() == QEvent::ParentChange
            || event->type() == QEvent::Show
            || event->type() == QEvent::ShowToParent)) {
        d_ptr->rebindAdaptiveWindow();
    }
    return handled;
}

bool ZzNavigationPane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d_ptr->adaptiveWindow.data() && event != nullptr
        && (event->type() == QEvent::Resize
            || event->type() == QEvent::Show
            || event->type() == QEvent::WindowStateChange)) {
        d_ptr->syncDisplayMode();
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ZzFluentUI
