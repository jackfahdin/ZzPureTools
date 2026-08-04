#include <ZzFluentUI/ZzBreadcrumbBar.h>

#include <utility>

#include <QtCore/QEvent>

#include "private/ZzBreadcrumbBarPrivate.h"

namespace ZzFluentUI {

ZzBreadcrumbBar::ZzBreadcrumbBar(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzBreadcrumbBarPrivate>(this))
{
}

ZzBreadcrumbBar::~ZzBreadcrumbBar() = default;

void ZzBreadcrumbBar::setItems(QStringList items)
{
    if (d_ptr->items == items) {
        return;
    }
    d_ptr->items = std::move(items);
    if (d_ptr->currentIndex < 0
        || static_cast<qsizetype>(d_ptr->currentIndex)
            >= d_ptr->items.size()) {
        d_ptr->currentIndex = -1;
    }
    d_ptr->rebuild();
    updateGeometry();
}

QStringList ZzBreadcrumbBar::items() const
{
    return d_ptr->items;
}

void ZzBreadcrumbBar::setCurrentIndex(int index)
{
    const int bounded = index >= 0
        && static_cast<qsizetype>(index) < d_ptr->items.size()
        ? index
        : -1;
    if (d_ptr->currentIndex == bounded) {
        return;
    }
    d_ptr->currentIndex = bounded;
    d_ptr->updateCurrentState();
}

int ZzBreadcrumbBar::currentIndex() const noexcept
{
    return d_ptr->currentIndex;
}

void ZzBreadcrumbBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event != nullptr
        && event->type() == QEvent::LayoutDirectionChange) {
        d_ptr->rebuild();
    }
}

} // namespace ZzFluentUI
