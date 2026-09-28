#include <ZzFluentUI/ZzCalendar.h>

#include <QtCore/QEvent>
#include <QtGui/QPainter>

#include "private/ZzCalendarPrivate.h"

namespace ZzFluentUI {

ZzCalendar::ZzCalendar(QWidget *parent)
    : QCalendarWidget(parent)
{
    // 私有实现装配布局时可能同步触发 changeEvent/sizeHint；先保证 d_ptr 已为空。
    d_ptr = std::make_unique<ZzCalendarPrivate>(this);
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

QSize ZzCalendar::sizeHint() const
{
    return minimumSizeHint();
}

QSize ZzCalendar::minimumSizeHint() const
{
    return d_ptr ? d_ptr->minimumSize().expandedTo(QCalendarWidget::minimumSizeHint())
                 : QCalendarWidget::minimumSizeHint();
}

void ZzCalendar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    d_ptr->paintSurface(&painter);
}

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
    if (event == nullptr || !d_ptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
        d_ptr->refreshVisuals();
        updateGeometry();
        updateCells();
        break;
    case QEvent::ApplicationPaletteChange:
    case QEvent::EnabledChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::LocaleChange:
    case QEvent::PaletteChange:
        d_ptr->refreshVisuals();
        d_ptr->clearHover();
        updateGeometry();
        updateCells();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
