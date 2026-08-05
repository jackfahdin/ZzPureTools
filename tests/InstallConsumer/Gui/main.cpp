#include <QtCore/QDate>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzProgressRing.h>
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
    ZzFluentUI::ZzProgressRing progressRing;
    ZzFluentUI::ZzProgressRing busyRing;
    ZzFluentUI::ZzTabWidget sourceTabs;
    ZzFluentUI::ZzTabWidget targetTabs;
    QWidget *tabPage = new QWidget;
    sourceTabs.addTab(tabPage, QStringLiteral("Overview"));
    const QDate expectedDate(2026, 8, 5);
    calendar.setSelectedDate(expectedDate);
    picker.setDate(expectedDate);
    progressRing.setRange(20, 120);
    progressRing.setValue(70);
    progressRing.setRingWidth(6);
    busyRing.setRange(0, 0);

    if (calendar.selectedDate() != expectedDate
        || picker.date() != expectedDate
        || picker.calendar() == nullptr
        || picker.calendarWidget() != picker.calendar()
        || actionCard.description() != QStringLiteral("Open preferences")
        || imageCard.description() != QStringLiteral("Open preview")
        || progressRing.minimum() != 20
        || progressRing.maximum() != 120
        || progressRing.value() != 70
        || progressRing.ringWidth() != 6
        || busyRing.minimum() != 0
        || busyRing.maximum() != 0
        || sourceTabs.fluentTabBar() == nullptr
        || !sourceTabs.transferTabTo(&targetTabs, 0)
        || sourceTabs.count() != 0
        || targetTabs.widget(0) != tabPage) {
        return 1;
    }
    return 0;
}
