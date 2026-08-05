#include <QtCore/QDate>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzFluentUI::ZzCalendar calendar;
    ZzFluentUI::ZzCalendarPicker picker;
    ZzFluentUI::ZzActionCard actionCard(
        QStringLiteral("Settings"),
        QStringLiteral("Open preferences"));
    ZzFluentUI::ZzImageCard imageCard(
        QStringLiteral("Project"),
        QStringLiteral("Open preview"));
    ZzFluentUI::ZzTabWidget sourceTabs;
    ZzFluentUI::ZzTabWidget targetTabs;
    QWidget *tabPage = new QWidget;
    sourceTabs.addTab(tabPage, QStringLiteral("Overview"));
    const QDate expectedDate(2026, 8, 5);
    calendar.setSelectedDate(expectedDate);
    picker.setDate(expectedDate);

    if (calendar.selectedDate() != expectedDate
        || picker.date() != expectedDate
        || picker.calendar() == nullptr
        || picker.calendarWidget() != picker.calendar()
        || actionCard.description() != QStringLiteral("Open preferences")
        || imageCard.description() != QStringLiteral("Open preview")
        || sourceTabs.fluentTabBar() == nullptr
        || !sourceTabs.transferTabTo(&targetTabs, 0)
        || sourceTabs.count() != 0
        || targetTabs.widget(0) != tabPage) {
        return 1;
    }
    return 0;
}
