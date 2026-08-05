#include <QtCore/QDate>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
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
    ZzFluentUI::ZzScrollArea scrollArea;
    ZzFluentUI::ZzSpinBox integerInput;
    ZzFluentUI::ZzDoubleSpinBox floatingInput;
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
    ZzFluentUI::ZzScrollBar *horizontalScrollBar =
        scrollArea.fluentHorizontalScrollBar();
    ZzFluentUI::ZzScrollBar *verticalScrollBar =
        scrollArea.fluentVerticalScrollBar();
    if (horizontalScrollBar == nullptr || verticalScrollBar == nullptr) {
        return 1;
    }
    horizontalScrollBar->setRange(10, 110);
    horizontalScrollBar->setValue(60);
    verticalScrollBar->setRange(20, 220);
    verticalScrollBar->setPageStep(40);
    verticalScrollBar->setValue(120);
    integerInput.setRange(-20, 80);
    integerInput.setValue(24);
    floatingInput.setRange(-10.0, 10.0);
    floatingInput.setDecimals(2);
    floatingInput.setValue(1.25);

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
        || horizontalScrollBar->orientation() != Qt::Horizontal
        || horizontalScrollBar->minimum() != 10
        || horizontalScrollBar->maximum() != 110
        || horizontalScrollBar->value() != 60
        || verticalScrollBar->orientation() != Qt::Vertical
        || verticalScrollBar->minimum() != 20
        || verticalScrollBar->maximum() != 220
        || verticalScrollBar->pageStep() != 40
        || verticalScrollBar->value() != 120
        || integerInput.minimum() != -20
        || integerInput.maximum() != 80
        || integerInput.value() != 24
        || integerInput.buttonSymbols() != QAbstractSpinBox::PlusMinus
        || floatingInput.minimum() != -10.0
        || floatingInput.maximum() != 10.0
        || floatingInput.decimals() != 2
        || floatingInput.value() != 1.25
        || floatingInput.buttonSymbols() != QAbstractSpinBox::PlusMinus
        || sourceTabs.fluentTabBar() == nullptr
        || !sourceTabs.transferTabTo(&targetTabs, 0)
        || sourceTabs.count() != 0
        || targetTabs.widget(0) != tabPage) {
        return 1;
    }
    return 0;
}
