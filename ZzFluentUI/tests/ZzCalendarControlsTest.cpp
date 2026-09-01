#include <algorithm>
#include <cmath>

#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QTableView>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>

namespace {

QDate dateForIndex(const ZzFluentUI::ZzCalendar &calendar,
    const QTableView *view,
    const QModelIndex &index)
{
    Q_ASSERT(view != nullptr);
    Q_ASSERT(index.isValid());
    const QModelIndex selected = view->selectionModel()
        ->selectedIndexes().value(0);
    if (view == nullptr || !index.isValid() || !selected.isValid()) {
        return {};
    }
    Q_ASSERT(view->model() != nullptr);
    Q_ASSERT(view->model()->columnCount() == 7);
    Q_ASSERT(index.row() >= 0 && index.row() < view->model()->rowCount());
    Q_ASSERT(index.column() >= 0 && index.column() < 7);
    Q_ASSERT(view->visualRect(index).isValid());
    const QDate derived = calendar.selectedDate().addDays(
        (index.row() - selected.row()) * 7 + index.column() - selected.column());
    Q_ASSERT(derived.isValid());
    return derived;
}

QModelIndex indexForDate(const ZzFluentUI::ZzCalendar &calendar,
    const QTableView *view,
    const QDate &date)
{
    Q_ASSERT(view != nullptr);
    Q_ASSERT(date.isValid());
    const QAbstractItemModel *model = view->model();
    if (view == nullptr || model == nullptr || !date.isValid()) {
        return {};
    }
    Q_ASSERT(model->columnCount() == 7);
    QModelIndex match;
    for (int row = 0; row < model->rowCount(); ++row) {
        for (int column = 0; column < model->columnCount(); ++column) {
            const QModelIndex index = model->index(row, column);
            if (dateForIndex(calendar, view, index) == date) {
                Q_ASSERT(view->visualRect(index).isValid());
                Q_ASSERT(!match.isValid());
                match = index;
            }
        }
    }
    return match;
}

}

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
        QVERIFY(focusTarget == &calendar || calendar.isAncestorOf(focusTarget));
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

    void rtlVisualRectSelectsSameDateAsLtr()
    {
        const QDate target(2026, 8, 10);
        QImage ltrImage;
        QImage rtlImage;
        QDate ltrSelected;
        QDate rtlSelected;
        for (const Qt::LayoutDirection direction : {
                 Qt::LeftToRight, Qt::RightToLeft}) {
            ZzFluentUI::ZzCalendar calendar;
            calendar.setLocale(QLocale::c());
            calendar.setCurrentPage(2026, 8);
            calendar.setLayoutDirection(direction);
            calendar.setSelectedDate(QDate(2026, 8, 5));
            calendar.resize(420, 320);
            calendar.show();
            QCoreApplication::processEvents();
            auto *view = calendar.findChild<QTableView *>();
            QVERIFY(view != nullptr);
            if (view == nullptr) {
                return;
            }
            const QModelIndex index = indexForDate(calendar, view, target);
            QVERIFY(index.isValid());
            QVERIFY(!view->visualRect(index).isEmpty());
            QTest::mouseClick(view->viewport(), Qt::LeftButton,
                Qt::NoModifier, view->visualRect(index).center());
            QCoreApplication::processEvents();
            QCOMPARE(calendar.selectedDate(), target);
            QImage image(calendar.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            calendar.render(&painter);
            QVERIFY(!image.isNull() && !image.size().isEmpty());
            int renderedPixels = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    renderedPixels += image.pixelColor(x, y).alpha() > 0;
                }
            }
            QVERIFY(renderedPixels > 0);
            if (direction == Qt::LeftToRight) {
                ltrSelected = calendar.selectedDate();
                ltrImage = image;
            } else {
                rtlSelected = calendar.selectedDate();
                rtlImage = image;
            }
        }
        QCOMPARE(ltrSelected, rtlSelected);
        QVERIFY(!ltrImage.isNull());
        QVERIFY(!rtlImage.isNull());
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
        calendar->setSelectedDate(selected.addDays(1));
        QCOMPARE(picker.date(), selected.addDays(1));
    }

    void clampsRangesAndEmitsOnlyNativeDateSignals()
    {
        const QDate minimum(2026, 1, 1);
        const QDate maximum(2026, 12, 31);
        const QDate selected(2026, 8, 5);
        ZzFluentUI::ZzCalendar calendar;
        calendar.setDateRange(minimum, maximum);
        calendar.setSelectedDate(minimum.addDays(-1));
        QCOMPARE(calendar.selectedDate(), minimum);
        calendar.setSelectedDate(maximum.addDays(1));
        QCOMPARE(calendar.selectedDate(), maximum);
        calendar.setSelectedDate(selected);
        QSignalSpy calendarSpy(
            &calendar,
            &QCalendarWidget::selectionChanged);

        calendar.setSelectedDate(selected);
        QCOMPARE(calendarSpy.count(), 0);
        calendar.setSelectedDate(selected.addDays(1));
        QCOMPARE(calendarSpy.count(), 1);
        calendar.setSelectedDate(selected.addDays(1));
        QCOMPARE(calendarSpy.count(), 1);

        ZzFluentUI::ZzCalendarPicker picker;
        picker.setDateRange(minimum, maximum);
        picker.setDate(selected);
        QSignalSpy pickerSpy(&picker, &QDateEdit::dateChanged);
        picker.setDate(selected);
        QCOMPARE(pickerSpy.count(), 0);
        picker.setDate(selected.addDays(1));
        QCOMPARE(pickerSpy.count(), 1);
        picker.setDate(selected.addDays(1));
        QCOMPARE(pickerSpy.count(), 1);
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

    void hoveringDateChangesOnlyItsLocalSurface()
    {
        ZzFluentUI::ZzCalendar calendar;
        calendar.setLocale(QLocale::c());
        calendar.setCurrentPage(2026, 8);
        calendar.setSelectedDate(QDate(2026, 8, 5));
        calendar.resize(420, 320);
        calendar.show();
        QCoreApplication::processEvents();

        auto *view = calendar.findChild<QTableView *>();
        QVERIFY(view != nullptr);
        if (view == nullptr) {
            return;
        }
        const QModelIndex selected = view->selectionModel()
            ->selectedIndexes().value(0);
        QVERIFY(selected.isValid());
        const QModelIndex hoverIndex = view->model()->index(
            selected.row(), (selected.column() + 1) % view->model()->columnCount());
        const QRect cell = view->visualRect(hoverIndex);
        QVERIFY(cell.isValid());

        auto render = [&calendar] {
            QImage image(calendar.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            calendar.render(&painter);
            return image;
        };
        QWidget parking;
        parking.resize(8, 8);
        parking.move(900, 500);
        parking.show();
        QTest::mouseMove(&parking, parking.rect().center());
        QCoreApplication::processEvents();
        const QImage baseline = render();
        QTest::mouseMove(view->viewport(), cell.center());
        QCoreApplication::processEvents();
        const QImage after = render();

        const QPoint topLeft = view->viewport()->mapTo(&calendar, cell.topLeft());
        int changedPixels = 0;
        int changedOutside = 0;
        const QPoint cellOrigin = view->viewport()->mapTo(&calendar, cell.topLeft());
        const QRect allowed(cellOrigin, cell.size());
        const QRect expanded = allowed.adjusted(-1, -1, 1, 1)
            .intersected(calendar.rect());
        for (int y = 0; y < after.height(); ++y) {
            for (int x = 0; x < after.width(); ++x) {
                if (!expanded.contains(x, y)
                    && baseline.pixelColor(x, y) != after.pixelColor(x, y)) {
                    ++changedOutside;
                }
            }
        }
        for (int y = 0; y < cell.height(); ++y) {
            for (int x = 0; x < cell.width(); ++x) {
                changedPixels += baseline.pixelColor(topLeft.x() + x, topLeft.y() + y)
                    != after.pixelColor(topLeft.x() + x, topLeft.y() + y);
            }
        }
        QVERIFY(changedPixels > 0);
        QCOMPARE(changedOutside, 0);
        QTest::mouseMove(view->viewport(), QPoint(-10, -10));
    }

    void weakensOutOfRangeAndAdjacentDateText()
    {
        ZzFluentUI::ZzCalendar calendar;
        QPalette palette = calendar.palette();
        palette.setColor(QPalette::Active, QPalette::Base, Qt::white);
        palette.setColor(QPalette::Inactive, QPalette::Base, Qt::white);
        palette.setColor(QPalette::Active, QPalette::Window, Qt::white);
        palette.setColor(QPalette::Inactive, QPalette::Window, Qt::white);
        palette.setColor(QPalette::Active, QPalette::AlternateBase, Qt::white);
        palette.setColor(QPalette::Inactive, QPalette::AlternateBase, Qt::white);
        palette.setColor(QPalette::Active, QPalette::Text, QColor(0, 0, 0));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0, 0, 0));
        palette.setColor(QPalette::Active, QPalette::HighlightedText, QColor(255, 0, 0));
        calendar.setPalette(palette);
        calendar.setLocale(QLocale::c());
        calendar.setCurrentPage(2026, 8);
        calendar.setDateRange(QDate(2026, 8, 5), QDate(2026, 8, 31));
        calendar.setSelectedDate(QDate(2026, 8, 5));
        calendar.resize(420, 320);
        calendar.show();
        QCoreApplication::processEvents();
        auto *view = calendar.findChild<QTableView *>();
        QVERIFY(view != nullptr);
        if (view == nullptr) {
            return;
        }
        const QModelIndex selected = view->selectionModel()->selectedIndexes().value(0);
        QVERIFY(selected.isValid());
        QModelIndex adjacent;
        QDate adjacentDate;
        for (int row = 0; row < view->model()->rowCount() && !adjacent.isValid(); ++row) {
            for (int column = 0; column < view->model()->columnCount(); ++column) {
                const QModelIndex candidate = view->model()->index(row, column);
                const QDate candidateDate = dateForIndex(calendar, view, candidate);
                if (candidateDate.isValid()
                    && view->visualRect(candidate).top() > 40
                    && (candidateDate.year() != calendar.yearShown()
                        || candidateDate.month() != calendar.monthShown())) {
                    adjacent = candidate;
                    adjacentDate = candidateDate;
                    break;
                }
            }
        }
        const QModelIndex available = indexForDate(calendar, view, QDate(2026, 8, 12));
        const QModelIndex disabled = indexForDate(calendar, view, QDate(2026, 8, 1));
        QVERIFY(adjacent.isValid());
        QVERIFY(adjacentDate.isValid());
        QVERIFY(adjacentDate.year() != calendar.yearShown()
            || adjacentDate.month() != calendar.monthShown());
        QVERIFY(available.isValid());
        QVERIFY(disabled.isValid());
        QCOMPARE(dateForIndex(calendar, view, adjacent), adjacentDate);
        QCOMPARE(dateForIndex(calendar, view, available), QDate(2026, 8, 12));
        const QDate disabledDate = dateForIndex(calendar, view, disabled);
        QVERIFY(disabledDate < calendar.minimumDate()
            || disabledDate > calendar.maximumDate());
        auto render = [&calendar] {
            QImage image(calendar.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);
            QPainter painter(&image);
            calendar.render(&painter);
            return image;
        };
        const QImage image = render();
        auto inkStrength = [&](const QModelIndex &index) {
            const QRect local = view->visualRect(index);
            const QPoint origin = view->viewport()->mapTo(&calendar, local.topLeft());
            qint64 strength = 0;
            for (int y = 4; y < local.height() - 4; ++y) {
                for (int x = 4; x < local.width() - 4; ++x) {
                    strength += 255 - image.pixelColor(origin.x() + x,
                        origin.y() + y).value();
                }
            }
            return strength;
        };
        const qint64 adjacentStrength = inkStrength(adjacent);
        const qint64 availableStrength = inkStrength(available);
        const qint64 disabledStrength = inkStrength(disabled);
        QVERIFY(adjacentStrength < availableStrength);
        QVERIFY(disabledStrength < availableStrength);

        int highlightedInk = 0;
        const QRect disabledRect = view->visualRect(disabled);
        const QPoint disabledOrigin = view->viewport()->mapTo(&calendar,
            disabledRect.topLeft());
        for (int y = 4; y < disabledRect.height() - 4; ++y) {
            for (int x = 4; x < disabledRect.width() - 4; ++x) {
                const QColor color = image.pixelColor(
                    disabledOrigin.x() + x, disabledOrigin.y() + y);
                highlightedInk += color.red() > 180
                    && color.green() < 120 && color.blue() < 120;
            }
        }
        QCOMPARE(highlightedInk, 0);
    }

    void todayUnselectedUsesCircularEdge()
    {
        const QDate today = QDate::currentDate();
        ZzFluentUI::ZzCalendar calendar;
        QPalette palette = calendar.palette();
        palette.setColor(QPalette::Active, QPalette::Highlight, QColor(0, 180, 0));
        palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(0, 180, 0));
        palette.setColor(QPalette::Active, QPalette::Base, Qt::white);
        palette.setColor(QPalette::Inactive, QPalette::Base, Qt::white);
        calendar.setPalette(palette);
        calendar.setLocale(QLocale::c());
        calendar.setCurrentPage(today.year(), today.month());
        const QDate other = today.addDays(today.day() == 1 ? 1 : -1);
        calendar.setSelectedDate(other);
        calendar.resize(420, 320);
        calendar.show();
        QCoreApplication::processEvents();
        auto *view = calendar.findChild<QTableView *>();
        QVERIFY(view != nullptr);
        if (view == nullptr) {
            return;
        }
        const QModelIndex todayIndex = indexForDate(calendar, view, today);
        QVERIFY(todayIndex.isValid());
        const QRect local = view->visualRect(todayIndex);
        const QRect cell = local;
        const QPoint origin = view->viewport()->mapTo(&calendar, local.topLeft());
        QImage image(calendar.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        calendar.render(&painter);
        const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
        auto isHighlight = [&](int x, int y) {
            const QColor color = image.pixelColor(origin.x() + x, origin.y() + y);
            return std::abs(color.red() - highlight.red()) < 70
                && std::abs(color.green() - highlight.green()) < 70
                && std::abs(color.blue() - highlight.blue()) < 70;
        };
        const QPoint center(cell.width() / 2, cell.height() / 2);
        const int radius = std::min(cell.width(), cell.height()) / 2 - 2;
        QVERIFY(radius > 4);
        int circumferenceHits = 0;
        int interiorHits = 0;
        for (int y = 0; y < cell.height(); ++y) {
            for (int x = 0; x < cell.width(); ++x) {
                if (!isHighlight(x, y)) {
                    continue;
                }
                const qreal distance = std::hypot(
                    static_cast<qreal>(x - center.x()),
                    static_cast<qreal>(y - center.y()));
                if (distance >= radius - 4 && distance <= radius + 2) {
                    ++circumferenceHits;
                } else if (distance <= radius / 2) {
                    ++interiorHits;
                }
            }
        }
        const int cornerHits = isHighlight(center.x() + radius, center.y() + radius)
            + isHighlight(center.x() - radius, center.y() + radius)
            + isHighlight(center.x() + radius, center.y() - radius)
            + isHighlight(center.x() - radius, center.y() - radius);
        QVERIFY(circumferenceHits >= 12);
        QVERIFY(circumferenceHits > interiorHits * 2);
        QCOMPARE(cornerHits, 0);
    }

    void selectedDateUsesCircularFill()
    {
        ZzFluentUI::ZzCalendar calendar;
        QPalette palette = calendar.palette();
        palette.setColor(QPalette::Active, QPalette::Highlight, QColor(0, 180, 0));
        palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(0, 180, 0));
        palette.setColor(QPalette::Active, QPalette::Base, Qt::white);
        palette.setColor(QPalette::Inactive, QPalette::Base, Qt::white);
        calendar.setPalette(palette);
        calendar.setLocale(QLocale::c());
        calendar.setCurrentPage(2026, 8);
        calendar.setSelectedDate(QDate(2026, 8, 5));
        calendar.resize(420, 320);
        calendar.show();
        QCoreApplication::processEvents();
        auto *view = calendar.findChild<QTableView *>();
        QVERIFY(view != nullptr);
        if (view == nullptr) {
            return;
        }
        const QModelIndex selected = indexForDate(calendar, view, calendar.selectedDate());
        QVERIFY(selected.isValid());
        const QRect cell = view->visualRect(selected);
        QVERIFY(!cell.isEmpty());
        view->setSelectionMode(QAbstractItemView::NoSelection);
        view->setStyleSheet(QStringLiteral("QTableView::item:selected { background: transparent; }"));
        const QPoint origin = view->viewport()->mapTo(&calendar, cell.topLeft());
        QImage image(calendar.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        calendar.render(&painter);
        const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
        auto isHighlight = [&](int x, int y) {
            const QColor color = image.pixelColor(origin.x() + x, origin.y() + y);
            return std::abs(color.red() - highlight.red()) < 80
                && std::abs(color.green() - highlight.green()) < 80
                && std::abs(color.blue() - highlight.blue()) < 80;
        };
        const QPoint center(cell.width() / 2, cell.height() / 2);
        const int radius = std::min(
            std::min(cell.width(), cell.height()) / 2 - 4,
            16) - 3;
        QVERIFY(radius > 4);
        const int centerHits = isHighlight(center.x(), center.y() - radius / 2)
            + isHighlight(center.x(), center.y() + radius / 2)
            + isHighlight(center.x() - radius / 2, center.y());
        const int edgeHits = isHighlight(center.x() + radius - 1, center.y())
            + isHighlight(center.x() - radius + 1, center.y())
            + isHighlight(center.x(), center.y() + radius - 1)
            + isHighlight(center.x(), center.y() - radius + 1);
        const int cornerHits = isHighlight(center.x() + radius, center.y() + radius)
            + isHighlight(center.x() - radius, center.y() + radius)
            + isHighlight(center.x() + radius, center.y() - radius)
            + isHighlight(center.x() - radius, center.y() - radius);
        QVERIFY(centerHits >= 2);
        QVERIFY(edgeHits >= 3);
        QCOMPARE(cornerHits, 0);
    }
};

QTEST_MAIN(ZzCalendarControlsTest)

#include "ZzCalendarControlsTest.moc"
