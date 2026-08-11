#include <ZzFluentUI/ZzPivot.h>

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>

#include "private/ZzPivotPrivate.h"

namespace ZzFluentUI {

ZzPivot::ZzPivot(QWidget *parent)
    : QTabBar(parent)
    , d_ptr(std::make_unique<ZzPivotPrivate>(this))
{
    setMovable(false);
    setTabsClosable(false);
    setExpanding(false);
    setShape(QTabBar::RoundedNorth);
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideRight);
    setDrawBase(false);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::StrongFocus);
}

ZzPivot::~ZzPivot() = default;

int ZzPivot::addItem(const QString &text)
{
    return addTab(text);
}

int ZzPivot::insertItem(int index, const QString &text)
{
    return insertTab(index, text);
}

void ZzPivot::removeItem(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    removeTab(index);
}

QString ZzPivot::itemText(int index) const
{
    return index >= 0 && index < count() ? tabText(index) : QString();
}

void ZzPivot::setItemText(int index, const QString &text)
{
    if (index < 0 || index >= count() || tabText(index) == text) {
        return;
    }
    setTabText(index, text);
    d_ptr->settleIndicator();
}

void ZzPivot::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr && event->modifiers() == Qt::NoModifier) {
        const bool navigateToFirst = event->key() == Qt::Key_Home;
        const bool navigateToLast = event->key() == Qt::Key_End;
        if (navigateToFirst || navigateToLast) {
            const int step = navigateToFirst ? 1 : -1;
            for (int index = navigateToFirst ? 0 : count() - 1;
                 index >= 0 && index < count();
                 index += step) {
                if (isTabVisible(index) && isTabEnabled(index)) {
                    setCurrentIndex(index);
                    break;
                }
            }
            event->accept();
            return;
        }
    }
    QTabBar::keyPressEvent(event);
}

void ZzPivot::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    if (event != nullptr) {
        painter.setClipRegion(event->region());
    }
    d_ptr->paint(&painter);
}

void ZzPivot::resizeEvent(QResizeEvent *event)
{
    QTabBar::resizeEvent(event);
    d_ptr->settleIndicator();
}

void ZzPivot::changeEvent(QEvent *event)
{
    QTabBar::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange) {
        d_ptr->refreshTheme();
        return;
    }
    if (event->type() == QEvent::FontChange
        || event->type() == QEvent::LayoutDirectionChange
        || event->type() == QEvent::DevicePixelRatioChange) {
        d_ptr->settleIndicator();
    }
}

void ZzPivot::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    d_ptr->settleIndicator();
    Q_EMIT itemCountChanged(count());
}

void ZzPivot::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    d_ptr->settleIndicator();
    Q_EMIT itemCountChanged(count());
}

} // namespace ZzFluentUI
