#include <ZzFluentUI/ZzCalendarPicker.h>

#include <ZzFluentUI/ZzCalendar.h>

#include "private/ZzCalendarPickerPrivate.h"

namespace ZzFluentUI {

ZzCalendarPicker::ZzCalendarPicker(QWidget *parent)
    : QDateEdit(parent)
    , d_ptr(std::make_unique<ZzCalendarPickerPrivate>(this))
{
}

ZzCalendarPicker::~ZzCalendarPicker() = default;

ZzCalendar *ZzCalendarPicker::calendar() const noexcept
{
    return d_ptr->calendar;
}

} // namespace ZzFluentUI
