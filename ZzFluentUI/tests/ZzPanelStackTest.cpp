#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzPanelStack.h>

/** @brief 验证多面板堆栈的所有权、显隐、尺寸和对象稳定性合同。 */
class ZzPanelStackTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ownsTakesAndKeepsVisibleSizes()
    {
        ZzFluentUI::ZzPanelStack stack;
        auto *first = new QWidget;
        QVERIFY(stack.addPanel(first, QStringLiteral("Sessions")));
        auto *second = new QWidget;
        QVERIFY(stack.addPanel(second, QStringLiteral("Files")));
        QCOMPARE(stack.panelCount(), 2);
        QCOMPARE(stack.panels(), QList<QWidget *>({first, second}));
        QCOMPARE(stack.visiblePanels(), QList<QWidget *>({first, second}));
        QCOMPARE(stack.currentPanel(), second);

        QVERIFY(stack.setPanelSizes({180, 320}));
        QCOMPARE(stack.panelSizes(), QList<int>({180, 320}));
        QVERIFY(stack.setPanelVisible(first, false));
        QCOMPARE(stack.visiblePanels(), QList<QWidget *>({second}));
        QVERIFY(!stack.setPanelSizes({0}));
        QCOMPARE(stack.panelSizes(), QList<int>({320}));
        QVERIFY(stack.setPanelVisible(first, true));
        QCOMPARE(stack.visiblePanelCount(), 2);
        QCOMPARE(stack.panelSizes(), QList<int>({180, 320}));

        std::unique_ptr<QWidget> taken(stack.takePanel(first));
        QCOMPARE(taken.get(), first);
        QCOMPARE(first->parent(), nullptr);
        QCOMPARE(stack.panels(), QList<QWidget *>({second}));
    }

    void rejectsParentedAndDuplicatePanelsWithoutMutation()
    {
        ZzFluentUI::ZzPanelStack stack;
        QWidget foreignOwner;
        auto *foreign = new QWidget(&foreignOwner);
        QVERIFY(!stack.addPanel(foreign, QStringLiteral("Foreign")));
        QCOMPARE(foreign->parentWidget(), &foreignOwner);
        QVERIFY(stack.panels().isEmpty());

        auto *page = new QWidget;
        QVERIFY(stack.addPanel(page, QStringLiteral("Original")));
        const qsizetype descendants =
            stack.findChildren<QObject *>().size();
        QVERIFY(!stack.addPanel(page, QStringLiteral("Replacement")));
        QCOMPARE(stack.findChildren<QObject *>().size(), descendants);
        QCOMPARE(stack.panelTitle(page), QStringLiteral("Original"));
        QCOMPARE(stack.panelCount(), 1);
    }

    void movesAndUpdatesRegisteredPanels()
    {
        ZzFluentUI::ZzPanelStack stack;
        auto *first = new QWidget;
        QVERIFY(stack.addPanel(first, QStringLiteral("First")));
        auto *second = new QWidget;
        QVERIFY(stack.addPanel(second, QStringLiteral("Second")));
        auto *third = new QWidget;
        QVERIFY(stack.addPanel(third, QStringLiteral("Third")));
        QSignalSpy movedSpy(
            &stack, &ZzFluentUI::ZzPanelStack::panelMoved);
        QSignalSpy currentSpy(
            &stack, &ZzFluentUI::ZzPanelStack::currentPanelChanged);

        QVERIFY(stack.movePanel(third, 0));
        QCOMPARE(stack.panels(), QList<QWidget *>({third, first, second}));
        QCOMPARE(movedSpy.count(), 1);
        QVERIFY(stack.movePanel(third, 0));
        QCOMPARE(movedSpy.count(), 1);
        QVERIFY(!stack.movePanel(third, 3));

        QVERIFY(stack.setPanelTitle(first, QStringLiteral("Sessions")));
        QCOMPARE(stack.panelTitle(first), QStringLiteral("Sessions"));
        QVERIFY(stack.setPanelIconDescriptor(
            first,
            ZzFluentUI::ZzIconDescriptor::fromBundledSvg(
                ZzFluentUI::ZzBundledSvgIcon::ComputerSystem)));
        QVERIFY(stack.setCurrentPanel(first));
        QCOMPARE(stack.currentPanel(), first);
        QCOMPARE(currentSpy.count(), 1);
        QVERIFY(stack.setCurrentPanel(first));
        QCOMPARE(currentSpy.count(), 1);
    }

    void closeButtonOnlyEmitsIntent()
    {
        ZzFluentUI::ZzPanelStack stack;
        auto *page = new QWidget;
        QVERIFY(stack.addPanel(page, QStringLiteral("Output")));
        QSignalSpy closeSpy(
            &stack, &ZzFluentUI::ZzPanelStack::panelCloseRequested);
        auto *closeButton = stack.findChild<QToolButton *>(
            QStringLiteral("zzPanelStackCloseButton"));
        QVERIFY(closeButton != nullptr);

        closeButton->click();

        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(qvariant_cast<QWidget *>(closeSpy.at(0).at(0)), page);
        QCOMPARE(stack.panelCount(), 1);
        QVERIFY(stack.isPanelVisible(page));
        QCOMPARE(page->parentWidget() != nullptr, true);
    }

    void removesExternallyDestroyedPanels()
    {
        ZzFluentUI::ZzPanelStack stack;
        auto *first = new QWidget;
        QVERIFY(stack.addPanel(first, QStringLiteral("First")));
        auto *second = new QWidget;
        QVERIFY(stack.addPanel(second, QStringLiteral("Second")));
        QCOMPARE(stack.currentPanel(), second);
        QSignalSpy currentSpy(
            &stack, &ZzFluentUI::ZzPanelStack::currentPanelChanged);
        QPointer<QWidget> guard(second);

        delete second;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(guard.isNull());
        QCOMPARE(stack.panelCount(), 1);
        QCOMPARE(stack.panels(), QList<QWidget *>({first}));
        QCOMPARE(stack.currentPanel(), first);
        QCOMPARE(currentSpy.count(), 1);
    }

    void doesNotPublishDestroyedCurrentPanelDuringVisibilitySignal()
    {
        ZzFluentUI::ZzPanelStack stack;
        auto *first = new QWidget;
        QVERIFY(stack.addPanel(first, QStringLiteral("First")));
        auto *second = new QWidget;
        QVERIFY(stack.addPanel(second, QStringLiteral("Second")));
        QPointer<QWidget> firstGuard(first);
        QObject::connect(
            &stack,
            &ZzFluentUI::ZzPanelStack::panelVisibilityChanged,
            &stack,
            [&firstGuard, second](QWidget *content, bool visible) {
                if (content == second
                    && !visible
                    && firstGuard != nullptr) {
                    delete firstGuard.data();
                }
            });
        QSignalSpy currentSpy(
            &stack, &ZzFluentUI::ZzPanelStack::currentPanelChanged);

        QVERIFY(stack.setPanelVisible(second, false));

        QVERIFY(firstGuard.isNull());
        QCOMPARE(stack.panelCount(), 1);
        QCOMPARE(stack.currentPanel(), nullptr);
        QCOMPARE(currentSpy.count(), 1);
        QVERIFY(
            qvariant_cast<QWidget *>(currentSpy.constLast().at(0))
            == nullptr);
    }

    void notifiesSizesWhenVisiblePanelIsDestroyed()
    {
        ZzFluentUI::ZzPanelStack stack;
        auto *first = new QWidget;
        QVERIFY(stack.addPanel(first, QStringLiteral("First")));
        auto *second = new QWidget;
        QVERIFY(stack.addPanel(second, QStringLiteral("Second")));
        QVERIFY(stack.setPanelSizes({180, 320}));
        QSignalSpy sizesSpy(
            &stack, &ZzFluentUI::ZzPanelStack::panelSizesChanged);

        delete first;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QCOMPARE(sizesSpy.count(), 1);
        QCOMPARE(
            qvariant_cast<QList<int>>(sizesSpy.at(0).at(0)),
            QList<int>({320}));
    }

    void survivesStackDeletionFromCurrentPanelSignals()
    {
        {
            auto *stack = new ZzFluentUI::ZzPanelStack;
            auto *page = new QWidget;
            QPointer<ZzFluentUI::ZzPanelStack> stackGuard(stack);
            QPointer<QWidget> pageGuard(page);
            QObject::connect(
                stack,
                &ZzFluentUI::ZzPanelStack::currentPanelChanged,
                stack,
                [stack] { delete stack; });

            QVERIFY(!stack->addPanel(page, QStringLiteral("Add")));
            QVERIFY(stackGuard.isNull());
            QVERIFY(pageGuard.isNull());
        }

        {
            auto *stack = new ZzFluentUI::ZzPanelStack;
            auto *first = new QWidget;
            auto *second = new QWidget;
            QVERIFY(stack->addPanel(first, QStringLiteral("First")));
            QVERIFY(stack->addPanel(second, QStringLiteral("Second")));
            QPointer<ZzFluentUI::ZzPanelStack> stackGuard(stack);
            QObject::connect(
                stack,
                &ZzFluentUI::ZzPanelStack::currentPanelChanged,
                stack,
                [stack] { delete stack; });

            QVERIFY(!stack->setCurrentPanel(first));
            QVERIFY(stackGuard.isNull());
        }

        {
            auto *stack = new ZzFluentUI::ZzPanelStack;
            auto *first = new QWidget;
            auto *second = new QWidget;
            QVERIFY(stack->addPanel(first, QStringLiteral("First")));
            QVERIFY(stack->addPanel(second, QStringLiteral("Second")));
            QPointer<ZzFluentUI::ZzPanelStack> stackGuard(stack);
            QObject::connect(
                stack,
                &ZzFluentUI::ZzPanelStack::currentPanelChanged,
                stack,
                [stack] { delete stack; });

            delete second;
            QCoreApplication::sendPostedEvents(
                nullptr, QEvent::DeferredDelete);
            QVERIFY(stackGuard.isNull());
        }
    }

    void keepsObjectBudgetStableAcrossVisibilityChanges()
    {
        ZzFluentUI::ZzPanelStack stack;
        auto *first = new QWidget;
        QVERIFY(stack.addPanel(first, QStringLiteral("First")));
        auto *second = new QWidget;
        QVERIFY(stack.addPanel(second, QStringLiteral("Second")));
        const qsizetype descendants =
            stack.findChildren<QObject *>().size();
        const qsizetype timers = stack.findChildren<QTimer *>().size();
        const qsizetype animations =
            stack.findChildren<QAbstractAnimation *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            QVERIFY(stack.setPanelVisible(first, false));
            QVERIFY(stack.setPanelVisible(first, true));
            QVERIFY(stack.setPanelSizes({180, 320}));
        }

        QCOMPARE(stack.findChildren<QObject *>().size(), descendants);
        QCOMPARE(stack.findChildren<QTimer *>().size(), timers);
        QCOMPARE(
            stack.findChildren<QAbstractAnimation *>().size(),
            animations);
    }
};

QTEST_MAIN(ZzPanelStackTest)

#include "ZzPanelStackTest.moc"
