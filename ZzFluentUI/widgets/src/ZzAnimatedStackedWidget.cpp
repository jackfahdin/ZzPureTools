#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include <algorithm>
#include <QtCore/QEvent>
#include "private/ZzAnimatedStackedWidgetPrivate.h"

namespace ZzFluentUI {

ZzAnimatedStackedWidget::ZzAnimatedStackedWidget(QWidget *parent) : QWidget(parent)
{
    d_ptr = std::make_unique<ZzAnimatedStackedWidgetPrivate>(this);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("页面容器"));
}
ZzAnimatedStackedWidget::~ZzAnimatedStackedWidget() = default;
QSize ZzAnimatedStackedWidget::sizeHint() const
{
    QSize result(0, 0);
    for (const auto &page : d_ptr->pages) if (page.view) result = result.expandedTo(page.view->sizeHint());
    return result.grownBy(contentsMargins());
}
QSize ZzAnimatedStackedWidget::minimumSizeHint() const
{
    QSize result(0, 0);
    for (const auto &page : d_ptr->pages) if (page.view) {
        result = result.expandedTo(page.view->minimumSizeHint()).expandedTo(page.view->minimumSize());
    }
    return result.grownBy(contentsMargins());
}
int ZzAnimatedStackedWidget::count() const noexcept { return static_cast<int>(d_ptr->pages.size()); }
QWidget *ZzAnimatedStackedWidget::widget(int index) const noexcept
{
    return index >= 0 && index < count() ? d_ptr->pages.at(index).view.data() : nullptr;
}
int ZzAnimatedStackedWidget::indexOf(QWidget *page) const noexcept { return d_ptr->indexOf(page); }
int ZzAnimatedStackedWidget::currentIndex() const noexcept { return indexOf(d_ptr->current); }
QWidget *ZzAnimatedStackedWidget::currentWidget() const noexcept { return widget(currentIndex()); }
int ZzAnimatedStackedWidget::addWidget(QWidget *page) { return insertWidget(count(), page); }
int ZzAnimatedStackedWidget::insertWidget(int index, QWidget *page) { return d_ptr->insert(index, page); }
QWidget *ZzAnimatedStackedWidget::takeWidget(QWidget *page) { return d_ptr->remove(page, true); }
bool ZzAnimatedStackedWidget::setCurrentIndex(int index, ZzTransitionDirection direction)
{
    return setCurrentWidget(widget(index), direction);
}
bool ZzAnimatedStackedWidget::setCurrentWidget(QWidget *page, ZzTransitionDirection direction)
{
    return d_ptr->request(page, direction);
}
ZzAnimatedStackedWidget::ZzTransitionMode ZzAnimatedStackedWidget::transitionMode() const noexcept
{
    return d_ptr->mode;
}
void ZzAnimatedStackedWidget::setTransitionMode(ZzTransitionMode mode)
{
    if (mode < ZzTransitionMode::None || mode > ZzTransitionMode::Fade || d_ptr->mode == mode) return;
    d_ptr->mode = mode;
    d_ptr->settle();
}
int ZzAnimatedStackedWidget::duration() const noexcept { return d_ptr->milliseconds; }
void ZzAnimatedStackedWidget::setDuration(int milliseconds)
{
    const int value = std::clamp(milliseconds, -1, 10000);
    if (d_ptr->milliseconds == value) return;
    d_ptr->milliseconds = value;
    d_ptr->settle();
}
bool ZzAnimatedStackedWidget::isTransitionRunning() const noexcept { return d_ptr->running; }
bool ZzAnimatedStackedWidget::setPageAnimationEnabled(QWidget *page, bool enabled)
{
    const int index = indexOf(page);
    if (index < 0) return false;
    d_ptr->pages[index].animationEnabled = enabled;
    if (!enabled && (page == d_ptr->current || page == d_ptr->source)) d_ptr->settle();
    return true;
}
bool ZzAnimatedStackedWidget::isPageAnimationEnabled(QWidget *page) const noexcept
{
    const int index = indexOf(page);
    return index >= 0 && d_ptr->pages.at(index).animationEnabled;
}
bool ZzAnimatedStackedWidget::event(QEvent *event)
{
    if (d_ptr) {
        const QPointer<ZzAnimatedStackedWidget> guard(this);
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::ContentsRectChange:
        case QEvent::Hide:
        case QEvent::EnabledChange:
        case QEvent::WindowDeactivate:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::FontChange:
        case QEvent::LanguageChange:
        case QEvent::LayoutDirectionChange:
        case QEvent::DevicePixelRatioChange:
            d_ptr->settle();
            if (!guard) return true;
            d_ptr->theme.refreshFallback();
            break;
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            if (d_ptr->running) { event->accept(); return true; }
            break;
        default: break;
        }
        if (event->type() == QEvent::Resize || event->type() == QEvent::ContentsRectChange) d_ptr->synchronizePages();
        if (!guard) return true;
    }
    return QWidget::event(event);
}

} // namespace ZzFluentUI
