#include <QtCore/QPointer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzInfoBar.h>
#include <ZzFluentUI/ZzThemeController.h>

using ZzFluentUI::ZzInfoBar;

class ZzInfoBarTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void opensAndDismisses()
    {
        ZzInfoBar bar;
        bar.setAnimationEnabled(false);
        QSignalSpy opened(&bar, &ZzInfoBar::opened);
        QSignalSpy closed(&bar, &ZzInfoBar::closed);
        QSignalSpy changed(&bar, &ZzInfoBar::openChanged);
        QVERIFY(bar.isHidden());
        bar.setOpen(true);
        QVERIFY(bar.isOpen());
        QVERIFY(!bar.isHidden());
        QCOMPARE(opened.size(), 1);
        bar.setOpen(true);
        QCOMPARE(changed.size(), 1);
        bar.dismiss();
        QVERIFY(!bar.isOpen());
        QVERIFY(bar.isHidden());
        QCOMPARE(closed.size(), 1);
        QCOMPARE(changed.size(), 2);
    }

    void actionOwnershipAndClick()
    {
        ZzInfoBar bar;
        bar.setAnimationEnabled(false);
        bar.setActionButtonText(QStringLiteral("Retry"));
        bar.setOpen(true);
        QSignalSpy triggered(&bar, &ZzInfoBar::actionTriggered);
        QTest::mouseClick(bar.actionButton(), Qt::LeftButton);
        QCOMPARE(triggered.size(), 1);

        auto* first = new QPushButton(QStringLiteral("First"));
        QPointer<QWidget> guard(first);
        bar.setActionWidget(first);
        QCOMPARE(bar.actionWidget(), first);
        bar.setActionWidget(new QPushButton(QStringLiteral("Second")));
        QVERIFY(guard.isNull());
        QWidget* taken = bar.takeActionWidget();
        QVERIFY(taken != nullptr);
        QCOMPARE(taken->parentWidget(), nullptr);
        delete taken;
        QCOMPARE(bar.actionWidget(), nullptr);
    }

    void closeButtonDismisses()
    {
        ZzInfoBar bar;
        bar.setAnimationEnabled(false);
        bar.setOpen(true);
        QSignalSpy clicked(&bar, &ZzInfoBar::closeButtonClicked);
        auto* button = bar.findChild<QToolButton*>();
        QVERIFY(button != nullptr);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(clicked.size(), 1);
        QVERIFY(bar.isHidden());
    }

    void wrapsAndMirrors()
    {
        ZzInfoBar bar;
        bar.setAnimationEnabled(false);
        bar.setTitle(QStringLiteral("A title"));
        bar.setMessage(QStringLiteral("A message that is long enough to wrap across several "
                                      "lines in a narrow information bar"));
        bar.setActionButtonText(QStringLiteral("Action"));
        const int wide = bar.heightForWidth(700);
        const int narrow = bar.heightForWidth(190);
        QVERIFY(narrow > wide);
        QVERIFY(wide >= 48);
        bar.resize(600, wide);
        bar.setOpen(true);
        QCoreApplication::processEvents();
        auto* title = bar.findChild<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
        Q_UNUSED(title)
        const auto labels = bar.findChildren<QLabel*>();
        QCOMPARE(labels.size(), 2);
        const int leftX = labels[0]->geometry().x();
        bar.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        QVERIFY(labels[0]->geometry().x() != leftX);
    }

    void externalActionLossAndInvalidOwnership()
    {
        ZzInfoBar bar;
        bar.setActionWidget(&bar);
        QCOMPARE(bar.actionWidget(), nullptr);
        bar.setActionWidget(bar.actionButton());
        QCOMPARE(bar.actionWidget(), nullptr);
        auto* first = new QWidget;
        bar.setActionWidget(first);
        delete first;
        QCOMPARE(bar.actionWidget(), nullptr);
        auto* second = new QWidget;
        bar.setActionWidget(second);
        QWidget elsewhere;
        second->setParent(&elsewhere);
        QCOMPARE(bar.actionWidget(), nullptr);
        QCOMPARE(second->parentWidget(), &elsewhere);
    }

    void reversesAnimationAndReducedMotion()
    {
        QWidget parent;
        auto* layout = new QVBoxLayout(&parent);
        auto* bar = new ZzInfoBar(&parent);
        bar->setMessage(QStringLiteral("Animated"));
        layout->addWidget(bar);
        parent.resize(400, 200);
        parent.show();
        QCoreApplication::processEvents();
        QSignalSpy opened(bar, &ZzInfoBar::opened);
        QSignalSpy closed(bar, &ZzInfoBar::closed);
        bar->setOpen(true);
        QTest::qWait(25);
        bar->dismiss();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 500);
        QCOMPARE(opened.size(), 0);
        QVERIFY(bar->isHidden());
        bar->setOpen(true);
        bar->setProperty("reducedMotion", true);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(bar->maximumHeight(), QWIDGETSIZE_MAX);
    }

    void callbackMayDeleteBar()
    {
        QPointer<ZzInfoBar> bar(new ZzInfoBar);
        QObject::connect(bar, &ZzInfoBar::openChanged, bar, [bar] { delete bar.data(); });
        bar->setOpen(true);
        QVERIFY(bar.isNull());
    }

    void replacementCallbackMayDeleteBar()
    {
        QPointer<ZzInfoBar> bar(new ZzInfoBar);
        auto* old = new QWidget;
        bar->setActionWidget(old);
        QObject::connect(old, &QObject::destroyed, bar, [bar] { delete bar.data(); });
        bar->setActionWidget(new QWidget);
        QVERIFY(bar.isNull());
    }

    void replacementCallbackMayDeleteIncoming()
    {
        ZzInfoBar bar;
        auto* old = new QWidget;
        bar.setActionWidget(old);
        QPointer<QWidget> incoming(new QWidget);
        QObject::connect(old, &QObject::destroyed, &bar, [incoming] { delete incoming.data(); });
        bar.setActionWidget(incoming);
        QVERIFY(incoming.isNull());
        QCOMPARE(bar.actionWidget(), nullptr);
    }

    void replacementCallbackMayReenter()
    {
        ZzInfoBar bar;
        auto* old = new QWidget;
        bar.setActionWidget(old);
        QPointer<QWidget> incoming(new QWidget);
        QObject::connect(
            old, &QObject::destroyed, &bar, [&bar] { bar.setActionWidget(new QWidget); });
        bar.setActionWidget(incoming);
        QVERIFY(incoming.isNull());
        QVERIFY(bar.actionWidget() != nullptr);
    }

    void invalidSeverityIsIgnored()
    {
        ZzInfoBar bar;
        QSignalSpy changed(&bar, &ZzInfoBar::severityChanged);
        volatile int invalidValue = 99;
        bar.setSeverity(static_cast<ZzInfoBar::ZzInfoSeverity>(invalidValue));
        QCOMPARE(bar.severity(), ZzInfoBar::Informational);
        QCOMPARE(changed.size(), 0);
    }

    void liveInlineCloseFinishesWhenMotionStops_data()
    {
        QTest::addColumn<int>("change");
        QTest::newRow("reduced-motion") << 0;
        QTest::newRow("hidden") << 1;
        QTest::newRow("disabled") << 2;
    }

    void liveInlineCloseFinishesWhenMotionStops()
    {
        QFETCH(int, change);
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QWidget parent;
        parent.setStyle(&style);
        auto* layout = new QVBoxLayout(&parent);
        auto* bar = new ZzInfoBar(&parent);
        bar->setStyle(&style);
        layout->addWidget(bar);
        parent.resize(400, 200);
        parent.show();
        bar->setOpen(true);
        QSignalSpy opened(bar, &ZzInfoBar::opened);
        QSignalSpy closed(bar, &ZzInfoBar::closed);
        QTRY_COMPARE_WITH_TIMEOUT(opened.size(), 1, 1000);
        bar->dismiss();
        QCOMPARE(closed.size(), 0);
        if (change == 0)
            theme.setReducedMotion(true);
        else if (change == 1)
            parent.hide();
        else
            parent.setEnabled(false);
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 100);
    }
};

QTEST_MAIN(ZzInfoBarTest)
#include "ZzInfoBarTest.moc"
