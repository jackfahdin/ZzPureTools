#include <ZzFluentUI/ZzCalendar.h>

#include <QtCore/QEvent>
#include <QtGui/QPainter>

#include "private/ZzCalendarPrivate.h"

namespace ZzFluentUI {

ZzCalendar::ZzCalendar(QWidget *parent)
    : QCalendarWidget(parent)
    , d_ptr(std::make_unique<ZzCalendarPrivate>(this))
{
    setGridVisible(false);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setSelectionMode(QCalendarWidget::SingleSelection);
    setFocusPolicy(Qt::StrongFocus);
    connect(
        this,
        &QCalendarWidget::currentPageChanged,
        this,
        [this] {
            d_ptr->clearHover();
            updateCells();
        });
}

ZzCalendar::~ZzCalendar() = default;

void ZzCalendar::paintCell(
    QPainter *painter,
    const QRect &rect,
    QDate date) const
{
    d_ptr->paintCell(painter, rect, date);
}

void ZzCalendar::changeEvent(QEvent *event)
{
    QCalendarWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
        updateGeometry();
        updateCells();
        break;
    case QEvent::ApplicationPaletteChange:
    case QEvent::EnabledChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::LocaleChange:
    case QEvent::PaletteChange:
        d_ptr->clearHover();
        updateCells();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
