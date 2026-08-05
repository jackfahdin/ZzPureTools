#include <QtCore/QDate>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzFluentUI::ZzCalendar calendar;
    ZzFluentUI::ZzCalendarPicker picker;
    const QDate expectedDate(2026, 8, 5);
    calendar.setSelectedDate(expectedDate);
    picker.setDate(expectedDate);

    if (calendar.selectedDate() != expectedDate
        || picker.date() != expectedDate
        || picker.calendar() == nullptr
        || picker.calendarWidget() != picker.calendar()) {
        return 1;
    }
    return 0;
}
