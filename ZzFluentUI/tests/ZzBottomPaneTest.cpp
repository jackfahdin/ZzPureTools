#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtGui/QColor>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/** @brief 验证中央底部工具区的所有权、切换、折叠和高度契约。 */
class ZzBottomPaneTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // 此测试捕获折叠时重建内容或丢失当前工具和最近展开高度的回归。
    void keepsCurrentToolAndHeightAcrossCollapse()
    {
        ZzFluentUI::ZzBottomPane pane;
        auto *terminal = new QWidget;
        auto *problems = new QWidget;
        QVERIFY(pane.addWidget(terminal, QStringLiteral("Terminal")));
        QVERIFY(pane.addWidget(problems, QStringLiteral("Problems")));
        QVERIFY(pane.setCurrentWidget(terminal));
        pane.setPaneHeight(260);
        pane.setCollapsed(true);
        QCOMPARE(pane.currentWidget(), terminal);
        QCOMPARE(pane.lastExpandedHeight(), 260);
        pane.setCollapsed(false);
        QCOMPARE(pane.paneHeight(), 260);
        QCOMPARE(pane.takeWidget(problems), problems);
        delete problems;
    }

    // 此测试捕获错误接管有父页面或向同一内容重复创建工具项的回归。
    void rejectsForeignAndDuplicateWidgetsWithoutChangingOwnership()
    {
        ZzFluentUI::ZzBottomPane pane;
        QWidget foreignOwner;
        auto *foreign = new QWidget(&foreignOwner);
        QVERIFY(!pane.addWidget(foreign, QStringLiteral("Foreign")));
        QCOMPARE(foreign->parentWidget(), &foreignOwner);

        auto *tool = new QWidget;
        QVERIFY(pane.addWidget(tool, QStringLiteral("Terminal")));
        QCOMPARE(pane.widgetCount(), 1);
        QVERIFY(!pane.addWidget(tool, QStringLiteral("Duplicate")));
        QCOMPARE(pane.widgetCount(), 1);
        QCOMPARE(pane.currentWidget(), tool);
    }

    // 此测试捕获最小和最大高度未约束当前值及折叠后值的回归。
    void clampsMinimumAndMaximumPaneHeights()
    {
        ZzFluentUI::ZzBottomPane pane;
        pane.setMinimumPaneHeight(180);
        pane.setMaximumPaneHeight(360);
        pane.setPaneHeight(500);
        QCOMPARE(pane.paneHeight(), 360);
        pane.setPaneHeight(20);
        QCOMPARE(pane.paneHeight(), 180);
        pane.setCollapsed(true);
        QCOMPARE(pane.lastExpandedHeight(), 180);
        pane.setMaximumPaneHeight(220);
        QCOMPARE(pane.lastExpandedHeight(), 180);
        pane.setMinimumPaneHeight(240);
        QCOMPARE(pane.minimumPaneHeight(), 240);
        QCOMPARE(pane.maximumPaneHeight(), 240);
        QCOMPARE(pane.lastExpandedHeight(), 240);
    }

    // 此测试捕获把手和初始高度脱离现有视觉 metric token 的回归。
    void derivesInitialDimensionsFromExistingMetricTokens()
    {
        ZzFluentUI::ZzBottomPane pane;
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::Light, QColor(), 1, false);
        const int headerHeight = qRound(
            snapshot.metric(ZzFluentUI::ZzMetricToken::BottomPaneHeaderHeight));
        const int handleHeight = qRound(
            snapshot.metric(ZzFluentUI::ZzMetricToken::PanelSplitterExtent));
        const int defaultHeight = qRound(
            snapshot.metric(ZzFluentUI::ZzMetricToken::DrawerDefaultWidth));
        const int maximumPaneHeight = qRound(
            snapshot.metric(ZzFluentUI::ZzMetricToken::DialogMaxWidth));
        auto *handle = pane.findChild<QWidget *>(
            QStringLiteral("zzBottomPaneResizeHandle"));
        QVERIFY(handle != nullptr);
        if (handle == nullptr) {
            return;
        }
        QCOMPARE(handle->height(), handleHeight);
        QCOMPARE(pane.minimumPaneHeight(), headerHeight + handleHeight);
        QCOMPARE(pane.paneHeight(), defaultHeight);
        QCOMPARE(pane.maximumPaneHeight(), maximumPaneHeight);
    }

    // 此测试捕获把手宽度或向上拖动时高度计算错误的回归。
    void resizesThroughTheFixedFourPixelHandle()
    {
        ZzFluentUI::ZzBottomPane pane;
        pane.setMinimumPaneHeight(120);
        pane.setMaximumPaneHeight(400);
        pane.setPaneHeight(240);
        pane.resize(640, 240);
        pane.show();
        QCoreApplication::processEvents();
        auto *handle = pane.findChild<QWidget *>(
            QStringLiteral("zzBottomPaneResizeHandle"));
        QVERIFY(handle != nullptr);
        if (handle == nullptr) {
            return;
        }
        QCOMPARE(handle->height(), 4);
        QCOMPARE(handle->y(), 0);
        const QPoint start = handle->rect().center();
        QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(handle, start - QPoint(0, 80));
        QTest::mouseRelease(
            handle, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 80));
        QCOMPARE(pane.paneHeight(), 320);

        QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(handle, start - QPoint(0, 800));
        QTest::mouseRelease(
            handle, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 800));
        QCOMPARE(pane.paneHeight(), pane.maximumPaneHeight());

        QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(handle, start + QPoint(0, 800));
        QTest::mouseRelease(
            handle, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, 800));
        QCOMPARE(pane.paneHeight(), pane.minimumPaneHeight());
    }

    // 此测试捕获关闭按钮主动删除、隐藏或接管当前业务页面的回归。
    void closeButtonOnlyRequestsClosingTheCurrentTool()
    {
        ZzFluentUI::ZzBottomPane pane;
        auto *terminal = new QWidget;
        auto *problems = new QWidget;
        QVERIFY(pane.addWidget(terminal, QStringLiteral("Terminal")));
        QVERIFY(pane.addWidget(problems, QStringLiteral("Problems")));
        QVERIFY(pane.setCurrentWidget(terminal));
        QSignalSpy closeSpy(
            &pane, &ZzFluentUI::ZzBottomPane::widgetCloseRequested);
        auto *closeButton = pane.findChild<QToolButton *>(
            QStringLiteral("zzBottomPaneCloseButton"));
        QVERIFY(closeButton != nullptr);
        if (closeButton == nullptr) {
            return;
        }
        QTest::mouseClick(closeButton, Qt::LeftButton);
        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(closeSpy.at(0).at(0).value<QWidget *>(), terminal);
        QCOMPARE(pane.currentWidget(), terminal);
        QCOMPARE(pane.widgetCount(), 2);
    }

    // 此测试捕获唯一当前工具被外部销毁后遗漏 nullptr 状态通知的回归。
    void notifiesWhenTheOnlyCurrentToolIsDestroyedExternally()
    {
        ZzFluentUI::ZzBottomPane pane;
        auto *terminal = new QWidget;
        QVERIFY(pane.addWidget(terminal, QStringLiteral("Terminal")));
        QSignalSpy currentSpy(
            &pane, &ZzFluentUI::ZzBottomPane::currentWidgetChanged);

        delete terminal;
        QCoreApplication::processEvents();

        QCOMPARE(pane.currentWidget(), nullptr);
        QCOMPARE(currentSpy.count(), 1);
        QCOMPARE(currentSpy.at(0).at(0).value<QWidget *>(), nullptr);
    }

    // 此测试捕获首次添加时先发出不完整当前工具状态的回归。
    void notifiesTheFirstCurrentToolOnlyAfterRegistrationIsConsistent()
    {
        ZzFluentUI::ZzBottomPane pane;
        auto *terminal = new QWidget;
        int notifiedCount = -1;
        bool couldSelectNotifiedWidget = false;
        QObject::connect(
            &pane,
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            &pane,
            [&pane, &notifiedCount, &couldSelectNotifiedWidget](QWidget *widget) {
                notifiedCount = pane.widgetCount();
                couldSelectNotifiedWidget = pane.setCurrentWidget(widget);
            });

        QVERIFY(pane.addWidget(terminal, QStringLiteral("Terminal")));

        QCOMPARE(notifiedCount, 1);
        QVERIFY(couldSelectNotifiedWidget);
    }

    // 此测试捕获外部删除后保留悬空映射或遗留 Pivot 项的回归。
    void observesExternallyDestroyedTools()
    {
        ZzFluentUI::ZzBottomPane pane;
        auto *terminal = new QWidget;
        auto *problems = new QWidget;
        auto *output = new QWidget;
        QVERIFY(pane.addWidget(terminal, QStringLiteral("Terminal")));
        QVERIFY(pane.addWidget(problems, QStringLiteral("Problems")));
        QVERIFY(pane.addWidget(output, QStringLiteral("Output")));
        QVERIFY(pane.setCurrentWidget(output));
        QPointer<QWidget> guard(problems);
        delete problems;
        QCoreApplication::processEvents();
        QVERIFY(guard.isNull());
        QCOMPARE(pane.currentWidget(), output);
        QCOMPARE(pane.widgetCount(), 2);
        auto *pivot = pane.findChild<ZzFluentUI::ZzPivot *>(
            QStringLiteral("zzBottomPanePivot"));
        QVERIFY(pivot != nullptr);
        if (pivot == nullptr) {
            return;
        }
        QCOMPARE(pivot->count(), 2);
        QCOMPARE(pivot->currentIndex(), 1);
        QVERIFY(pane.setCurrentWidget(terminal));
        QVERIFY(pane.setCurrentWidget(output));
    }

    // 此测试捕获 Pivot 交互未同步到真实工具堆栈或控件缺失无障碍名称的回归。
    void switchesToolsThroughPivotKeyboardWithAccessibleControls()
    {
        ZzFluentUI::ZzBottomPane pane;
        auto *terminal = new QWidget;
        auto *problems = new QWidget;
        QVERIFY(pane.addWidget(terminal, QStringLiteral("Terminal")));
        QVERIFY(pane.addWidget(problems, QStringLiteral("Problems")));
        pane.resize(640, 240);
        pane.show();
        QCoreApplication::processEvents();
        auto *pivot = pane.findChild<ZzFluentUI::ZzPivot *>(
            QStringLiteral("zzBottomPanePivot"));
        auto *closeButton = pane.findChild<QToolButton *>(
            QStringLiteral("zzBottomPaneCloseButton"));
        auto *handle = pane.findChild<QWidget *>(
            QStringLiteral("zzBottomPaneResizeHandle"));
        QVERIFY(pivot != nullptr);
        QVERIFY(closeButton != nullptr);
        QVERIFY(handle != nullptr);
        if (pivot == nullptr || closeButton == nullptr || handle == nullptr) {
            return;
        }
        QVERIFY(!pivot->accessibleName().isEmpty());
        QVERIFY(!closeButton->accessibleName().isEmpty());
        QVERIFY(!handle->accessibleName().isEmpty());
        pivot->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(pivot, Qt::Key_Left);
        QCOMPARE(pane.currentWidget(), terminal);
    }

    // 此测试捕获每次折叠、恢复时创建额外 QObject 的回归。
    void keepsTheFixedObjectBudgetAcrossRepeatedCollapse()
    {
        QWidget host;
        ZzFluentUI::ZzBottomPane pane(&host);
        QVERIFY(pane.addWidget(new QLabel(QStringLiteral("Terminal")),
            QStringLiteral("Terminal")));
        const qsizetype objectCount = pane.findChildren<QObject *>().size();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            pane.setCollapsed(true);
            pane.setCollapsed(false);
        }
        QCOMPARE(pane.findChildren<QObject *>().size(), objectCount);
        QVERIFY(pane.findChildren<QDockWidget *>().isEmpty());
        QVERIFY(!pane.isWindow());
    }
};

QTEST_MAIN(ZzBottomPaneTest)

#include "ZzBottomPaneTest.moc"
