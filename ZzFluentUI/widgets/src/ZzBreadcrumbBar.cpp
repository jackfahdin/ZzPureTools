#include <ZzFluentUI/ZzBreadcrumbBar.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QToolButton>

#include "private/ZzBreadcrumbBarPrivate.h"

namespace ZzFluentUI {

ZzBreadcrumbBar::ZzBreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    d_ptr = std::make_unique<ZzBreadcrumbBarPrivate>(this);
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
    if (d_ptr && (event->type() == QEvent::LayoutDirectionChange
        || event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange || event->type() == QEvent::LanguageChange
        || event->type() == QEvent::EnabledChange)) {
        d_ptr->updateAppearance();
        d_ptr->updateCurrentState();
    }
}

void ZzBreadcrumbBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (d_ptr) {
        d_ptr->updateOverflow();
    }
}

bool ZzBreadcrumbBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    auto *button = qobject_cast<QToolButton *>(watched);
    QList<QToolButton *> visible;
    if (!d_ptr->overflow->isHidden()) {
        visible.append(d_ptr->overflow);
    }
    for (auto *item : d_ptr->buttons) {
        if (!item->isHidden()) {
            visible.append(item);
        }
    }
    const qsizetype index = visible.indexOf(button);
    if (index < 0) {
        return QWidget::eventFilter(watched, event);
    }
    const auto *key = static_cast<QKeyEvent *>(event);
    qsizetype target = index;
    switch (key->key()) {
    case Qt::Key_Home: target = 0; break;
    case Qt::Key_End: target = visible.size() - 1; break;
    case Qt::Key_Left: target += isRightToLeft() ? 1 : -1; break;
    case Qt::Key_Right: target += isRightToLeft() ? -1 : 1; break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (button == d_ptr->overflow) {
            button->showMenu();
        } else {
            button->click();
        }
        // Navigation receivers may rebuild or destroy this bar.
        return true;
    default: return QWidget::eventFilter(watched, event);
    }
    target = qBound(qsizetype(0), target, visible.size() - 1);
    visible.at(target)->setFocus(Qt::OtherFocusReason);
    return true;
}

} // namespace ZzFluentUI
