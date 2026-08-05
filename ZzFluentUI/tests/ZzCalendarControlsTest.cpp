#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>

/** @brief 验证日历控件保留 Qt 日期语义并维持稳定绘制对象数量。 */
class ZzCalendarControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableDefaults()
    {
        ZzFluentUI::ZzCalendar calendar;

        QVERIFY(calendar.selectedDate().isValid());
        QVERIFY(!calendar.isGridVisible());
        QCOMPARE(
            calendar.verticalHeaderFormat(),
            QCalendarWidget::NoVerticalHeader);
        QCOMPARE(
            calendar.horizontalHeaderFormat(),
            QCalendarWidget::ShortDayNames);
        QCOMPARE(
            calendar.selectionMode(),
            QCalendarWidget::SingleSelection);
        QCOMPARE(calendar.focusPolicy(), Qt::StrongFocus);
    }

    void preservesDateRangeAndKeyboardNavigation()
    {
        ZzFluentUI::ZzCalendar calendar;
        const QDate minimum(2026, 1, 1);
        const QDate maximum(2026, 12, 31);
        const QDate selected(2026, 8, 5);
        calendar.setDateRange(minimum, maximum);
        calendar.setSelectedDate(selected);
        calendar.show();
        calendar.setFocus();
        QCoreApplication::processEvents();

        QCOMPARE(calendar.minimumDate(), minimum);
        QCOMPARE(calendar.maximumDate(), maximum);
        QCOMPARE(calendar.selectedDate(), selected);
        QWidget *focusTarget = QApplication::focusWidget();
        QVERIFY(focusTarget != nullptr);
        if (focusTarget == nullptr) {
            return;
        }
        QTest::keyClick(focusTarget, Qt::Key_Right);
        QCOMPARE(calendar.selectedDate(), selected.addDays(1));
        QTest::keyClick(focusTarget, Qt::Key_PageDown);
        QCOMPARE(calendar.monthShown(), 9);
        QCOMPARE(calendar.yearShown(), 2026);
    }

    void preservesLocaleAndLayoutDirection()
    {
        ZzFluentUI::ZzCalendar calendar;
        const QLocale locale(QLocale::C);
        calendar.setLocale(locale);
        calendar.setFirstDayOfWeek(Qt::Monday);
        calendar.setLayoutDirection(Qt::RightToLeft);
        calendar.setSelectedDate(QDate(2026, 8, 5));

        QCOMPARE(calendar.locale(), locale);
        QCOMPARE(calendar.firstDayOfWeek(), Qt::Monday);
        QCOMPARE(calendar.layoutDirection(), Qt::RightToLeft);
        QCOMPARE(calendar.selectedDate(), QDate(2026, 8, 5));
    }

    void pickerOwnsTypedCalendarAndSynchronizesDate()
    {
        ZzFluentUI::ZzCalendarPicker picker;
        auto *calendar = picker.calendar();
        QVERIFY(calendar != nullptr);
        if (calendar == nullptr) {
            return;
        }

        QCOMPARE(picker.calendarWidget(), calendar);
        QVERIFY(picker.calendarPopup());
        QVERIFY(picker.isAccelerated());
        QVERIFY(!picker.wrapping());
        QCOMPARE(
            picker.displayFormat(),
            picker.locale().dateFormat(QLocale::ShortFormat));

        const QDate minimum(2026, 1, 1);
        const QDate maximum(2026, 12, 31);
        const QDate selected(2026, 8, 5);
        picker.setDateRange(minimum, maximum);
        picker.setDate(selected);

        QCOMPARE(picker.minimumDate(), minimum);
        QCOMPARE(picker.maximumDate(), maximum);
        QCOMPARE(picker.date(), selected);
        QCOMPARE(calendar->selectedDate(), selected);
    }

    void repeatedRenderingDoesNotAllocateChildren()
    {
        ZzFluentUI::ZzCalendar calendar;
        calendar.setLocale(QLocale::c());
        calendar.setFirstDayOfWeek(Qt::Monday);
        calendar.setSelectedDate(QDate(2026, 8, 5));
        calendar.resize(320, 280);
        calendar.show();
        QCoreApplication::processEvents();
        const qsizetype initialChildren =
            calendar.findChildren<QObject *>().size();
        QImage image(
            calendar.size(),
            QImage::Format_ARGB32_Premultiplied);

        for (int iteration = 0; iteration < 24; ++iteration) {
            calendar.setCurrentPage(2026, (iteration % 12) + 1);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            calendar.render(&painter);
        }

        QCOMPARE(calendar.findChildren<QObject *>().size(), initialChildren);
        QVERIFY(!image.isNull());
    }
};

QTEST_MAIN(ZzCalendarControlsTest)

#include "ZzCalendarControlsTest.moc"
