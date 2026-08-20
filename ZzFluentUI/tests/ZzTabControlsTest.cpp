#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QAccessible>
#include <QtGui/QDragEnterEvent>
#include <QtCore/QMimeData>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtCore/QAbstractAnimation>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include "../widgets/src/private/ZzTabBarPrivate.h"

namespace {

/** @brief 创建带稳定对象名的轻量测试页面。 */
QWidget *zzCreatePage(const QString &name, QWidget *parent = nullptr)
{
    auto *page = new QLabel(name, parent);
    page->setObjectName(name);
    return page;
}

/** @brief 为标签设置全部需要跨容器保留的公开元数据。 */
void zzSetTabMetadata(
    ZzFluentUI::ZzTabWidget *tabs,
    int index)
{
    tabs->setTabToolTip(index, QStringLiteral("工具提示"));
    tabs->setTabWhatsThis(index, QStringLiteral("上下文帮助"));
    tabs->setTabEnabled(index, false);
    tabs->fluentTabBar()->setTabData(
        index,
        QStringLiteral("stable-data"));
    tabs->fluentTabBar()->setTabTextColor(index, QColor(21, 84, 156));
}

} // namespace

class ZzTabControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void workspaceStateAndCloseIntentContract()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *a = zzCreatePage(QStringLiteral("a"));
        auto *b = zzCreatePage(QStringLiteral("b"));
        auto *c = zzCreatePage(QStringLiteral("c"));
        tabs.addTab(a, QStringLiteral("A")); tabs.addTab(b, QStringLiteral("B")); tabs.addTab(c, QStringLiteral("C"));
        QSignalSpy modified(&tabs, &ZzFluentUI::ZzTabWidget::tabModifiedChanged);
        tabs.setTabModified(1, true); tabs.setTabModified(1, true); QCOMPARE(modified.count(), 1);
        QSignalSpy pinned(&tabs, &ZzFluentUI::ZzTabWidget::tabPinnedChanged);
        tabs.setTabPinned(2, true); QCOMPARE(tabs.widget(0), c); tabs.setTabPinned(0, true); QCOMPARE(pinned.count(), 1);
        QSignalSpy attention(&tabs, &ZzFluentUI::ZzTabWidget::tabAttentionChanged);
        QSignalSpy closeEnabled(&tabs, &ZzFluentUI::ZzTabWidget::tabCloseEnabledChanged);
        tabs.setTabAttention(1, true); tabs.setTabAttention(1, true); QCOMPARE(attention.count(), 1);
        tabs.setTabCloseEnabled(1, false); tabs.setTabCloseEnabled(1, false); QCOMPARE(closeEnabled.count(), 1);
        tabs.setPageTitle(1, QStringLiteral("Renamed")); QCOMPARE(tabs.tabText(1), QStringLiteral("Renamed")); QCOMPARE(tabs.widget(1)->windowTitle(), QStringLiteral("Renamed"));
        QSignalSpy batch(&tabs, &ZzFluentUI::ZzTabWidget::tabsCloseRequested);
        tabs.closeOtherTabs(0); QCOMPARE(batch.count(), 1); QCOMPARE(batch.at(0).at(0).value<QList<QWidget *>>().size(), 1);
        batch.clear(); tabs.setTabCloseEnabled(1, true); tabs.closeTabsToRight(0); QCOMPARE(batch.count(), 1); QCOMPARE(batch.at(0).at(0).value<QList<QWidget *>>().size(), 2);
        QVERIFY(tabs.fluentTabBar()->newTabButton() != nullptr);
        QSignalSpy newSpy(&tabs, &ZzFluentUI::ZzTabWidget::newTabRequested);
        QTest::mouseClick(tabs.fluentTabBar()->newTabButton(), Qt::LeftButton);
        QCOMPARE(newSpy.count(), 1);
    }
    void exposesStableDefaults()
    {
        ZzFluentUI::ZzTabWidget tabs;

        QVERIFY(tabs.fluentTabBar() != nullptr);
        QCOMPARE(tabs.fluentTabBar()->parentWidget(), &tabs);
        QVERIFY(tabs.isMovable());
        QVERIFY(tabs.fluentTabBar()->isTearOffEnabled());
        QVERIFY(tabs.fluentTabBar()->isTabTransferEnabled());
        QVERIFY(tabs.fluentTabBar()->acceptDrops());
        QVERIFY(tabs.fluentTabBar()->usesScrollButtons());
        QCOMPARE(tabs.fluentTabBar()->elideMode(), Qt::ElideRight);
    }

    void emitsCapabilityChangesOnlyForRealUpdates()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QSignalSpy tearOffSpy(
            tabs.fluentTabBar(),
            &ZzFluentUI::ZzTabBar::tearOffEnabledChanged);
        QSignalSpy transferSpy(
            tabs.fluentTabBar(),
            &ZzFluentUI::ZzTabBar::tabTransferEnabledChanged);

        tabs.fluentTabBar()->setTearOffEnabled(true);
        tabs.fluentTabBar()->setTabTransferEnabled(true);
        QCOMPARE(tearOffSpy.count(), 0);
        QCOMPARE(transferSpy.count(), 0);

        tabs.fluentTabBar()->setTearOffEnabled(false);
        tabs.fluentTabBar()->setTabTransferEnabled(false);
        QCOMPARE(tearOffSpy.count(), 1);
        QCOMPARE(transferSpy.count(), 1);
        QCOMPARE(tearOffSpy.at(0).at(0).toBool(), false);
        QCOMPARE(transferSpy.at(0).at(0).toBool(), false);
    }

    void transfersPageAndCompleteMetadata()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        QWidget *first = zzCreatePage(QStringLiteral("first"));
        QWidget *moved = zzCreatePage(QStringLiteral("moved"));
        QWidget *last = zzCreatePage(QStringLiteral("last"));
        const QIcon icon = source.style()->standardIcon(
            QStyle::SP_FileIcon);

        source.addTab(first, QStringLiteral("First"));
        source.addTab(moved, icon, QStringLiteral("Moved"));
        target.addTab(last, QStringLiteral("Last"));
        zzSetTabMetadata(&source, 1);
        QSignalSpy transferredSpy(
            &target,
            &ZzFluentUI::ZzTabWidget::tabTransferred);

        QVERIFY(source.transferTabTo(&target, 1, 0));
        QCOMPARE(source.count(), 1);
        QCOMPARE(target.count(), 2);
        QCOMPARE(target.widget(0), moved);
        QCOMPARE(target.tabText(0), QStringLiteral("Moved"));
        QCOMPARE(target.tabIcon(0).cacheKey(), icon.cacheKey());
        QCOMPARE(target.tabToolTip(0), QStringLiteral("工具提示"));
        QCOMPARE(target.tabWhatsThis(0), QStringLiteral("上下文帮助"));
        QVERIFY(!target.isTabEnabled(0));
        QCOMPARE(
            target.fluentTabBar()->tabData(0).toString(),
            QStringLiteral("stable-data"));
        QCOMPARE(
            target.fluentTabBar()->tabTextColor(0),
            QColor(21, 84, 156));
        QCOMPARE(target.currentWidget(), moved);
        QCOMPARE(transferredSpy.count(), 1);
        QCOMPARE(
            qvariant_cast<ZzFluentUI::ZzTabWidget *>(
                transferredSpy.at(0).at(0)),
            &source);
        QCOMPARE(transferredSpy.at(0).at(1).toInt(), 1);
        QCOMPARE(transferredSpy.at(0).at(2).toInt(), 0);
        QCOMPARE(
            qvariant_cast<QWidget *>(transferredSpy.at(0).at(3)),
            moved);
    }

    void reordersWithinSameContainerByInsertionSlot()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QWidget *a = zzCreatePage(QStringLiteral("a"));
        QWidget *b = zzCreatePage(QStringLiteral("b"));
        QWidget *c = zzCreatePage(QStringLiteral("c"));
        tabs.addTab(a, QStringLiteral("A"));
        tabs.addTab(b, QStringLiteral("B"));
        tabs.addTab(c, QStringLiteral("C"));

        QVERIFY(tabs.transferTabTo(&tabs, 0, 3));
        QCOMPARE(tabs.widget(0), b);
        QCOMPARE(tabs.widget(1), c);
        QCOMPARE(tabs.widget(2), a);

        QVERIFY(tabs.transferTabTo(&tabs, 2, 0));
        QCOMPARE(tabs.widget(0), a);
        QCOMPARE(tabs.widget(1), b);
        QCOMPARE(tabs.widget(2), c);

        QVERIFY(tabs.transferTabTo(&tabs, 1, 2));
        QCOMPARE(tabs.widget(1), b);
    }

    void rejectsInvalidTransfersWithoutChangingSource()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        QWidget *page = zzCreatePage(QStringLiteral("guarded"));
        source.addTab(page, QStringLiteral("Guarded"));

        QVERIFY(!source.transferTabTo(nullptr, 0));
        QVERIFY(!source.transferTabTo(&target, -1));
        QVERIFY(!source.transferTabTo(&target, 1));
        target.fluentTabBar()->setTabTransferEnabled(false);
        QVERIFY(!source.transferTabTo(&target, 0));
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QCOMPARE(target.count(), 0);
    }

    void forwardsTearOffIntentWithoutRemovingPage()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QWidget *page = zzCreatePage(QStringLiteral("tear-off"));
        tabs.addTab(page, QStringLiteral("Tear off"));
        QSignalSpy spy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tearOffRequested);
        const QPoint globalPosition(240, 160);

        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tearOffRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(QPoint, globalPosition)));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(qvariant_cast<QWidget *>(spy.at(0).at(1)), page);
        QCOMPARE(spy.at(0).at(2).toPoint(), globalPosition);
        QCOMPARE(tabs.count(), 1);
        QCOMPARE(tabs.widget(0), page);
    }

    void closeRequestIsIntentOnly()
    {
        ZzFluentUI::ZzTabWidget tabs;
        QPointer<QWidget> page = zzCreatePage(QStringLiteral("closable"));
        tabs.addTab(page, QStringLiteral("Closable"));
        tabs.setTabsClosable(true);
        QSignalSpy spy(&tabs, &QTabWidget::tabCloseRequested);

        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(tabs.count(), 1);
        QCOMPARE(tabs.widget(0), page);
        QVERIFY(!page.isNull());
    }

    void rejectsForgedMimePayload()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(
            zzCreatePage(QStringLiteral("mime")),
            QStringLiteral("MIME"));
        QMimeData mimeData;
        mimeData.setData(
            QStringLiteral("application/x-zz-fluent-tab-v1"),
            QByteArrayLiteral("1"));
        QDragEnterEvent event(
            QPoint(4, 4),
            Qt::MoveAction,
            &mimeData,
            Qt::LeftButton,
            Qt::NoModifier);

        QApplication::sendEvent(tabs.fluentTabBar(), &event);

        QVERIFY(!event.isAccepted());
        QCOMPARE(tabs.count(), 1);
    }

    void computesInsertionSlotsForLtrRtlAndVerticalBars()
    {
        ZzFluentUI::ZzTabBar bar;
        bar.addTab(QStringLiteral("One"));
        bar.addTab(QStringLiteral("Two"));
        bar.addTab(QStringLiteral("Three"));
        bar.resize(360, 40);
        bar.show();
        QCoreApplication::processEvents();

        bar.setLayoutDirection(Qt::LeftToRight);
        QCoreApplication::processEvents();
        const QRect firstLtr = bar.tabRect(0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstLtr.center().x() - 1,
                    firstLtr.center().y())),
            0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstLtr.center().x() + 1,
                    firstLtr.center().y())),
            1);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(bar.width() + 10, 10)),
            bar.count());

        bar.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        const QRect firstRtl = bar.tabRect(0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstRtl.center().x() + 1,
                    firstRtl.center().y())),
            0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstRtl.center().x() - 1,
                    firstRtl.center().y())),
            1);

        bar.setShape(QTabBar::RoundedWest);
        bar.resize(80, 300);
        QCoreApplication::processEvents();
        const QRect firstVertical = bar.tabRect(0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstVertical.center().x(),
                    firstVertical.center().y() - 1)),
            0);
        QCOMPARE(
            ZzFluentUI::zzTabInsertionIndex(
                &bar,
                QPoint(
                    firstVertical.center().x(),
                    firstVertical.center().y() + 1)),
            1);
    }

    void keepsPagesAndQObjectBudgetsStableAcrossTransfers()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        QSet<QWidget *> expectedPages;
        for (int index = 0; index < 20; ++index) {
            QWidget *page = zzCreatePage(
                QStringLiteral("page-%1").arg(index));
            source.addTab(page, QString::number(index));
            expectedPages.insert(page);
        }

        const qsizetype sourceObjectCount =
            source.findChildren<QObject *>().size();
        const qsizetype targetObjectCount =
            target.findChildren<QObject *>().size();
        const qsizetype timerCount =
            source.findChildren<QTimer *>().size()
            + target.findChildren<QTimer *>().size();
        const qsizetype animationCount =
            source.findChildren<QAbstractAnimation *>().size()
            + target.findChildren<QAbstractAnimation *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            QWidget *page = source.widget(0);
            QVERIFY(page != nullptr);
            QVERIFY(source.transferTabTo(&target, 0));
            const int targetIndex = target.indexOf(page);
            QVERIFY(targetIndex >= 0);
            QVERIFY(target.transferTabTo(&source, targetIndex));
        }

        QSet<QWidget *> actualPages;
        for (int index = 0; index < source.count(); ++index) {
            actualPages.insert(source.widget(index));
        }
        QCOMPARE(actualPages, expectedPages);
        QCOMPARE(source.count(), 20);
        QCOMPARE(target.count(), 0);
        QCOMPARE(
            source.findChildren<QObject *>().size(),
            sourceObjectCount);
        QCOMPARE(
            target.findChildren<QObject *>().size(),
            targetObjectCount);
        QCOMPARE(
            source.findChildren<QTimer *>().size()
                + target.findChildren<QTimer *>().size(),
            timerCount);
        QCOMPARE(
            source.findChildren<QAbstractAnimation *>().size()
                + target.findChildren<QAbstractAnimation *>().size(),
            animationCount);
    }

    void keepsNativeCtrlTabNavigation()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(
            zzCreatePage(QStringLiteral("keyboard-a")),
            QStringLiteral("A"));
        tabs.addTab(
            zzCreatePage(QStringLiteral("keyboard-b")),
            QStringLiteral("B"));
        tabs.setCurrentIndex(0);
        tabs.show();
        tabs.setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();

        QTest::keyClick(&tabs, Qt::Key_Tab, Qt::ControlModifier);

        QCOMPARE(tabs.currentIndex(), 1);
    }

    void keepsNativeAccessibilityRoles()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(
            zzCreatePage(QStringLiteral("accessible")),
            QStringLiteral("Overview"));
        tabs.setAccessibleName(QStringLiteral("Workspace tabs"));

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(tabs.fluentTabBar());
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::PageTabList);
        QVERIFY(interface->childCount() >= 1);
        QAccessibleInterface *tabInterface = interface->child(0);
        QVERIFY(tabInterface != nullptr);
        QCOMPARE(tabInterface->role(), QAccessible::PageTab);
        QCOMPARE(
            tabInterface->text(QAccessible::Name),
            QStringLiteral("Overview"));
    }
};

QTEST_MAIN(ZzTabControlsTest)

#include "ZzTabControlsTest.moc"
