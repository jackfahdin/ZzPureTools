#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzSidePaneMode.h>

/** @brief 验证 Side Pane 的页面所有权、宽度和物理边缘契约。 */
class ZzSidePaneTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addsTakesAndRejectsForeignPageTransfersWithoutChangingOwnership()
    {
        ZzFluentUI::ZzSidePane pane;
        auto *page = new QLabel(QStringLiteral("Page"));
        QVERIFY(pane.addWidget(page, QStringLiteral("First")));
        QCOMPARE(pane.currentWidget(), page);
        QVERIFY(page->parentWidget() != nullptr);
        QCOMPARE(pane.panelStack()->panels(), QList<QWidget *>({page}));

        std::unique_ptr<QWidget> taken(pane.takeWidget(page));
        QCOMPARE(taken.get(), page);
        QCOMPARE(taken->parentWidget(), nullptr);
        QVERIFY(!pane.setCurrentWidget(page));

        QWidget foreignOwner;
        auto *foreign = new QLabel(QStringLiteral("Foreign"), &foreignOwner);
        QVERIFY(!pane.addWidget(foreign, QStringLiteral("Rejected")));
        QCOMPARE(foreign->parentWidget(), &foreignOwner);
    }

    void keepsSingleCompatibilityAndRestoresStackedVisiblePanels()
    {
        ZzFluentUI::ZzSidePane pane;
        auto *first = new QLabel(QStringLiteral("First"));
        auto *second = new QLabel(QStringLiteral("Second"));
        QVERIFY(pane.addWidget(first, QStringLiteral("First")));
        QVERIFY(pane.addWidget(second, QStringLiteral("Second")));

        QCOMPARE(pane.mode(), ZzFluentUI::ZzSidePaneMode::Single);
        QCOMPARE(pane.currentWidget(), second);
        QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({second}));

        pane.setMode(ZzFluentUI::ZzSidePaneMode::Stacked);
        QVERIFY(pane.setWidgetVisible(first, true));
        QVERIFY(pane.setWidgetVisible(second, true));
        QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({first, second}));
        QCOMPARE(pane.panelStack()->visiblePanelCount(), 2);
        QVERIFY(pane.panelStack()->setPanelSizes({120, 200}));
        pane.setCollapsed(true);
        pane.setCollapsed(false);
        QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({first, second}));
        QCOMPARE(pane.panelStack()->panelSizes(), QList<int>({120, 200}));
        pane.setMode(ZzFluentUI::ZzSidePaneMode::Single);
        pane.setMode(ZzFluentUI::ZzSidePaneMode::Stacked);
        QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({first, second}));
        QVERIFY(pane.setWidgetVisible(first, false));
        QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({second}));
        pane.setMode(ZzFluentUI::ZzSidePaneMode::Single);
        pane.setMode(ZzFluentUI::ZzSidePaneMode::Stacked);
        QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({second}));
    }

    void collapsesAndRestoresTheLastExpandedClampedWidth()
    {
        ZzFluentUI::ZzSidePane pane;
        pane.setMinimumPaneWidth(180);
        pane.setMaximumPaneWidth(360);
        pane.setPaneWidth(500);
        QCOMPARE(pane.paneWidth(), 360);
        QCOMPARE(pane.lastExpandedWidth(), 360);
        pane.setPaneWidth(20);
        QCOMPARE(pane.paneWidth(), 180);
        QCOMPARE(pane.lastExpandedWidth(), 180);

        pane.setPaneWidth(280);
        pane.setCollapsed(true);
        QVERIFY(pane.isCollapsed());
        QVERIFY(!pane.isVisible());
        QCOMPARE(pane.lastExpandedWidth(), 280);
        pane.setCollapsed(false);
        QVERIFY(!pane.isCollapsed());
        QCOMPARE(pane.paneWidth(), 280);
        QCOMPARE(pane.width(), 280);
    }

    void positionsTheFourPixelHandleOnThePhysicalOuterEdge()
    {
        ZzFluentUI::ZzSidePane pane;
        pane.resize(280, 240);
        pane.show();
        QCoreApplication::processEvents();
        QWidget *handle = pane.findChild<QWidget *>(
            QStringLiteral("zzSidePaneResizeHandle"));
        QVERIFY(handle != nullptr);
        QCOMPARE(handle->width(), 4);
        QCOMPARE(handle->x(), pane.width() - handle->width());

        pane.setEdge(ZzFluentUI::ZzSidePaneEdge::Right);
        QCoreApplication::processEvents();
        QCOMPARE(handle->x(), 0);
    }

    // 此测试捕获偏好宽度被实现成硬约束，导致窄宿主中的相邻工作区覆盖侧栏标题按钮。
    void shrinksBelowPreferredWidthInsideAConstrainedHost()
    {
        QWidget host;
        host.setFixedSize(300, 240);
        auto *pane = new ZzFluentUI::ZzSidePane(
            ZzFluentUI::ZzSidePaneEdge::Left, &host);
        pane->setMinimumPaneWidth(120);
        pane->setPaneWidth(280);
        QVERIFY(pane->addWidget(
            new QLabel(QStringLiteral("Page")),
            QStringLiteral("Explorer")));
        auto *workspace = new QWidget(&host);
        workspace->setMinimumWidth(180);
        auto *layout = new QHBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(pane);
        layout->addWidget(workspace, 1);

        host.show();
        QCoreApplication::processEvents();

        QCOMPARE(pane->paneWidth(), 280);
        QVERIFY(pane->width() >= pane->minimumPaneWidth());
        QVERIFY(pane->width() < pane->paneWidth());
        QCOMPARE(workspace->x(), pane->width());
        QVERIFY(workspace->geometry().right() < host.width());
        auto *closeButton = pane->findChild<QWidget *>(
            QStringLiteral("zzPanelStackCloseButton"));
        QVERIFY(closeButton != nullptr);
        if (closeButton == nullptr) {
            return;
        }
        const QRect closeRect(
            closeButton->mapTo(pane, QPoint{}), closeButton->size());
        QVERIFY(pane->rect().contains(closeRect));
    }

    // 此测试捕获布局长期采用最小宽度，而不是用户配置的展开宽度作为首选值。
    void usesPreferredWidthInsideARoomyHost()
    {
        QWidget host;
        host.setFixedSize(600, 240);
        auto *pane = new ZzFluentUI::ZzSidePane(
            ZzFluentUI::ZzSidePaneEdge::Left, &host);
        pane->setMinimumPaneWidth(120);
        pane->setPaneWidth(280);
        QVERIFY(pane->addWidget(
            new QLabel(QStringLiteral("Page")),
            QStringLiteral("Explorer")));
        auto *workspace = new QWidget(&host);
        workspace->setMinimumWidth(180);
        auto *layout = new QHBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(pane);
        layout->addWidget(workspace, 1);

        host.show();
        QCoreApplication::processEvents();

        QCOMPARE(pane->width(), pane->paneWidth());
        QCOMPARE(workspace->x(), pane->paneWidth());
    }

    // 页面可由外部 delete，面板只能丢弃观察状态，不能保留悬空指针。
    void observesExternallyDestroyedRegisteredPages()
    {
        ZzFluentUI::ZzSidePane pane;
        auto *first = new QLabel(QStringLiteral("First"));
        QVERIFY(pane.addWidget(first, QStringLiteral("First")));
        auto *second = new QLabel(QStringLiteral("Second"));
        QVERIFY(pane.addWidget(second, QStringLiteral("Second")));
        QSignalSpy currentSpy(
            &pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged);
        QVERIFY(!currentSpy.isValid() || currentSpy.isEmpty());
        QPointer<QWidget> guard(first);

        delete first;
        QCoreApplication::processEvents();

        QVERIFY(guard.isNull());
        QCOMPARE(pane.currentWidget(), second);
        QCOMPARE(pane.pageCount(), 1);
        QCOMPARE(currentSpy.count(), 0);

        QVERIFY(pane.setCurrentWidget(second));
        QCOMPARE(currentSpy.count(), 0);
    }

    void rejectsAddWhenCurrentChangedDeletesTheNewPageSynchronously()
    {
        ZzFluentUI::ZzSidePane pane;
        auto *page = new QLabel(QStringLiteral("Ephemeral"));
        QPointer<QWidget> pageGuard(page);
        QObject::connect(
            &pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            &pane, [&pageGuard](QWidget *current) {
                if (current == pageGuard.data()) {
                    delete current;
                }
            });

        QVERIFY(!pane.addWidget(page, QStringLiteral("Ephemeral")));
        QVERIFY(pageGuard.isNull());
        QCOMPARE(pane.pageCount(), 0);
        QCOMPARE(pane.currentWidget(), nullptr);
    }

    void rejectsTakeWhenCurrentChangedDeletesTheRemovedPageSynchronously()
    {
        ZzFluentUI::ZzSidePane pane;
        auto *first = new QLabel(QStringLiteral("First"));
        QVERIFY(pane.addWidget(first, QStringLiteral("First")));
        auto *second = new QLabel(QStringLiteral("Second"));
        QVERIFY(pane.addWidget(second, QStringLiteral("Second")));
        QVERIFY(pane.setCurrentWidget(first));
        QPointer<QWidget> firstGuard(first);
        QObject::connect(
            &pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            &pane, [&firstGuard](QWidget *) {
                if (firstGuard != nullptr) {
                    delete firstGuard.data();
                }
            });

        QCOMPARE(pane.takeWidget(first), nullptr);
        QVERIFY(firstGuard.isNull());
        QCOMPARE(pane.pageCount(), 1);
        QCOMPARE(pane.currentWidget(), second);
    }

    void notifiesWhenMinimumWidthRaisesMaximumWidth()
    {
        ZzFluentUI::ZzSidePane pane;
        pane.setMaximumPaneWidth(120);
        QSignalSpy minimumSpy(
            &pane, &ZzFluentUI::ZzSidePane::minimumPaneWidthChanged);
        QSignalSpy maximumSpy(
            &pane, &ZzFluentUI::ZzSidePane::maximumPaneWidthChanged);

        pane.setMinimumPaneWidth(180);

        QCOMPARE(pane.minimumPaneWidth(), 180);
        QCOMPARE(pane.maximumPaneWidth(), 180);
        QCOMPARE(minimumSpy.count(), 1);
        QCOMPARE(maximumSpy.count(), 1);
        QCOMPARE(maximumSpy.at(0).at(0).toInt(), 180);
    }
};

QTEST_MAIN(ZzSidePaneTest)

#include "ZzSidePaneTest.moc"
