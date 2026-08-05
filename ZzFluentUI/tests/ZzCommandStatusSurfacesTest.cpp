#include <array>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QAction>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSizeGrip>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 暴露标准工具按钮 style option，测试不读取 Qt 私有状态。 */
class ZzToolButtonProbe final : public QToolButton
{
public:
    /** @brief 返回由 QToolButton 公共状态初始化的绘制 option。 */
    [[nodiscard]] QStyleOptionToolButton styleOption() const
    {
        QStyleOptionToolButton option;
        initStyleOption(&option);
        return option;
    }
};

/** @brief 判断图像是否包含接近目标值的不透明像素。 */
bool zzContainsColor(const QImage &image, const QColor &expected)
{
    constexpr int tolerance = 8;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            if (actual.alpha() > 0
                && qAbs(actual.red() - expected.red()) <= tolerance
                && qAbs(actual.green() - expected.green()) <= tolerance
                && qAbs(actual.blue() - expected.blue()) <= tolerance) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 统计图像中的非透明像素。 */
int zzOpaquePixelCount(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() > 0) {
                ++count;
            }
        }
    }
    return count;
}

/** @brief 将一个标准 primitive 绘制到透明图像。 */
template<typename ZzOption>
QImage zzRenderPrimitive(
    ZzFluentUI::ZzFluentStyle *style,
    QStyle::PrimitiveElement element,
    const ZzOption &option)
{
    QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    style->drawPrimitive(element, &option, &painter);
    painter.end();
    return image;
}

/** @brief 将工具栏 control 绘制到透明图像。 */
QImage zzRenderToolBar(
    ZzFluentUI::ZzFluentStyle *style,
    const QStyleOptionToolBar &option)
{
    QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    style->drawControl(QStyle::CE_ToolBar, &option, &painter);
    painter.end();
    return image;
}

/** @brief 刷新布局和延迟销毁，保证对象统计确定。 */
void zzFlushEvents()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

} // namespace

