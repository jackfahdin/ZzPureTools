#include <QtCore/QPointer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <functional>

#include <ZzFluentUI/ZzAnimatedStackedWidget.h>

using ZzStack = ZzFluentUI::ZzAnimatedStackedWidget;

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
