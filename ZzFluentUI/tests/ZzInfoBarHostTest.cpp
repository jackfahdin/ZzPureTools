#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtGui/QEnterEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>
#include <limits>
#include <new>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzInfoBarHost.h>
#include <ZzFluentUI/ZzThemeController.h>

using ZzFluentUI::ZzInfoBar;
using ZzFluentUI::ZzInfoBarHost;

class ZzInfoBarHostTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void showsAndDismisses()
    {
        QWidget target;
        target.resize(600, 400);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        QSignalSpy closed(&host, &ZzInfoBarHost::infoBarClosed);
        auto* bar = host.showInfoBar(ZzInfoBar::Success, QStringLiteral("Saved"),
            QStringLiteral("All changes saved"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar != nullptr);
        QTRY_COMPARE(shown.size(), 1);
        QVERIFY(bar->isOpen());
        host.dismissAll();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 1500);
    }

    void noTargetDoesNotCreateBar()
    {
        ZzInfoBarHost host(nullptr);
        QCOMPARE(host.showInfoBar(ZzInfoBar::Error, {}, {}), nullptr);
    }

    void positions_data()
    {
        QTest::addColumn<int>("position");
        for (int i = 0; i < 6; ++i)
            QTest::newRow(QByteArray::number(i).constData()) << i;
    }

    void positions()
    {
        QFETCH(int, position);
        QWidget target;
        target.resize(600, 400);
        target.show();
        ZzInfoBarHost host(&target);
        auto* bar = host.showInfoBar(ZzInfoBar::Informational, QStringLiteral("Info"),
            QStringLiteral("Message"), static_cast<ZzInfoBarHost::Position>(position), 0);
        QVERIFY(bar != nullptr);
        QTRY_VERIFY(bar->isVisible());
        const QRect geometry = bar->parentWidget()->geometry();
        if (position == ZzInfoBarHost::TopLeft || position == ZzInfoBarHost::BottomLeft)
            QCOMPARE(geometry.left(), host.margin());
        if (position == ZzInfoBarHost::TopRight || position == ZzInfoBarHost::BottomRight)
            QCOMPARE(geometry.right(), target.width() - host.margin() - 1);
        if (position == ZzInfoBarHost::Top || position == ZzInfoBarHost::Bottom)
            QVERIFY(qAbs(geometry.center().x() - target.rect().center().x()) <= 1);
        if (position <= ZzInfoBarHost::TopRight)
            QCOMPARE(geometry.top(), host.margin());
        else
            QCOMPARE(geometry.bottom(), target.height() - host.margin() - 1);
    }

    void strictFifoAndResizeRecovery()
    {
        QWidget target;
        target.resize(500, 120);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        auto* first = host.showInfoBar(ZzInfoBar::Success, QStringLiteral("1"),
            QStringLiteral("First"), ZzInfoBarHost::TopRight, 0);
        auto* second = host.showInfoBar(ZzInfoBar::Success, QStringLiteral("2"),
            QStringLiteral("Second"), ZzInfoBarHost::TopRight, 0);
        auto* third = host.showInfoBar(ZzInfoBar::Success, QStringLiteral("3"),
            QStringLiteral("Third"), ZzInfoBarHost::TopRight, 0);
        QTRY_COMPARE(shown.size(), 1);
        QVERIFY(first->parentWidget()->isVisible());
        QVERIFY(second->parentWidget()->isHidden());
        QVERIFY(third->parentWidget()->isHidden());
        target.resize(500, 300);
        QTRY_COMPARE(shown.size(), 3);
        QVERIFY(second->parentWidget()->isVisible());
        target.resize(500, 120);
        QTRY_VERIFY(second->parentWidget()->isHidden());
        target.resize(500, 300);
        QTRY_VERIFY(second->parentWidget()->isVisible());
        host.dismissAll();
    }

    void timeoutPausesOnHoverAndHiddenTarget()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy closed(&host, &ZzInfoBarHost::infoBarClosed);
        auto* bar = host.showInfoBar(
            ZzInfoBar::Warning, {}, QStringLiteral("Pause"), ZzInfoBarHost::Top, 120);
        QVERIFY(bar != nullptr);
        QTRY_VERIFY(bar->isVisible());
        QEnterEvent enter({ 2, 2 }, { 2, 2 }, { 2, 2 });
        QCoreApplication::sendEvent(bar, &enter);
        QTest::qWait(170);
        QCOMPARE(closed.size(), 0);
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(bar, &leave);
        target.hide();
        QTest::qWait(170);
        QCOMPARE(closed.size(), 0);
        target.show();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 1200);
    }

    void transferAndDefaultTarget()
    {
        QWidget firstTarget;
        QWidget secondTarget;
        firstTarget.resize(500, 300);
        secondTarget.resize(500, 300);
        firstTarget.show();
        secondTarget.show();
        ZzInfoBarHost first(&firstTarget);
        ZzInfoBarHost second(&secondTarget);
        QSignalSpy firstClosed(&first, &ZzInfoBarHost::infoBarClosed);
        auto* bar = first.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Transfer"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar != nullptr);
        second.addInfoBar(bar, ZzInfoBarHost::BottomLeft, 0);
        QCOMPARE(firstClosed.size(), 1);
        QCOMPARE(bar->parentWidget()->parentWidget(), &secondTarget);
        ZzInfoBarHost::setDefaultTarget(&firstTarget);
        QVERIFY(ZzInfoBarHost::defaultHost() != nullptr);
        ZzInfoBarHost::setDefaultTarget(&secondTarget);
        QCOMPARE(ZzInfoBarHost::defaultTarget(), &secondTarget);
        ZzInfoBarHost::setDefaultTarget(nullptr);
        QCOMPARE(ZzInfoBarHost::defaultHost(), nullptr);
    }

    void shownCallbackMayDeleteHost()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        QPointer<ZzInfoBarHost> host(new ZzInfoBarHost(&target));
        QObject::connect(
            host, &ZzInfoBarHost::infoBarShown, &target, [host] { delete host.data(); });
        host->showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Delete"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(host.isNull());
    }

    void shownCallbackMayDeleteBar()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QPointer<ZzInfoBar> bar;
        QObject::connect(&host, &ZzInfoBarHost::infoBarShown, &target,
            [&bar](ZzInfoBar* shown, ZzInfoBarHost::Position) {
                bar = shown;
                delete shown;
            });
        host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Delete"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar.isNull());
    }

    void closedCallbackMayDeleteHost()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        QPointer<ZzInfoBarHost> host(new ZzInfoBarHost(&target));
        auto* bar = host->showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Delete"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar != nullptr);
        QObject::connect(
            host, &ZzInfoBarHost::infoBarClosed, &target, [host] { delete host.data(); });
        host->dismissAll();
        QTRY_VERIFY_WITH_TIMEOUT(host.isNull(), 1200);
    }

    void targetDeletionStopsOutstandingEntries()
    {
        auto* target = new QWidget;
        target->resize(500, 120);
        target->show();
        QObject owner;
        ZzInfoBarHost host(target, &owner);
        QPointer<ZzInfoBar> first(host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Active"), ZzInfoBarHost::TopRight, 100));
        QPointer<ZzInfoBar> queued(host.showInfoBar(
            ZzInfoBar::Warning, {}, QStringLiteral("Queued"), ZzInfoBarHost::TopRight, 100));
        QVERIFY(first && queued);
        delete target;
        QCOMPARE(host.target(), nullptr);
        QVERIFY(first.isNull());
        QVERIFY(queued.isNull());
    }

    void externallyDeletedBarAddressCanBeReused()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        alignas(ZzInfoBar) unsigned char storage[sizeof(ZzInfoBar)];
        auto* first = new (storage) ZzInfoBar;
        host.addInfoBar(first, ZzInfoBarHost::TopRight, 0);
        QCOMPARE(shown.size(), 1);
        first->~ZzInfoBar();
        auto* second = new (storage) ZzInfoBar;
        host.addInfoBar(second, ZzInfoBarHost::TopRight, 0);
        QCOMPARE(shown.size(), 2);
        QCOMPARE(second->parentWidget()->parentWidget(), &target);
        second->~ZzInfoBar();
    }

    void expiredTimerOnTargetShowMayDeleteHost()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QWidget target;
        target.resize(500, 300);
        target.show();
        QPointer<ZzInfoBarHost> host(new ZzInfoBarHost(&target));
        auto* bar = host->showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Expires"), ZzInfoBarHost::TopRight, 10);
        QVERIFY(bar);
        bar->parentWidget()->setStyle(&style);
        QObject::connect(
            host, &ZzInfoBarHost::infoBarClosed, &target, [host] { delete host.data(); });
        QThread::msleep(25);
        target.hide();
        QVERIFY(host);
        target.show();
        QVERIFY(host.isNull());
    }

    void expiredTimerOnBarLeaveMayDeleteHost()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QWidget target;
        target.resize(500, 300);
        target.show();
        QPointer<ZzInfoBarHost> host(new ZzInfoBarHost(&target));
        auto* bar = host->showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Expires"), ZzInfoBarHost::TopRight, 10);
        QVERIFY(bar);
        bar->parentWidget()->setStyle(&style);
        QObject::connect(
            host, &ZzInfoBarHost::infoBarClosed, &target, [host] { delete host.data(); });
        QThread::msleep(25);
        QEnterEvent enter({ 2, 2 }, { 2, 2 }, { 2, 2 });
        QCoreApplication::sendEvent(bar, &enter);
        QVERIFY(host);
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(bar, &leave);
        QVERIFY(host.isNull());
    }

    void expiredTimerOnBarLeaveMayDeleteFilteredBar()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        QPointer<ZzInfoBarHost> host(new ZzInfoBarHost(&target));
        QPointer<ZzInfoBar> bar(host->showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Expires"), ZzInfoBarHost::TopRight, 10));
        QVERIFY(bar);
        QThread::msleep(25);
        QEnterEvent enter({ 2, 2 }, { 2, 2 }, { 2, 2 });
        QCoreApplication::sendEvent(bar, &enter);
        QVERIFY(host);
        QObject::connect(bar, &ZzInfoBar::openChanged, &target, [host](bool open) {
            if (!open)
                delete host.data();
        });
        QEvent leave(QEvent::Leave);
        QVERIFY(QCoreApplication::sendEvent(bar, &leave));
        QVERIFY(host.isNull());
        QVERIFY(bar.isNull());
    }

    void expiredTimerOnTargetShowMayDeleteFilteredTarget()
    {
        QObject owner;
        QPointer<QWidget> target(new QWidget);
        target->resize(500, 300);
        target->show();
        ZzInfoBarHost host(target, &owner);
        auto* bar = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Expires"), ZzInfoBarHost::TopRight, 10);
        QVERIFY(bar);
        QThread::msleep(25);
        target->hide();
        QVERIFY(target);
        target->removeEventFilter(&host);
        target->show();
        target->installEventFilter(&host);
        QObject::connect(bar, &ZzInfoBar::openChanged, &owner, [&target](bool open) {
            if (!open)
                delete target.data();
        });
        QEvent show(QEvent::Show);
        QVERIFY(QCoreApplication::sendEvent(target, &show));
        QVERIFY(target.isNull());
        QCOMPARE(host.target(), nullptr);
    }

    void expiredTimerOnReopenMayDeleteHost()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        QPointer<ZzInfoBarHost> host(new ZzInfoBarHost(&target));
        auto* bar = host->showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Expires"), ZzInfoBarHost::TopRight, 10);
        QVERIFY(bar);
        QThread::msleep(25);
        bar->dismiss();
        QVERIFY(host);
        QObject::connect(bar, &ZzInfoBar::openChanged, &target, [host](bool open) {
            if (!open)
                delete host.data();
        });
        bar->setOpen(true);
        QVERIFY(host.isNull());
    }

    void expiredQueuedTimerDoesNotEmitShown()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QWidget target;
        target.setStyle(&style);
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        QPointer<ZzInfoBar> first(host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Expires"), ZzInfoBarHost::TopRight, 10));
        ZzInfoBar* firstAddress = first.data();
        QCOMPARE(shown.size(), 1);
        QThread::msleep(25);
        target.resize(500, 50);
        host.addInfoBar(new ZzInfoBar, ZzInfoBarHost::TopRight, 0);
        QVERIFY(first);
        QCOMPARE(shown.size(), 1);
        target.resize(500, 300);
        host.addInfoBar(new ZzInfoBar, ZzInfoBarHost::TopRight, 0);
        QVERIFY(!first || !first->isOpen());
        int firstShown = 0;
        for (const auto& emission : shown) {
            if (emission.at(0).value<ZzInfoBar*>() == firstAddress)
                ++firstShown;
        }
        QCOMPARE(firstShown, 1);
    }

    void shownCallbackAddsNextBar()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        bool added = false;
        QObject::connect(
            &host, &ZzInfoBarHost::infoBarShown, &target, [&](ZzInfoBar*, ZzInfoBarHost::Position) {
                if (!added) {
                    added = true;
                    host.showInfoBar(ZzInfoBar::Success, {}, QStringLiteral("Second"),
                        ZzInfoBarHost::TopRight, 0);
                }
            });
        host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("First"), ZzInfoBarHost::TopRight, 0);
        QTRY_COMPARE(shown.size(), 2);
    }

    void invalidEnumsDoNotCreateOrAdopt()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        volatile int invalidValue = 99;
        const auto invalidSeverity = static_cast<ZzInfoBar::Severity>(invalidValue);
        const auto invalidPosition = static_cast<ZzInfoBarHost::Position>(invalidValue);
        QCOMPARE(host.showInfoBar(invalidSeverity, {}, QStringLiteral("Invalid")), nullptr);
        QCOMPARE(
            host.showInfoBar(ZzInfoBar::Success, {}, QStringLiteral("Invalid"), invalidPosition),
            nullptr);
        ZzInfoBar bar;
        host.addInfoBar(&bar, invalidPosition, 0);
        QCOMPARE(bar.parentWidget(), nullptr);
        QVERIFY(!bar.isOpen());
        QCOMPARE(shown.size(), 0);
    }

    void hugeMarginKeepsBarQueued()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        host.setMargin(std::numeric_limits<int>::max());
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        auto* bar = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Queued"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar != nullptr);
        QCOMPARE(shown.size(), 0);
        QVERIFY(bar->parentWidget()->isHidden());
    }

    void hugeSpacingKeepsSecondBarQueued()
    {
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        host.setSpacing(std::numeric_limits<int>::max());
        QSignalSpy shown(&host, &ZzInfoBarHost::infoBarShown);
        auto* first = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("First"), ZzInfoBarHost::TopRight, 0);
        auto* second = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Second"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(first && second);
        QCOMPARE(shown.size(), 1);
        QVERIFY(second->parentWidget()->isHidden());
    }

    void livePopupLeaveFinishesWhenMotionIsReduced()
    {
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QWidget target;
        target.setStyle(&style);
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy closed(&host, &ZzInfoBarHost::infoBarClosed);
        auto* bar = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Leaving"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar);
        bar->parentWidget()->setStyle(&style);
        bar->dismiss();
        QCOMPARE(closed.size(), 0);
        theme.setReducedMotion(true);
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 100);
    }

    void livePopupEnterFinishesWhenMotionIsReduced()
    {
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        auto* bar = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Entering"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar);
        bar->parentWidget()->setStyle(&style);
        QVERIFY(bar->x() > 6);
        theme.setReducedMotion(true);
        QTRY_COMPARE_WITH_TIMEOUT(bar->x(), 6, 100);
    }

    void livePopupPositionFinishesWhenMotionIsReduced()
    {
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        auto* first = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("First"), ZzInfoBarHost::TopRight, 0);
        auto* second = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Second"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(first && second);
        QWidget* secondSurface = second->parentWidget();
        secondSurface->setStyle(&style);
        QSignalSpy closed(&host, &ZzInfoBarHost::infoBarClosed);
        first->dismiss();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(secondSurface->y() > host.margin(), 100);
        theme.setReducedMotion(true);
        QTRY_COMPARE_WITH_TIMEOUT(secondSurface->y(), host.margin(), 100);
    }

    void livePopupLeaveFinishesWhenTargetIsHiddenOrDisabled_data()
    {
        QTest::addColumn<bool>("hideTarget");
        QTest::newRow("hidden") << true;
        QTest::newRow("disabled") << false;
    }

    void livePopupLeaveFinishesWhenTargetIsHiddenOrDisabled()
    {
        QFETCH(bool, hideTarget);
        QWidget target;
        target.resize(500, 300);
        target.show();
        ZzInfoBarHost host(&target);
        QSignalSpy closed(&host, &ZzInfoBarHost::infoBarClosed);
        auto* bar = host.showInfoBar(
            ZzInfoBar::Success, {}, QStringLiteral("Leaving"), ZzInfoBarHost::TopRight, 0);
        QVERIFY(bar);
        bar->dismiss();
        QCOMPARE(closed.size(), 0);
        if (hideTarget)
            target.hide();
        else
            target.setEnabled(false);
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 100);
    }
};

QTEST_MAIN(ZzInfoBarHostTest)
#include "ZzInfoBarHostTest.moc"
