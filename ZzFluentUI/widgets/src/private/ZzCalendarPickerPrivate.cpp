#include "ZzCalendarPickerPrivate.h"

#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtWidgets/QLineEdit>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>

namespace ZzFluentUI {

ZzCalendarPickerPrivate::ZzCalendarPickerPrivate(ZzCalendarPicker *q)
    : q_ptr(q)
    , calendar(new ZzCalendar(q))
{
    Q_ASSERT(q_ptr != nullptr);
    // Use the same borderless input surface as the other Fluent spin fields;
    // QDateEdit remains the owner of all date parsing and popup semantics.
    q_ptr->setFrame(false);
    if (auto *edit = q_ptr->findChild<QLineEdit *>()) {
        edit->setFrame(false);
    }
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
