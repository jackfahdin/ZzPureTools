#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QAccessible>
#include <QtGui/QDragEnterEvent>
#include <QtCore/QMimeData>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtCore/QAbstractAnimation>
#include <QtGui/QContextMenuEvent>
#include <QtCore/QTimer>
#include <QtWidgets/QMenu>
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
    void keepsPinnedPartitionAcrossAllMoves()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *a = zzCreatePage(QStringLiteral("a"));
        auto *b = zzCreatePage(QStringLiteral("b"));
        auto *c = zzCreatePage(QStringLiteral("c"));
        tabs.addTab(a, QStringLiteral("a"));
        tabs.addTab(b, QStringLiteral("b"));
        tabs.addTab(c, QStringLiteral("c"));

        tabs.setTabPinned(2, true);
        tabs.fluentTabBar()->moveTab(0, 2);
        QVERIFY(tabs.transferTabTo(&tabs, 2, 0));
        int ordinary = tabs.count();
        for (int index = 0; index < tabs.count(); ++index) {
            if (!tabs.isTabPinned(index)) {
                ordinary = index;
                break;
            }
        }
        for (int index = 0; index < tabs.count(); ++index) {
            if (tabs.isTabPinned(index)) {
                QVERIFY(index < ordinary);
            }
        }

        tabs.setTabPinned(tabs.indexOf(c), false);
        ordinary = tabs.count();
        for (int index = 0; index < tabs.count(); ++index) {
            if (!tabs.isTabPinned(index)) {
                ordinary = index;
                break;
            }
        }
        for (int index = 0; index < tabs.count(); ++index) {
            if (tabs.isTabPinned(index)) {
                QVERIFY(index < ordinary);
            }
        }
    }

    void normalizesPublicInsertTabAgainstPinnedPartition()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *pinned = zzCreatePage(QStringLiteral("pinned"));
        auto *ordinary = zzCreatePage(QStringLiteral("ordinary"));
        tabs.addTab(pinned, QStringLiteral("Pinned"));
        tabs.setTabPinned(0, true);

        tabs.insertTab(0, ordinary, QStringLiteral("Ordinary"));

        QCOMPARE(tabs.indexOf(pinned), 0);
        QCOMPARE(tabs.indexOf(ordinary), 1);
        QVERIFY(tabs.isTabPinned(0));
        QVERIFY(!tabs.isTabPinned(1));
    }

    void preservesWorkspaceMetadataAcrossTransfersAndFailures()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        auto *page = zzCreatePage(QStringLiteral("page"));
        source.addTab(page, QStringLiteral("page"));
        source.setTabPinned(0, true);
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        QVERIFY(source.transferTabTo(&target, 0));
        QCOMPARE(target.widget(0), page);
        QVERIFY(target.isTabPinned(0));
        QVERIFY(target.isTabModified(0));
        QVERIFY(target.hasTabAttention(0));
        QVERIFY(!target.isTabCloseEnabled(0));

        auto *ordinaryPage = zzCreatePage(QStringLiteral("ordinary"));
        source.addTab(ordinaryPage, QStringLiteral("ordinary"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        QVERIFY(source.transferTabTo(&target, 0, 0));
        const int ordinaryIndex = target.indexOf(ordinaryPage);
        QCOMPARE(ordinaryIndex, 1);
        QVERIFY(!target.isTabPinned(ordinaryIndex));
        QVERIFY(target.isTabModified(ordinaryIndex));
        QVERIFY(target.hasTabAttention(ordinaryIndex));

        auto *failedPage = zzCreatePage(QStringLiteral("fail"));
        source.addTab(failedPage, QStringLiteral("fail"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);
        target.fluentTabBar()->setTabTransferEnabled(false);
        QVERIFY(!source.transferTabTo(&target, 0));
        QCOMPARE(source.widget(0), failedPage);
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));

        QSignalSpy tearOffSpy(
            &source,
            &ZzFluentUI::ZzTabWidget::tearOffRequested);
        QVERIFY(QMetaObject::invokeMethod(
            source.fluentTabBar(),
            "tearOffRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(QPoint, QPoint(20, 20))));
        QCOMPARE(tearOffSpy.count(), 1);
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void filtersCloseIntentByPageState()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *a = zzCreatePage(QStringLiteral("a"));
        auto *b = zzCreatePage(QStringLiteral("b"));
        tabs.addTab(a, QStringLiteral("a"));
        tabs.addTab(b, QStringLiteral("b"));
        tabs.setTabsClosable(true);
        QSignalSpy spy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);

        tabs.setTabCloseEnabled(0, false);
        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));
        QCOMPARE(spy.count(), 0);

        tabs.setTabCloseEnabled(0, true);
        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(
            spy.at(0).at(0).value<QList<QWidget *>>().first(),
            a);

        tabs.setTabPinned(0, true);
        QVERIFY(QMetaObject::invokeMethod(
            tabs.fluentTabBar(),
            "tabCloseRequested",
            Qt::DirectConnection,
            Q_ARG(int, 0)));
        QCOMPARE(spy.count(), 1);
    }

    void laysOutAndInvokesNewTabAndContextActions()
    {
        ZzFluentUI::ZzTabWidget tabs;
        tabs.addTab(zzCreatePage(QStringLiteral("first")),
                    QStringLiteral("First"));
        tabs.addTab(zzCreatePage(QStringLiteral("second")),
                    QStringLiteral("Second"));
        tabs.resize(500, 180);
        tabs.show();
        QCoreApplication::processEvents();

        QWidget *const button = tabs.fluentTabBar()->newTabButton();
        QVERIFY(button->isVisible());
        QVERIFY(!button->geometry().isEmpty());
        const QRect buttonRect(
            button->mapToGlobal(button->rect().topLeft()),
            button->size());
        for (int index = 0; index < tabs.count(); ++index) {
            const QRect tab = tabs.fluentTabBar()->tabRect(index);
            const QRect globalTab(
                tabs.fluentTabBar()->mapToGlobal(tab.topLeft()),
                tab.size());
            QVERIFY(!globalTab.intersects(buttonRect));
        }

        tabs.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        QVERIFY(button->isVisible());

        QSignalSpy newSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::newTabRequested);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(newSpy.count(), 1);

        const QPoint tabPosition = tabs.fluentTabBar()->tabRect(0).center();
        auto triggerContextAction = [&](const QString &text) {
            bool triggered = false;
            QTimer::singleShot(0, [&] {
                auto *menu = qobject_cast<QMenu *>(
                    QApplication::activePopupWidget());
                if (menu == nullptr) {
                    return;
                }
                for (QAction *action : menu->actions()) {
                    if (action->text() == text) {
                        action->trigger();
                        menu->close();
                        triggered = true;
                        return;
                    }
                }
            });
            QContextMenuEvent event(
                QContextMenuEvent::Mouse,
                tabPosition,
                tabs.fluentTabBar()->mapToGlobal(tabPosition));
            QApplication::sendEvent(tabs.fluentTabBar(), &event);
            QCoreApplication::processEvents();
            QVERIFY(triggered);
        };

        QSignalSpy closeSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);
        triggerContextAction(QStringLiteral("关闭其他标签页"));
        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(
            closeSpy.at(0).at(0).value<QList<QWidget *>>().size(),
            1);
        closeSpy.clear();
        triggerContextAction(QStringLiteral("关闭右侧标签页"));
        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(
            closeSpy.at(0).at(0).value<QList<QWidget *>>().size(),
            1);

        triggerContextAction(QStringLiteral("新建标签页"));
        QCOMPARE(newSpy.count(), 2);
    }

    void keepsContextMenuTargetPageAfterReorder()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *first = zzCreatePage(QStringLiteral("first"));
        auto *second = zzCreatePage(QStringLiteral("second"));
        tabs.addTab(first, QStringLiteral("First"));
        tabs.addTab(second, QStringLiteral("Second"));
        tabs.resize(500, 180);
        tabs.show();
        QCoreApplication::processEvents();

        QSignalSpy closeSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);
        const QPoint tabPosition = tabs.fluentTabBar()->tabRect(0).center();
        bool actionTriggered = false;
        QTimer::singleShot(0, [&] {
            auto *menu = qobject_cast<QMenu *>(
                QApplication::activePopupWidget());
            if (menu == nullptr) {
                return;
            }
            tabs.fluentTabBar()->moveTab(0, 1);
            for (QAction *action : menu->actions()) {
                if (action->text() == QStringLiteral("关闭其他标签页")) {
                    action->trigger();
                    menu->close();
                    actionTriggered = true;
                    return;
                }
            }
        });
        QContextMenuEvent event(
            QContextMenuEvent::Mouse,
            tabPosition,
            tabs.fluentTabBar()->mapToGlobal(tabPosition));
        QApplication::sendEvent(tabs.fluentTabBar(), &event);
        QCoreApplication::processEvents();

        QVERIFY(actionTriggered);
        QCOMPARE(closeSpy.count(), 1);
        const QList<QWidget *> pages =
            closeSpy.at(0).at(0).value<QList<QWidget *>>();
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages.front(), second);
    }

    void keepsObserversAndObjectBudgetsStable()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        for (int index = 0; index < 200; ++index) {
            source.addTab(
                zzCreatePage(QString::number(index)),
                QString::number(index));
        }
        const qsizetype objectCount =
            source.findChildren<QObject *>().size()
            + target.findChildren<QObject *>().size();
        const qsizetype timerCount =
            source.findChildren<QTimer *>().size()
            + target.findChildren<QTimer *>().size();
        const qsizetype animationCount =
            source.findChildren<QAbstractAnimation *>().size()
            + target.findChildren<QAbstractAnimation *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            const int index = iteration % source.count();
            source.setCurrentIndex(index);
            source.setTabModified(index, (iteration & 1) != 0);
            source.setTabAttention(index, (iteration & 1) != 0);
        }
        QCOMPARE(
            source.findChildren<QObject *>().size()
                + target.findChildren<QObject *>().size(),
            objectCount);
        QCOMPARE(
            source.findChildren<QTimer *>().size()
                + target.findChildren<QTimer *>().size(),
            timerCount);
        QCOMPARE(
            source.findChildren<QAbstractAnimation *>().size()
                + target.findChildren<QAbstractAnimation *>().size(),
            animationCount);

        int businessSignals = 0;
        auto *next = zzCreatePage(QStringLiteral("next"));
        source.addTab(next, QStringLiteral("next"));
        QObject::connect(
            next,
            &QWidget::windowTitleChanged,
            &source,
            [&businessSignals] { ++businessSignals; });
        QVERIFY(source.transferTabTo(&target, source.indexOf(next)));
        next->setWindowTitle(QStringLiteral("transferred"));
        QCOMPARE(businessSignals, 1);

        QWidget *const removed = source.widget(0);
        source.setTabModified(0, true);
        delete removed;
        QCoreApplication::processEvents();
        auto *replacement = zzCreatePage(QStringLiteral("replacement"));
        source.addTab(replacement, QStringLiteral("replacement"));
        QVERIFY(source.indexOf(replacement) >= 0);
    }

    void restoresTransferAfterTargetIsDestroyedDuringCommit()
    {
        ZzFluentUI::ZzTabWidget source;
        auto *page = zzCreatePage(QStringLiteral("rollback"));
        source.addTab(page, QStringLiteral("Rollback"));
        source.setTabPinned(0, true);
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        auto *target = new ZzFluentUI::ZzTabWidget;
        QPointer<ZzFluentUI::ZzTabWidget> targetGuard(target);
        QObject::connect(
            &source,
            &QTabWidget::currentChanged,
            &source,
            [&targetGuard](int) {
                delete targetGuard.data();
            });

        QVERIFY(!source.transferTabTo(target, 0));
        QVERIFY(targetGuard.isNull());
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QVERIFY(source.isTabPinned(0));
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void rollsBackWhenTargetIsDestroyedFromTabMovedCallback()
    {
        ZzFluentUI::ZzTabWidget source;
        auto *page = zzCreatePage(QStringLiteral("destroyed-target"));
        source.addTab(page, QStringLiteral("Destroyed target"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        auto *target = new ZzFluentUI::ZzTabWidget;
        auto *existing = zzCreatePage(QStringLiteral("existing-pinned"));
        target->addTab(existing, QStringLiteral("Existing"));
        target->setTabPinned(0, true);
        QPointer<ZzFluentUI::ZzTabWidget> targetGuard(target);
        bool destroyingTarget = false;
        QObject::connect(
            target->fluentTabBar(),
            &QTabBar::tabMoved,
            &source,
            [&, page](int, int) {
                if (destroyingTarget || targetGuard.isNull()) {
                    return;
                }
                destroyingTarget = true;
                const int pageIndex = targetGuard->indexOf(page);
                if (pageIndex >= 0) {
                    targetGuard->removeTab(pageIndex);
                }
                page->setParent(nullptr);
                delete targetGuard.data();
            });

        QVERIFY(!source.transferTabTo(target, 0, 0));
        QVERIFY(targetGuard.isNull());
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QVERIFY(!source.isTabPinned(0));
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void rollsBackWhenTargetRemovesPageDuringNormalize()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        auto *page = zzCreatePage(QStringLiteral("removed-during-normalize"));
        source.addTab(page, QStringLiteral("Removed during normalize"));
        source.setTabModified(0, true);
        source.setTabAttention(0, true);
        source.setTabCloseEnabled(0, false);

        auto *pinned = zzCreatePage(QStringLiteral("target-pinned"));
        target.addTab(pinned, QStringLiteral("Pinned"));
        target.setTabPinned(0, true);
        bool removedDuringNormalize = false;
        QObject::connect(
            target.fluentTabBar(),
            &QTabBar::tabMoved,
            &target,
            [&](int, int) {
                const int pageIndex = target.indexOf(page);
                if (pageIndex < 0) {
                    return;
                }
                target.removeTab(pageIndex);
                removedDuringNormalize = true;
            });

        QVERIFY(!source.transferTabTo(&target, 0, 0));
        QVERIFY(removedDuringNormalize);
        QCOMPARE(target.indexOf(page), -1);
        QCOMPARE(source.count(), 1);
        QCOMPARE(source.widget(0), page);
        QVERIFY(!source.isTabPinned(0));
        QVERIFY(source.isTabModified(0));
        QVERIFY(source.hasTabAttention(0));
        QVERIFY(!source.isTabCloseEnabled(0));
    }

    void reportsNormalizedTargetIndexAfterPinnedPartition()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        auto *page = zzCreatePage(QStringLiteral("ordinary-transfer"));
        auto *pinned = zzCreatePage(QStringLiteral("existing-pinned"));
        source.addTab(page, QStringLiteral("Ordinary"));
        target.addTab(pinned, QStringLiteral("Pinned"));
        target.setTabPinned(0, true);
        QSignalSpy transferredSpy(
            &target,
            &ZzFluentUI::ZzTabWidget::tabTransferred);

        QVERIFY(source.transferTabTo(&target, 0, 0));

        QCOMPARE(target.indexOf(page), 1);
        QCOMPARE(transferredSpy.count(), 1);
        QCOMPARE(
            transferredSpy.at(0).at(2).toInt(),
            target.indexOf(page));
    }

    void doesNotStealPageTakenByThirdPartyDuringTransfer()
    {
        ZzFluentUI::ZzTabWidget source;
        ZzFluentUI::ZzTabWidget target;
        ZzFluentUI::ZzTabWidget thirdParty;
        auto *page = zzCreatePage(QStringLiteral("third-party-page"));
        auto *pinned = zzCreatePage(QStringLiteral("target-pinned"));
        source.addTab(page, QStringLiteral("Third party"));
        target.addTab(pinned, QStringLiteral("Pinned"));
        target.setTabPinned(0, true);
        bool taken = false;
        QObject::connect(
            target.fluentTabBar(),
            &QTabBar::tabMoved,
            &target,
            [&](int, int) {
                if (taken) {
                    return;
                }
                const int pageIndex = target.indexOf(page);
                if (pageIndex >= 0) {
                    taken = target.transferTabTo(
                        &thirdParty,
                        pageIndex);
                }
            });

        QVERIFY(!source.transferTabTo(&target, 0, 0));
        QVERIFY(taken);
        QCOMPARE(source.indexOf(page), -1);
        QCOMPARE(target.indexOf(page), -1);
        QCOMPARE(thirdParty.count(), 1);
        QCOMPARE(thirdParty.widget(0), page);
    }

    void workspaceStateAndCloseIntentContract()
    {
        ZzFluentUI::ZzTabWidget tabs;
        auto *a = zzCreatePage(QStringLiteral("a"));
        auto *b = zzCreatePage(QStringLiteral("b"));
        auto *c = zzCreatePage(QStringLiteral("c"));
        tabs.addTab(a, QStringLiteral("A"));
        tabs.addTab(b, QStringLiteral("B"));
        tabs.addTab(c, QStringLiteral("C"));

        QSignalSpy modified(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabModifiedChanged);
        tabs.setTabModified(1, true);
        tabs.setTabModified(1, true);
        QCOMPARE(modified.count(), 1);

        QSignalSpy pinned(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabPinnedChanged);
        tabs.setTabPinned(2, true);
        QCOMPARE(tabs.widget(0), c);
        tabs.setTabPinned(0, true);
        QCOMPARE(pinned.count(), 1);

        QSignalSpy attention(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabAttentionChanged);
        QSignalSpy closeEnabled(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabCloseEnabledChanged);
        tabs.setTabAttention(1, true);
        tabs.setTabAttention(1, true);
        QCOMPARE(attention.count(), 1);
        tabs.setTabCloseEnabled(1, false);
        tabs.setTabCloseEnabled(1, false);
        QCOMPARE(closeEnabled.count(), 1);

        tabs.setPageTitle(1, QStringLiteral("Renamed"));
        QCOMPARE(tabs.tabText(1), QStringLiteral("Renamed"));
        QCOMPARE(
            tabs.widget(1)->windowTitle(),
            QStringLiteral("Renamed"));

        QSignalSpy batch(
            &tabs,
            &ZzFluentUI::ZzTabWidget::tabsCloseRequested);
        tabs.closeOtherTabs(0);
        QCOMPARE(batch.count(), 1);
        QCOMPARE(
            batch.at(0).at(0).value<QList<QWidget *>>().size(),
            1);
        batch.clear();
        tabs.setTabCloseEnabled(1, true);
        tabs.closeTabsToRight(0);
        QCOMPARE(batch.count(), 1);
        QCOMPARE(
            batch.at(0).at(0).value<QList<QWidget *>>().size(),
            2);

        QVERIFY(tabs.fluentTabBar()->newTabButton() != nullptr);
        QSignalSpy newSpy(
            &tabs,
            &ZzFluentUI::ZzTabWidget::newTabRequested);
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
