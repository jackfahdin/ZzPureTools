#include <QtGui/QAccessible>
#include <QtCore/QPointer>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzRangeSlider.h>

class ZzRangeSliderAccessibilityTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void focusCallbackMayScheduleSliderDeletion()
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *edit = new QLineEdit(&window);
        auto *slider = new ZzFluentUI::ZzRangeSlider(&window);
        layout->addWidget(edit);
        layout->addWidget(slider);
        window.show();
        window.activateWindow();
        edit->setFocus();
        QTRY_COMPARE(QApplication::focusWidget(), edit);
        QPointer<ZzFluentUI::ZzRangeSlider> guard = slider;
        auto *root = QAccessible::queryAccessibleInterface(slider);
        QCOMPARE(root->childCount(), 2);
        auto *upper = root->child(1);
        const auto id = QAccessible::uniqueId(upper);
        auto *action = upper->actionInterface();
        QVERIFY(action);
        const auto connection = QObject::connect(qApp, &QApplication::focusChanged,
            &window, [slider](QWidget *, QWidget *now) { if (now == slider) slider->deleteLater(); });
        action->doAction(QAccessibleActionInterface::setFocusAction());
        QObject::disconnect(connection);
        // Qt 正在分发焦点事件时必须使用延迟销毁。
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(guard.isNull());
        QVERIFY(!QAccessible::accessibleInterface(id));
    }

    void exposesTwoIndependentBoundedValues()
    {
        ZzFluentUI::ZzRangeSlider slider;
        slider.setAccessibleName(QStringLiteral("Price"));
        slider.setValues(20, 80);
        auto *root = QAccessible::queryAccessibleInterface(&slider);
        QVERIFY(root);
        QCOMPARE(root->childCount(), 2);
        auto *lower = root->child(0);
        auto *upper = root->child(1);
        QVERIFY(lower && upper);
        QCOMPARE(lower->parent(), root);
        QCOMPARE(upper->parent(), root);
        QCOMPARE(root->indexOfChild(upper), 1);
        QVERIFY(lower->valueInterface() && upper->valueInterface());
        QVERIFY(lower->text(QAccessible::Name) != upper->text(QAccessible::Name));
        QCOMPARE(lower->valueInterface()->maximumValue().toInt(), 80);
        QCOMPARE(upper->valueInterface()->minimumValue().toInt(), 20);
        lower->valueInterface()->setCurrentValue(95);
        QCOMPARE(slider.lowerValue(), 80);
        upper->valueInterface()->setCurrentValue(10);
        QCOMPARE(slider.upperValue(), 80);
        slider.setValues(20, 80);
        slider.setDisabled(true);
        QVERIFY(lower->state().disabled);
        lower->valueInterface()->setCurrentValue(40);
        QCOMPARE(slider.lowerValue(), 20);
        slider.setEnabled(true);
        lower->valueInterface()->setCurrentValue(QStringLiteral("invalid"));
        QCOMPARE(slider.lowerValue(), 20);
    }

    void childrenFollowFocusGeometryAndLifetime()
    {
        auto *slider = new ZzFluentUI::ZzRangeSlider;
        slider->resize(220, 32);
        slider->setValues(20, 80);
        slider->show();
        slider->activateWindow();
        slider->setFocus();
        QCoreApplication::processEvents();
        auto *root = QAccessible::queryAccessibleInterface(slider);
        QCOMPARE(root->childCount(), 2);
        auto *lower = root->child(0);
        auto *upper = root->child(1);
        QVERIFY(lower->rect().center().x() < upper->rect().center().x());
        QCOMPARE(root->childAt(lower->rect().center().x(), lower->rect().center().y()), lower);
        auto *action = upper->actionInterface();
        QVERIFY(action);
        action->doAction(QAccessibleActionInterface::setFocusAction());
        QCOMPARE(root->focusChild(), upper);
        QVERIFY(upper->state().focused);
        action->doAction(QAccessibleActionInterface::decreaseAction());
        QCOMPARE(slider->upperValue(), 79);
        slider->setLayoutDirection(Qt::RightToLeft);
        QVERIFY(lower->rect().center().x() > upper->rect().center().x());
        const auto lowerId = QAccessible::uniqueId(lower);
        const auto upperId = QAccessible::uniqueId(upper);
        delete slider;
        QVERIFY(!QAccessible::accessibleInterface(lowerId));
        QVERIFY(!QAccessible::accessibleInterface(upperId));
    }
};

QTEST_MAIN(ZzRangeSliderAccessibilityTest)
#include "ZzRangeSliderAccessibilityTest.moc"
