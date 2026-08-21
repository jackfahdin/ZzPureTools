#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>

#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>

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
        QCOMPARE(page->parentWidget()->metaObject()->className(), "QStackedWidget");

        std::unique_ptr<QWidget> taken(pane.takeWidget(page));
        QCOMPARE(taken.get(), page);
        QCOMPARE(taken->parentWidget(), nullptr);
        QVERIFY(!pane.setCurrentWidget(page));

        QWidget foreignOwner;
        auto *foreign = new QLabel(QStringLiteral("Foreign"), &foreignOwner);
        QVERIFY(!pane.addWidget(foreign, QStringLiteral("Rejected")));
        QCOMPARE(foreign->parentWidget(), &foreignOwner);
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
