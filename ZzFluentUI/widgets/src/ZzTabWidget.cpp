#include <ZzFluentUI/ZzTabWidget.h>

#include "private/ZzTabBarPrivate.h"
#include "private/ZzTabWidgetPrivate.h"

#include <ZzFluentUI/ZzTabBar.h>

namespace ZzFluentUI {

ZzTabWidget::ZzTabWidget(QWidget *parent)
    : QTabWidget(parent)
    , d_ptr(std::make_unique<ZzTabWidgetPrivate>(this))
{
    d_ptr->tabBar = new ZzTabBar(this);
    d_ptr->tabBar->d_ptr->setHost(this);
    setTabBar(d_ptr->tabBar);
    setMovable(true);

    connect(
        d_ptr->tabBar,
        &ZzTabBar::tearOffRequested,
        this,
        [this](int index, const QPoint &globalPosition) {
            QWidget *page = widget(index);
            if (page != nullptr) {
                Q_EMIT tearOffRequested(index, page, globalPosition);
            }
        });
}

ZzTabWidget::~ZzTabWidget() = default;

ZzTabBar *ZzTabWidget::fluentTabBar() const noexcept
{
    return d_ptr->tabBar;
}

bool ZzTabWidget::transferTabTo(
    ZzTabWidget *target,
    int sourceIndex,
    int targetIndex)
{
    return d_ptr->transferTo(target, sourceIndex, targetIndex);
}

} // namespace ZzFluentUI
