#include <memory>

#include <QtCore/QCoreApplication>
#include <QtTest/QTest>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

/** @brief 验证 Dock Panel 保留 Qt 原生停靠协议和内容转移语义。 */
class ZzDockPanelTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesNativeDockConfigurationAndToggleAction()
    {
        ZzFluentUI::ZzDockPanel panel(QStringLiteral("Terminal"));
        panel.setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        panel.setFeatures(
            QDockWidget::DockWidgetClosable
            | QDockWidget::DockWidgetMovable);

        QCOMPARE(
            panel.allowedAreas(),
            Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        QCOMPARE(
            panel.features(),
            QDockWidget::DockWidgetClosable
                | QDockWidget::DockWidgetMovable);
        QCOMPARE(panel.toggleViewAction()->text(), QStringLiteral("Terminal"));
        QVERIFY(panel.toggleViewAction()->isCheckable());

        panel.hide();
        QVERIFY(!panel.toggleViewAction()->isChecked());
        panel.toggleViewAction()->trigger();
        QVERIFY(panel.isVisible());
        QVERIFY(panel.toggleViewAction()->isChecked());
    }

    void followsFeatureChangesAndProvidesAccessibleTitleControls()
    {
        QMainWindow host;
        auto *panel = new ZzFluentUI::ZzDockPanel(
            QStringLiteral("Properties"), &host);
        host.addDockWidget(Qt::LeftDockWidgetArea, panel);
        panel->setIconDescriptor(
            ZzFluentUI::ZzIconDescriptor::fromFontIcon(
                ZzFluentUI::ZzFontIcon::Folder));
        host.resize(640, 480);
        host.show();
        QCoreApplication::processEvents();

        QWidget *const titleBar = panel->titleBarWidget();
        QVERIFY(titleBar != nullptr);
        QVERIFY(!titleBar->accessibleName().isEmpty());
        auto *floatButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzDockPanelFloatButton"));
        auto *closeButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzDockPanelCloseButton"));
        QVERIFY(floatButton != nullptr);
        QVERIFY(closeButton != nullptr);
        QVERIFY(!floatButton->accessibleName().isEmpty());
        QVERIFY(!closeButton->accessibleName().isEmpty());
        QVERIFY(floatButton->isVisible());
        QVERIFY(closeButton->isVisible());

        panel->setFeatures(QDockWidget::DockWidgetMovable);
        QCoreApplication::processEvents();
        QVERIFY(floatButton->isHidden());
        QVERIFY(closeButton->isHidden());
    }

    void usesQtFloatingAndRedockingProtocol()
    {
        QMainWindow host;
        auto *panel = new ZzFluentUI::ZzDockPanel(
            QStringLiteral("Output"), &host);
        panel->setObjectName(QStringLiteral("outputDock"));
        host.addDockWidget(Qt::LeftDockWidgetArea, panel);
        host.resize(640, 480);
        host.show();
        QCoreApplication::processEvents();

        QCOMPARE(host.dockWidgetArea(panel), Qt::LeftDockWidgetArea);
        QVERIFY(!panel->isFloating());
        panel->setFloating(true);
        QTRY_VERIFY(panel->isFloating());

        host.addDockWidget(Qt::RightDockWidgetArea, panel);
        panel->setFloating(false);
        QTRY_VERIFY(!panel->isFloating());
        QCOMPARE(host.dockWidgetArea(panel), Qt::RightDockWidgetArea);
    }

    void returnsContentWithoutDeletingOrRetainingIt()
    {
        ZzFluentUI::ZzDockPanel panel(QStringLiteral("Log"));
        auto content = std::make_unique<QWidget>();
        QWidget *const rawContent = content.get();
        panel.setWidget(content.release());
        QCOMPARE(panel.widget(), rawContent);
        QCOMPARE(rawContent->parentWidget(), &panel);

        std::unique_ptr<QWidget> returned(panel.takeContentWidget());

        QCOMPARE(returned.get(), rawContent);
        QCOMPARE(panel.widget(), nullptr);
        QCOMPARE(returned->parentWidget(), nullptr);
    }
};

QTEST_MAIN(ZzDockPanelTest)
#include "ZzDockPanelTest.moc"
