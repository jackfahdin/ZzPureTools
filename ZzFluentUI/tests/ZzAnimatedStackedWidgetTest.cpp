#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <functional>

#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

using ZzStack = ZzFluentUI::ZzAnimatedStackedWidget;

/** @brief 提供可按像素验证的页面，并统计实际绘制次数。 */
class ZzStackColorPage final : public QWidget
{
public:
    explicit ZzStackColorPage(QColor value) : color(value) {}
    QColor color;
    int paints = 0;
    std::function<void()> onPaint;
protected:
    void paintEvent(QPaintEvent *) override
    {
        ++paints;
        QPainter painter(this);
        painter.fillRect(rect(), color);
        const auto callback = onPaint;
        if (callback) callback();
    }
};

/** @brief 在真实 Qt 事件中重入容器，验证事务边界。 */
class ZzStackEventPage final : public QWidget
{
public:
    std::function<void(QEvent::Type)> callback;
protected:
    bool event(QEvent *event) override
    {
        const auto handler = callback;
        const bool result = QWidget::event(event);
        if (handler) handler(event->type());
        return result;
    }
};

/** @brief 验证页面容器的所有权、索引及动效契约。 */
class ZzAnimatedStackedWidgetTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void rendersSlideAndFade_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<bool>("backward");
        for (int mode = 1; mode <= 3; ++mode) for (bool rtl : {false, true}) for (bool backward : {false, true}) {
            QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(mode).arg(rtl).arg(backward))) << mode << rtl << backward;
        }
    }

    void rendersSlideAndFade()
    {
        QFETCH(int, mode);
        QFETCH(bool, rtl);
        QFETCH(bool, backward);
        ZzStack stack;
        stack.setContentsMargins(7, 9, 11, 13);
        stack.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        stack.setTransitionMode(static_cast<ZzStack::ZzTransitionMode>(mode));
        stack.setDuration(200);
        auto *red = new ZzStackColorPage(Qt::red);
        auto *blue = new ZzStackColorPage(Qt::blue);
        stack.addWidget(red);
        stack.addWidget(blue);
        stack.resize(218, 122);
        stack.show();
        QCoreApplication::processEvents();
        QSignalSpy started(&stack, &ZzStack::transitionStarted);
        QSignalSpy finished(&stack, &ZzStack::transitionFinished);
        QVERIFY(stack.setCurrentIndex(1, backward ? ZzStack::ZzTransitionDirection::Backward : ZzStack::ZzTransitionDirection::Forward));
        QCOMPARE(stack.currentWidget(), blue);
        QVERIFY(stack.isTransitionRunning());
        QCOMPARE(started.count(), 1);
        auto *animation = stack.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        animation->pause();
        animation->setCurrentTime(100);
        const int previousPaints = red->paints + blue->paints;
        const QImage frame = stack.grab().toImage();
        QCOMPARE(red->paints + blue->paints, previousPaints);
        const qreal dpr = frame.devicePixelRatio();
        const auto pixel = [&](int x, int y) { return frame.pixelColor(qRound(x * dpr), qRound(y * dpr)); };
        if (mode == 3) {
            const QColor color = pixel(100, 50);
            QVERIFY(qAbs(color.red() - 32) <= 2);
            QVERIFY(qAbs(color.blue() - 223) <= 2);
        } else {
            const bool reverse = backward != (mode == 1 && rtl);
            const QColor leading = mode == 1 ? pixel(12, 50) : pixel(100, 12);
            const QColor trailing = mode == 1 ? pixel(202, 50) : pixel(100, 105);
            QCOMPARE(leading, QColor(reverse ? Qt::blue : Qt::red));
            QCOMPARE(trailing, QColor(reverse ? Qt::red : Qt::blue));
        }
        animation->setCurrentTime(200);
        QVERIFY(!stack.isTransitionRunning());
        QCOMPARE(finished.count(), 1);
        QVERIFY(blue->isVisible());
        QVERIFY(!red->isVisible());
    }

    void coalescesLatestDestination()
    {
        ZzStack stack;
        for (auto color : {Qt::red, Qt::blue, Qt::green, Qt::yellow}) stack.addWidget(new ZzStackColorPage(color));
        stack.resize(200, 100);
        stack.show();
        QCoreApplication::processEvents();
        QSignalSpy changed(&stack, &ZzStack::currentChanged);
        stack.setCurrentIndex(1);
        QVERIFY(stack.isTransitionRunning());
        auto *animation = stack.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        animation->pause();
        stack.setCurrentIndex(2);
        stack.setCurrentIndex(3);
        QVERIFY(!stack.setCurrentIndex(99));
        QCOMPARE(stack.currentIndex(), 1);
        animation->setCurrentTime(animation->duration());
        QCOMPARE(stack.currentIndex(), 3);
        QVERIFY(stack.isTransitionRunning());
        animation->pause();
        animation->setCurrentTime(animation->duration());
        QVERIFY(!stack.isTransitionRunning());
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.at(1).at(0).toInt(), 3);
        QCOMPARE(stack.findChildren<QVariantAnimation *>().size(), 1);
    }

    void skipsSnapshotsForReducedMotionAndOptOut()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzStack stack;
        stack.setStyle(&style);
        stack.addWidget(new ZzStackColorPage(Qt::red));
        stack.addWidget(new ZzStackColorPage(Qt::blue));
        stack.resize(200, 100);
        stack.show();
        QCoreApplication::processEvents();
        controller.setReducedMotion(true);
        QSignalSpy started(&stack, &ZzStack::transitionStarted);
        stack.setCurrentIndex(1);
        QVERIFY(!stack.isTransitionRunning());
        controller.setReducedMotion(false);
        stack.setPageAnimationEnabled(stack.widget(0), false);
        stack.setCurrentIndex(0);
        QVERIFY(!stack.isTransitionRunning());
        QCOMPARE(started.count(), 0);
    }

    void settlesOnEnvironmentChange_data()
    {
        QTest::addColumn<int>("change");
        for (int i = 0; i < 8; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
    }

    void settlesOnEnvironmentChange()
    {
        QFETCH(int, change);
        ZzStack stack;
        for (auto color : {Qt::red, Qt::blue, Qt::green}) stack.addWidget(new ZzStackColorPage(color));
        stack.resize(200, 100);
        stack.show();
        QCoreApplication::processEvents();
        stack.setCurrentIndex(1);
        QVERIFY(stack.isTransitionRunning());
        stack.setCurrentIndex(2);
        QSignalSpy finished(&stack, &ZzStack::transitionFinished);
        switch (change) {
        case 0: stack.resize(220, 120); break;
        case 1: stack.hide(); break;
        case 2: stack.setEnabled(false); break;
        case 3: stack.setLayoutDirection(Qt::RightToLeft); break;
        case 4: stack.setDuration(0); break;
        case 5: stack.setTransitionMode(ZzStack::ZzTransitionMode::None); break;
        case 6: { QEvent event(QEvent::WindowDeactivate); QApplication::sendEvent(&stack, &event); break; }
        case 7: { QEvent event(QEvent::DevicePixelRatioChange); QApplication::sendEvent(&stack, &event); break; }
        }
        QVERIFY(!stack.isTransitionRunning());
        QCOMPARE(stack.currentIndex(), 2);
        QCOMPARE(finished.count(), 1);
    }

    void signalReceiverCanDeleteStack_data()
    {
        QTest::addColumn<int>("signal");
        for (int i = 0; i < 3; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
    }

    void signalReceiverCanDeleteStack()
    {
        QFETCH(int, signal);
        auto *stack = new ZzStack;
        QPointer<ZzStack> guard(stack);
        stack->addWidget(new ZzStackColorPage(Qt::red));
        stack->addWidget(new ZzStackColorPage(Qt::blue));
        stack->resize(200, 100);
        stack->show();
        QCoreApplication::processEvents();
        if (signal == 0) connect(stack, &ZzStack::currentChanged, this, [stack] { delete stack; });
        if (signal == 1) connect(stack, &ZzStack::transitionStarted, this, [stack] { delete stack; });
        if (signal == 2) connect(stack, &ZzStack::transitionFinished, this, [stack] { delete stack; });
        stack->setCurrentIndex(1);
        if (signal == 2) {
            QVERIFY(guard);
            auto *animation = stack->findChild<QVariantAnimation *>();
            QVERIFY(animation);
            animation->setCurrentTime(animation->duration());
        }
        QVERIFY(guard.isNull());
    }

    void showCallbackPreservesCurrentNotification()
    {
        ZzStack stack;
        stack.addWidget(new QWidget);
        auto *second = new ZzStackEventPage;
        stack.addWidget(second);
        QPointer<QWidget> last = new QWidget;
        stack.addWidget(last);
        stack.show();
        QCoreApplication::processEvents();
        QSignalSpy changed(&stack, &ZzStack::currentChanged);
        second->callback = [&](QEvent::Type type) {
            if (type == QEvent::Show && last) delete last.data();
        };
        stack.setCurrentWidget(second);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toInt(), 1);
        second->callback = {};
    }

    void pageGeometryChangeInvalidatesOuterLayout()
    {
        QWidget outer;
        auto *layout = new QVBoxLayout(&outer);
        auto *stack = new ZzStack(&outer);
        auto *page = new QWidget;
        page->setMinimumSize(100, 50);
        stack->addWidget(page);
        layout->addWidget(stack);
        outer.show();
        QCoreApplication::processEvents();
        QVERIFY(layout->minimumSize().width() < 400);
        page->setMinimumWidth(400);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(layout->minimumSize().width() >= 400);
    }

    void reentrantParentChangeDoesNotLoseOwnership()
    {
        ZzStack stack;
        stack.addWidget(new QWidget);
        auto *second = new QWidget;
        stack.addWidget(second);
        auto *page = new ZzStackEventPage;
        page->callback = [&](QEvent::Type type) {
            if (type == QEvent::ParentChange) stack.setCurrentWidget(second);
        };
        QCOMPARE(stack.addWidget(page), 2);
        QCOMPARE(stack.count(), 3);
        QCOMPARE(stack.currentWidget(), second);
    }

    void hideCallbackSelectsLatestPage()
    {
        ZzStack stack;
        stack.setTransitionMode(ZzStack::ZzTransitionMode::None);
        auto *first = new ZzStackEventPage;
        auto *second = new QWidget;
        auto *third = new QWidget;
        stack.addWidget(first);
        stack.addWidget(second);
        stack.addWidget(third);
        stack.resize(200, 100);
        stack.show();
        QCoreApplication::processEvents();
        first->callback = [&](QEvent::Type type) {
            if (type == QEvent::Hide) stack.setCurrentWidget(third);
        };
        QVERIFY(stack.setCurrentWidget(second));
        QCOMPARE(stack.currentWidget(), third);
        QVERIFY(third->isVisible());
        QVERIFY(!second->isVisible());
        first->callback = {};
    }

    void ownsAndReturnsPages()
    {
        ZzStack stack;
        auto *first = new QWidget;
        QPointer<QWidget> guard(first);
        QCOMPARE(stack.addWidget(first), 0);
        QCOMPARE(stack.currentWidget(), first);
        QCOMPARE(stack.addWidget(first), 0);
        QCOMPARE(stack.count(), 1);
        QCOMPARE(stack.takeWidget(first), first);
        QVERIFY(first->parentWidget() == nullptr);
        QCOMPARE(stack.currentIndex(), -1);
        delete first;
        QVERIFY(guard.isNull());
        QPointer<QWidget> owned = new QWidget;
        {
            ZzStack owner;
            QCOMPARE(owner.addWidget(owned), 0);
        }
        QVERIFY(owned.isNull());
    }

    void takeDuringHideCallbackReturnsOwnership()
    {
        ZzStack stack;
        auto *first = new ZzStackEventPage;
        stack.addWidget(first);
        stack.addWidget(new QWidget);
        auto *last = new QWidget;
        stack.addWidget(last);
        stack.show();
        QCoreApplication::processEvents();
        first->callback = [&](QEvent::Type type) {
            if (type == QEvent::Hide) stack.setCurrentWidget(last);
        };
        auto *returned = stack.takeWidget(first);
        QCOMPARE(returned, first);
        QVERIFY(first->parent() == nullptr);
        QCOMPARE(stack.currentWidget(), last);
        first->callback = {};
        delete first;
    }

    void hideCallbackCanDeleteDestination()
    {
        ZzStack stack;
        auto *first = new ZzStackEventPage;
        QPointer<QWidget> second = new QWidget;
        stack.addWidget(first);
        stack.addWidget(second);
        stack.show();
        QCoreApplication::processEvents();
        first->callback = [&](QEvent::Type type) {
            if (type == QEvent::Hide && second) delete second.data();
        };
        stack.setCurrentWidget(second);
        QCOMPARE(stack.currentWidget(), first);
        QVERIFY(first->isVisible());
        first->callback = {};
    }

    void rejectsInvalidPages()
    {
        QWidget parent;
        ZzStack stack(&parent);
        QWidget foreign(&parent);
        QCOMPARE(stack.addWidget(nullptr), -1);
        QCOMPARE(stack.addWidget(&stack), -1);
        QCOMPARE(stack.addWidget(&parent), -1);
        QCOMPARE(stack.addWidget(&foreign), -1);
        QVERIFY(foreign.parentWidget() == &parent);
        QVERIFY(!stack.setCurrentIndex(0));
        QVERIFY(!stack.setCurrentWidget(&foreign));
        QVERIFY(stack.widget(-1) == nullptr);
        QVERIFY(stack.takeWidget(&foreign) == nullptr);
        auto *page = new QWidget(&stack);
        QCOMPARE(stack.insertWidget(2, page), -1);
        QCOMPARE(stack.addWidget(page), 0);
    }

    void removesCurrentDeterministically()
    {
        ZzStack stack;
        auto *a = new QWidget;
        auto *b = new QWidget;
        auto *c = new QWidget;
        stack.addWidget(a);
        stack.addWidget(b);
        stack.addWidget(c);
        QVERIFY(stack.setCurrentWidget(b));
        delete stack.takeWidget(b);
        QCOMPARE(stack.currentWidget(), c);
        QCOMPARE(stack.currentIndex(), 1);
        delete c;
        QCOMPARE(stack.currentWidget(), a);
        QCOMPARE(stack.currentIndex(), 0);
        delete a;
        QCOMPARE(stack.currentIndex(), -1);
        QCOMPARE(stack.count(), 0);
    }

    void indexChangesPreserveIdentity()
    {
        ZzStack stack;
        auto *page = new QWidget;
        stack.addWidget(page);
        QSignalSpy changed(&stack, &ZzStack::currentChanged);
        auto *before = new QWidget;
        QCOMPARE(stack.insertWidget(0, before), 0);
        QCOMPARE(stack.currentWidget(), page);
        QCOMPARE(stack.currentIndex(), 1);
        QCOMPARE(changed.count(), 1);
        delete stack.takeWidget(before);
        QCOMPARE(stack.currentWidget(), page);
        QCOMPARE(stack.currentIndex(), 0);
        QCOMPARE(changed.count(), 2);
    }

    void observesReparentedPages()
    {
        ZzStack stack;
        QWidget other;
        auto *page = new QWidget;
        stack.addWidget(page);
        page->setParent(&other);
        QCOMPARE(stack.count(), 0);
        QCOMPARE(stack.currentIndex(), -1);
        QCOMPARE(page->parentWidget(), &other);
    }
};

QTEST_MAIN(ZzAnimatedStackedWidgetTest)
#include "ZzAnimatedStackedWidgetTest.moc"