/** @brief 验证标准命令与状态控件的 Fluent 绘制和 Qt 原生协议。 */
class ZzCommandStatusSurfacesTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableMetricsWithoutShrinkingContent()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzToolButtonProbe button;
        button.setStyle(&style);
        button.setText(QStringLiteral("A deliberately long command"));
        button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        const QStyleOptionToolButton option = button.styleOption();
        const QSize contents(260, 72);
        const QSize base = style.baseStyle()->sizeFromContents(
            QStyle::CT_ToolButton,
            &option,
            contents,
            &button);
        const QSize fluent = style.sizeFromContents(
            QStyle::CT_ToolButton,
            &option,
            contents,
            &button);

        QVERIFY(fluent.width() >= base.width());
        QVERIFY(fluent.height() >= base.height());
        QVERIFY(fluent.width() >= 32);
        QVERIFY(fluent.height() >= 32);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolBarFrameWidth), 0);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolBarHandleExtent), 10);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolBarItemSpacing), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolBarItemMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolBarSeparatorExtent), 8);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolBarExtensionExtent), 28);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolBarIconSize), 20);
    }

    void preservesToolbarActionAndOverflowContracts()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMainWindow window;
        window.setStyle(&style);
        window.setCentralWidget(new QLabel(QStringLiteral("Workspace")));
        auto *toolBar = new QToolBar(QStringLiteral("Commands"), &window);
        toolBar->setStyle(&style);
        toolBar->setAllowedAreas(
            Qt::TopToolBarArea | Qt::LeftToolBarArea);
        toolBar->setMovable(true);
        toolBar->setFloatable(true);
        window.addToolBar(Qt::TopToolBarArea, toolBar);

        QAction *primary = toolBar->addAction(QStringLiteral("Build"));
        primary->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
        primary->setCheckable(true);
        QSignalSpy triggered(primary, &QAction::triggered);
        toolBar->addSeparator();
        auto *custom = new QLabel(QStringLiteral("Local"));
        QAction *widgetAction = toolBar->addWidget(custom);
        std::array<QAction *, 14> extraActions{};
        for (std::size_t index = 0; index < extraActions.size(); ++index) {
            extraActions.at(index) = toolBar->addAction(
                QStringLiteral("Command %1").arg(index + 1));
        }

        window.resize(240, 180);
        window.show();
        zzFlushEvents();
        QVERIFY(toolBar->isVisible());
        QVERIFY(toolBar->isAreaAllowed(Qt::TopToolBarArea));
        QVERIFY(toolBar->isAreaAllowed(Qt::LeftToolBarArea));
        QVERIFY(!toolBar->isAreaAllowed(Qt::RightToolBarArea));
        QVERIFY(toolBar->isMovable());
        QVERIFY(toolBar->isFloatable());
        QCOMPARE(toolBar->orientation(), Qt::Horizontal);
        QCOMPARE(toolBar->widgetForAction(widgetAction), custom);
        QVERIFY(toolBar->widgetForAction(primary) != nullptr);
        QVERIFY(toolBar->actionGeometry(primary).isValid());

        primary->trigger();
        QCOMPARE(triggered.count(), 1);
        QVERIFY(primary->isChecked());
        QCOMPARE(primary->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_B));

        int hiddenActions = 0;
        for (QAction *action : extraActions) {
            QWidget *widget = toolBar->widgetForAction(action);
            if (widget != nullptr && !widget->isVisible()) {
                ++hiddenActions;
            }
        }
        QVERIFY(hiddenActions > 0);

        QAction *visibilityAction = toolBar->toggleViewAction();
        QVERIFY(visibilityAction != nullptr);
        visibilityAction->trigger();
        QVERIFY(!toolBar->isVisible());
        visibilityAction->trigger();
        QVERIFY(toolBar->isVisible());

        window.addToolBar(Qt::LeftToolBarArea, toolBar);
        zzFlushEvents();
        QCOMPARE(window.toolBarArea(toolBar), Qt::LeftToolBarArea);
        QCOMPARE(toolBar->orientation(), Qt::Vertical);
    }

    void preservesToolButtonKeyboardMenuAndAccessibility()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzToolButtonProbe button;
        button.setStyle(&style);
        button.setAccessibleName(QStringLiteral("Deploy command"));
        auto *action = new QAction(QStringLiteral("Deploy"), &button);
        action->setCheckable(true);
        button.setDefaultAction(action);
        QSignalSpy triggered(action, &QAction::triggered);
        auto *menu = new QMenu(&button);
        QAction *local = menu->addAction(QStringLiteral("Local"));
        QSignalSpy localTriggered(local, &QAction::triggered);
        button.setMenu(menu);
        button.setPopupMode(QToolButton::MenuButtonPopup);
        button.resize(132, 40);
        button.show();
        button.setFocus(Qt::TabFocusReason);
        zzFlushEvents();

        QTest::keyClick(&button, Qt::Key_Space);
        QCOMPARE(triggered.count(), 1);
        QVERIFY(action->isChecked());
        QVERIFY(button.hasFocus());

        const QStyleOptionToolButton option = button.styleOption();
        const QRect mainRect = style.subControlRect(
            QStyle::CC_ToolButton,
            &option,
            QStyle::SC_ToolButton,
            &button);
        const QRect menuRect = style.subControlRect(
            QStyle::CC_ToolButton,
            &option,
            QStyle::SC_ToolButtonMenu,
            &button);
        QVERIFY(button.rect().contains(mainRect));
        QVERIFY(button.rect().contains(menuRect));
        QVERIFY(!menuRect.isEmpty());

        QTimer::singleShot(0, menu, [local, menu] {
            local->trigger();
            menu->close();
        });
        button.showMenu();
        QCOMPARE(localTriggered.count(), 1);
        QVERIFY(!menu->isVisible());

        for (const Qt::ToolButtonStyle buttonStyle : {
                 Qt::ToolButtonIconOnly,
                 Qt::ToolButtonTextOnly,
                 Qt::ToolButtonTextBesideIcon,
                 Qt::ToolButtonTextUnderIcon}) {
            button.setToolButtonStyle(buttonStyle);
            QVERIFY(button.sizeHint().width() > 0);
            QVERIFY(button.sizeHint().height() >= 32);
        }

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&button);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::ButtonMenu);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Deploy command"));
        QVERIFY(interface->actionInterface() != nullptr);
    }

    void preservesStatusMessageWidgetsAndSizeGrip()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMainWindow window;
        window.setStyle(&style);
        auto *status = new QStatusBar(&window);
        status->setStyle(&style);
        status->setAccessibleName(QStringLiteral("Application status"));
        window.setStatusBar(status);
        auto *normal = new QLabel(QStringLiteral("Ready"));
        auto *permanent = new QLabel(QStringLiteral("Local"));
        status->addWidget(normal, 1);
        status->addPermanentWidget(permanent);
        QSignalSpy messages(status, &QStatusBar::messageChanged);
        window.resize(480, 180);
        window.show();
        zzFlushEvents();

        QVERIFY(normal->isVisible());
        QVERIFY(permanent->isVisible());
        QVERIFY(status->isSizeGripEnabled());
        QVERIFY(status->findChild<QSizeGrip *>() != nullptr);

        status->showMessage(QStringLiteral("Synchronizing"), 0);
        zzFlushEvents();
        QCOMPARE(status->currentMessage(), QStringLiteral("Synchronizing"));
        QVERIFY(!normal->isVisible());
        QVERIFY(permanent->isVisible());
        QCOMPARE(messages.count(), 1);

        status->clearMessage();
        zzFlushEvents();
        QVERIFY(status->currentMessage().isEmpty());
        QVERIFY(normal->isVisible());
        QCOMPARE(messages.count(), 2);

        status->showMessage(QStringLiteral("Transient"), 20);
        QTest::qWait(50);
        zzFlushEvents();
        QVERIFY(status->currentMessage().isEmpty());
        QCOMPARE(messages.count(), 4);

        status->setSizeGripEnabled(false);
        QVERIFY(!status->isSizeGripEnabled());
        status->setSizeGripEnabled(true);
        zzFlushEvents();
        QVERIFY(status->isSizeGripEnabled());
        QVERIFY(status->findChild<QSizeGrip *>() != nullptr);

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(status);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::StatusBar);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Application status"));
    }

    void rendersThemeTokensForEverySurface()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const auto snapshot = controller.snapshot();

        QStyleOption normalButton;
        normalButton.rect = QRect(0, 0, 48, 32);
        normalButton.state = QStyle::State_Enabled;
        QCOMPARE(
            zzOpaquePixelCount(zzRenderPrimitive(
                &style,
                QStyle::PE_PanelButtonTool,
                normalButton)),
            0);

        QStyleOption hoveredButton = normalButton;
        hoveredButton.state |= QStyle::State_MouseOver;
        QVERIFY(zzContainsColor(
            zzRenderPrimitive(
                &style,
                QStyle::PE_PanelButtonTool,
                hoveredButton),
            snapshot->color(ZzFluentUI::ZzColorToken::ControlFillHover)));

        QStyleOption pressedButton = normalButton;
        pressedButton.state |= QStyle::State_Sunken;
        QVERIFY(zzContainsColor(
            zzRenderPrimitive(
                &style,
                QStyle::PE_PanelButtonTool,
                pressedButton),
            snapshot->color(
                ZzFluentUI::ZzColorToken::ControlFillPressed)));

        QStyleOption checkedButton = normalButton;
        checkedButton.state |= QStyle::State_On;
        const QImage checked = zzRenderPrimitive(
            &style,
            QStyle::PE_PanelButtonTool,
            checkedButton);
        QVERIFY(zzContainsColor(
            checked,
            snapshot->color(ZzFluentUI::ZzColorToken::ControlFill)));
        QVERIFY(zzContainsColor(
            checked,
            snapshot->color(ZzFluentUI::ZzColorToken::ControlStroke)));

        QStyleOptionToolBar toolBar;
        toolBar.rect = QRect(0, 0, 120, 36);
        toolBar.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        toolBar.toolBarArea = Qt::TopToolBarArea;
        const QImage toolBarImage = zzRenderToolBar(&style, toolBar);
        QVERIFY(zzContainsColor(
            toolBarImage,
            snapshot->color(ZzFluentUI::ZzColorToken::Surface)));
        QVERIFY(zzContainsColor(
            toolBarImage,
            snapshot->color(ZzFluentUI::ZzColorToken::ControlStroke)));

        QStyleOption status;
        status.rect = QRect(0, 0, 120, 28);
        status.state = QStyle::State_Enabled;
        const QImage statusImage = zzRenderPrimitive(
            &style,
            QStyle::PE_PanelStatusBar,
            status);
        QVERIFY(zzContainsColor(
            statusImage,
            snapshot->color(
                ZzFluentUI::ZzColorToken::SurfaceSecondary)));
        QVERIFY(zzContainsColor(
            statusImage,
            snapshot->color(ZzFluentUI::ZzColorToken::ControlStroke)));

        QStyleOption handle;
        handle.rect = QRect(0, 0, 12, 24);
        handle.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        QVERIFY(zzContainsColor(
            zzRenderPrimitive(
                &style,
                QStyle::PE_IndicatorToolBarHandle,
                handle),
            snapshot->color(ZzFluentUI::ZzColorToken::TextSecondary)));
    }

    void keepsObjectsStableAcrossRepeatedUpdates()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMainWindow window;
        window.setStyle(&style);
        auto *toolBar = new QToolBar(QStringLiteral("Stable"), &window);
        auto *status = new QStatusBar(&window);
        toolBar->setStyle(&style);
        status->setStyle(&style);
        window.addToolBar(toolBar);
        window.setStatusBar(status);
        std::array<QAction *, 8> actions{};
        for (std::size_t index = 0; index < actions.size(); ++index) {
            QAction *action = toolBar->addAction(
                QStringLiteral("Action %1").arg(index + 1));
            action->setCheckable(true);
            actions.at(index) = action;
        }
        status->addWidget(new QLabel(QStringLiteral("Ready")));
        status->showMessage(QStringLiteral("Warm"), 0);
        window.resize(480, 200);
        window.show();
        zzFlushEvents();

        const qsizetype descendants =
            window.findChildren<QObject *>().size();
        const qsizetype animations =
            window.findChildren<QAbstractAnimation *>().size();
        const qsizetype timers = window.findChildren<QTimer *>().size();
        const qsizetype styles = window.findChildren<QStyle *>().size();

        for (int round = 0; round < 1000; ++round) {
            QAction *action = actions.at(
                static_cast<std::size_t>(round) % actions.size());
            action->setChecked(!action->isChecked());
            action->setEnabled((round % 5) != 0);
            toolBar->setOrientation(
                (round % 2) == 0 ? Qt::Horizontal : Qt::Vertical);
            window.setLayoutDirection(
                (round % 2) == 0
                    ? Qt::LeftToRight
                    : Qt::RightToLeft);
            status->showMessage(QString::number(round), 0);
            controller.setMode(
                (round % 3) == 0
                    ? ZzFluentUI::ZzThemeMode::Light
                    : ((round % 3) == 1
                           ? ZzFluentUI::ZzThemeMode::Dark
                           : ZzFluentUI::ZzThemeMode::HighContrast));
        }
        zzFlushEvents();

        QCOMPARE(window.findChildren<QObject *>().size(), descendants);
        QCOMPARE(
            window.findChildren<QAbstractAnimation *>().size(),
            animations);
        QCOMPARE(window.findChildren<QTimer *>().size(), timers);
        QCOMPARE(window.findChildren<QStyle *>().size(), styles);
        QCOMPARE(toolBar->style(), &style);
        QCOMPARE(status->style(), &style);
    }
};

QTEST_MAIN(ZzCommandStatusSurfacesTest)

#include "ZzCommandStatusSurfacesTest.moc"
