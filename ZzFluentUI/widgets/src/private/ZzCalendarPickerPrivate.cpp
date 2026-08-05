#include "ZzCalendarPickerPrivate.h"

#include <QtCore/QDate>
#include <QtCore/QLocale>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>

namespace ZzFluentUI {

ZzCalendarPickerPrivate::ZzCalendarPickerPrivate(ZzCalendarPicker *q)
    : q_ptr(q)
    , calendar(new ZzCalendar(q))
{
    Q_ASSERT(q_ptr != nullptr);
    q_ptr->setCalendarPopup(true);
    q_ptr->setCalendarWidget(calendar);
    q_ptr->setDisplayFormat(
        q_ptr->locale().dateFormat(QLocale::ShortFormat));
    q_ptr->setDate(QDate::currentDate());
    q_ptr->setAccelerated(true);
    q_ptr->setWrapping(false);
    q_ptr->setFocusPolicy(Qt::StrongFocus);
}

} // namespace ZzFluentUI
