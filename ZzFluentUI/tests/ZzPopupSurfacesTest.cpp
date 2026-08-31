#include <array>
#include <cstddef>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 判断图像区域是否包含接近目标值的不透明像素。 */
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

/** @brief 处理普通事件与延迟销毁，保证对象统计稳定。 */
void zzFlushDeferredObjects()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

/** @brief 通过公开 window type 查找当前可见的标准工具提示。 */
QWidget *zzVisibleToolTipWindow()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget != nullptr && widget->isVisible()
            && widget->windowType() == Qt::ToolTip) {
            return widget;
        }
    }
    return nullptr;
}

/** @brief 将菜单项绘制到透明图像供状态颜色检查。 */
QImage zzRenderMenuItem(
    ZzFluentUI::ZzFluentStyle *style,
    QStyleOptionMenuItem option,
    const QWidget *widget = nullptr)
{
    QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    style->drawControl(
        QStyle::CE_MenuItem,
        &option,
        &painter,
        widget);
    painter.end();
    return image;
}

} // namespace

/** @brief 验证标准菜单、菜单栏和工具提示的 Fluent 绘制契约。 */
class ZzPopupSurfacesTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesBaseMeasurementsAndStableMetrics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);

        QStyleOptionMenuItem item;
        item.initFrom(&menu);
        item.menuItemType = QStyleOptionMenuItem::Normal;
        item.text = QStringLiteral("Open workspace\tCtrl+Shift+O");
        item.maxIconWidth = 20;
        item.reservedShortcutWidth = 96;
        const QSize contents(220, 18);
        const QSize base = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        const QSize fluent = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        QVERIFY(fluent.width() >= base.width() + 12);
        QVERIFY(fluent.height() >= base.height());
        QVERIFY(fluent.height() >= 32);

        item.menuItemType = QStyleOptionMenuItem::DefaultItem;
        const QSize defaultBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        const QSize defaultItem = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        QVERIFY(defaultItem.width() >= defaultBase.width() + 12);

        item.text = QStringLiteral("Export workspace");
        item.reservedShortcutWidth = 0;
        item.menuItemType = QStyleOptionMenuItem::Normal;
        const QSize normalItem = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        item.menuItemType = QStyleOptionMenuItem::SubMenu;
        const QSize submenuBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        const QSize submenuItem = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        QVERIFY(submenuItem.width() >= submenuBase.width());
        QVERIFY(submenuItem.width() >= normalItem.width() + 28);

        item.menuItemType = QStyleOptionMenuItem::Separator;
        item.text.clear();
        const QSize separatorBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            QSize(120, 1),
            &menu);
        const QSize separator = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            QSize(120, 1),
            &menu);
        QVERIFY(separator.width() >= separatorBase.width());
        QVERIFY(separator.height() >= separatorBase.height());
        QVERIFY(separator.height() >= 9);
        QVERIFY(separator.height() < fluent.height());

        QMenuBar menuBar;
        menuBar.setNativeMenuBar(false);
        menuBar.setStyle(&style);
        item.menuItemType = QStyleOptionMenuItem::Normal;
        item.text = QStringLiteral("&File");
        const QSize menuBarBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuBarItem,
            &item,
            QSize(48, 18),
            &menuBar);
        const QSize menuBarItem = style.sizeFromContents(
            QStyle::CT_MenuBarItem,
            &item,
            QSize(48, 18),
            &menuBar);
        QVERIFY(menuBarItem.width() >= menuBarBase.width());
        QVERIFY(menuBarItem.height() >= menuBarBase.height());
        QVERIFY(menuBarItem.height() >= 32);

        QCOMPARE(style.pixelMetric(QStyle::PM_MenuPanelWidth), 1);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuHMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuVMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuBarHMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuBarVMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuBarItemSpacing), 2);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolTipLabelFrameWidth), 8);
    }

    void rendersEveryThemeAndInteractionState()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            const auto snapshot = controller.snapshot();
            const QPalette palette = style.standardPalette();
            const QColor menuStateFill = mode
                == ZzFluentUI::ZzThemeMode::HighContrast
                ? snapshot->color(ZzFluentUI::ZzColorToken::Accent)
                : snapshot->color(ZzFluentUI::ZzColorToken::ControlFillHover);

            QStyleOption panel;
            panel.rect = QRect(0, 0, 220, 140);
            panel.state = QStyle::State_Enabled;
            panel.palette = palette;
            QImage image(panel.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawPrimitive(
                QStyle::PE_PanelMenu,
                &panel,
                &painter,
                &menu);
            painter.end();
            QCOMPARE(
                image.pixelColor(panel.rect.center()),
                snapshot->color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::ControlStroke)));

            QStyleOptionMenuItem item;
            item.rect = QRect(0, 0, 220, 32);
            item.state = QStyle::State_Enabled | QStyle::State_Selected;
            item.palette = palette;
            item.menuItemType = QStyleOptionMenuItem::Normal;
            item.checkType = QStyleOptionMenuItem::NonExclusive;
            item.checked = true;
            item.menuHasCheckableItems = true;
            item.maxIconWidth = 20;
            item.text = QStringLiteral("Checked\tCtrl+C");
            item.reservedShortcutWidth = 64;
            image = zzRenderMenuItem(&style, item, &menu);
            QVERIFY(zzContainsColor(
                image,
                menuStateFill));
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::Accent)));

            item.state = QStyle::State_Enabled | QStyle::State_Sunken;
            item.checked = false;
            image = zzRenderMenuItem(&style, item, &menu);
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::ControlFillPressed)));

            item.state = QStyle::State_None
                | QStyle::State_Selected;
            item.text.clear();
            image = zzRenderMenuItem(&style, item, &menu);
            if (mode != ZzFluentUI::ZzThemeMode::HighContrast) {
                QVERIFY(!zzContainsColor(
                    image,
                    snapshot->color(
                        ZzFluentUI::ZzColorToken::ControlFillHover)));
            }

            item.menuItemType = QStyleOptionMenuItem::Separator;
            item.text.clear();
            item.rect = QRect(0, 0, 220, 9);
            image = zzRenderMenuItem(&style, item, &menu);
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::ControlStroke)));

            QStyleOptionMenuItem menuBarItem;
            menuBarItem.rect = QRect(0, 0, 90, 32);
            menuBarItem.state = QStyle::State_Enabled
                | QStyle::State_Selected;
            menuBarItem.palette = palette;
            menuBarItem.menuItemType = QStyleOptionMenuItem::Normal;
            QImage menuBarImage(
                menuBarItem.rect.size(),
                QImage::Format_ARGB32_Premultiplied);
            menuBarImage.fill(Qt::transparent);
            painter.begin(&menuBarImage);
            style.drawControl(
                QStyle::CE_MenuBarItem,
                &menuBarItem,
                &painter);
            painter.end();
            QVERIFY(zzContainsColor(menuBarImage, menuStateFill));

            QStyleOption toolTip;
            toolTip.rect = QRect(0, 0, 180, 40);
            toolTip.state = QStyle::State_Enabled;
            toolTip.palette = palette;
            QImage toolTipImage(
                toolTip.rect.size(),
                QImage::Format_ARGB32_Premultiplied);
            toolTipImage.fill(Qt::transparent);
            painter.begin(&toolTipImage);
            style.drawPrimitive(
                QStyle::PE_PanelTipLabel,
                &toolTip,
                &painter);
            painter.end();
            QCOMPARE(
                toolTipImage.pixelColor(toolTip.rect.center()),
                snapshot->color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
            QCOMPARE(
                palette.color(QPalette::ToolTipBase),
                snapshot->color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
        }
    }

    void keepsRoundedMenuCornersTransparentAfterEmptyAreaPaint()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);

        QStyleOption panel;
        panel.rect = QRect(0, 0, 220, 140);
        panel.state = QStyle::State_Enabled;
        panel.palette = style.standardPalette();
        QImage image(panel.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawPrimitive(QStyle::PE_PanelMenu, &panel, &painter, &menu);
        style.drawControl(QStyle::CE_MenuEmptyArea, &panel, &painter, &menu);
        painter.end();

        QVERIFY(image.pixelColor(0, 0).alpha() == 0);
        QVERIFY(image.pixelColor(image.width() - 1, 0).alpha() == 0);
        QVERIFY(image.pixelColor(0, image.height() - 1).alpha() == 0);
        QVERIFY(image.pixelColor(image.width() - 1, image.height() - 1).alpha() == 0);
    }

    void mirrorsSubmenuChevronWithoutChangingActionState()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);
        const QColor textColor = style.standardPalette().color(
            QPalette::Text);

        QStyleOptionMenuItem item;
        item.rect = QRect(0, 0, 220, 32);
        item.state = QStyle::State_Enabled;
        item.palette = style.standardPalette();
        item.menuItemType = QStyleOptionMenuItem::SubMenu;
        item.text = QStringLiteral("Export");
        item.direction = Qt::LeftToRight;
        const QImage ltr = zzRenderMenuItem(&style, item, &menu);
        QVERIFY(zzContainsColor(ltr.copy(QRect(192, 6, 24, 20)), textColor));

        item.direction = Qt::RightToLeft;
        const QImage rtl = zzRenderMenuItem(&style, item, &menu);
        QVERIFY(zzContainsColor(rtl.copy(QRect(4, 6, 24, 20)), textColor));

        QAction action(QStringLiteral("Export"), &menu);
        action.setCheckable(true);
        action.setChecked(true);
        action.setData(42);
        action.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
        QSignalSpy toggledSpy(&action, &QAction::toggled);
        QSignalSpy triggeredSpy(&action, &QAction::triggered);
        action.trigger();
        QCOMPARE(action.text(), QStringLiteral("Export"));
        QCOMPARE(action.data().toInt(), 42);
        QCOMPARE(action.shortcut(), QKeySequence(Qt::CTRL | Qt::Key_E));
        QVERIFY(!action.isChecked());
        QCOMPARE(toggledSpy.count(), 1);
        QCOMPARE(triggeredSpy.count(), 1);
    }

    void preservesActionsGroupsAndNativeSignals()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu(QStringLiteral("Workspace"));
        menu.setStyle(&style);
        menu.setToolTipsVisible(true);

        QPixmap iconPixmap(12, 12);
        iconPixmap.fill(Qt::green);
        QAction *open = menu.addAction(
            QIcon(iconPixmap),
            QStringLiteral("&Open"));
        open->setShortcut(QKeySequence::Open);
        open->setData(17);
        open->setToolTip(QStringLiteral("Open workspace"));
        open->setMenuRole(QAction::ApplicationSpecificRole);
        QAction *section = menu.addSection(QStringLiteral("Mode"));
        auto *group = new QActionGroup(&menu);
        group->setExclusive(true);
        QAction *local = menu.addAction(QStringLiteral("&Local"));
        QAction *remote = menu.addAction(QStringLiteral("&Remote"));
        local->setCheckable(true);
        remote->setCheckable(true);
        group->addAction(local);
        group->addAction(remote);
        local->setChecked(true);
        QAction *separator = menu.addSeparator();
        QMenu *submenu = menu.addMenu(QStringLiteral("E&xport"));
        QAction *json = submenu->addAction(QStringLiteral("JSON"));
        QAction *disabled = menu.addAction(QStringLiteral("Unavailable"));
        disabled->setEnabled(false);
        menu.setDefaultAction(open);
        menu.setActiveAction(local);

        QCOMPARE(menu.title(), QStringLiteral("Workspace"));
        QVERIFY(menu.toolTipsVisible());
        QCOMPARE(open->shortcut(), QKeySequence::Open);
        QCOMPARE(open->data().toInt(), 17);
        QCOMPARE(open->toolTip(), QStringLiteral("Open workspace"));
        QCOMPARE(open->menuRole(), QAction::ApplicationSpecificRole);
        QVERIFY(!open->icon().isNull());
        QVERIFY(section->isSeparator());
        QCOMPARE(section->text(), QStringLiteral("Mode"));
        QVERIFY(separator->isSeparator());
        QCOMPARE(QMenu::menuInAction(submenu->menuAction()), submenu);
        QCOMPARE(menu.defaultAction(), open);
        QCOMPARE(menu.activeAction(), local);
        QVERIFY(!disabled->isEnabled());

        QSignalSpy menuTriggeredSpy(&menu, &QMenu::triggered);
        QSignalSpy groupTriggeredSpy(group, &QActionGroup::triggered);
        QSignalSpy remoteToggledSpy(remote, &QAction::toggled);
        remote->trigger();
        QVERIFY(remote->isChecked());
        QVERIFY(!local->isChecked());
        QCOMPARE(menuTriggeredSpy.count(), 1);
        QCOMPARE(groupTriggeredSpy.count(), 1);
        QCOMPARE(remoteToggledSpy.count(), 1);

        QSignalSpy submenuTriggeredSpy(submenu, &QMenu::triggered);
        json->trigger();
        QCOMPARE(submenuTriggeredSpy.count(), 1);
        QCOMPARE(menuTriggeredSpy.count(), 2);
        QSignalSpy disabledTriggeredSpy(disabled, &QAction::triggered);
        disabled->trigger();
        QCOMPARE(disabledTriggeredSpy.count(), 0);
    }

    void preservesKeyboardAndSubmenuNavigation()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);
        QAction *open = menu.addAction(QStringLiteral("&Open"));
        QAction *disabled = menu.addAction(QStringLiteral("Disabled"));
        disabled->setEnabled(false);
        QAction *check = menu.addAction(QStringLiteral("&Check"));
        check->setCheckable(true);
        QMenu *submenu = menu.addMenu(QStringLiteral("E&xport"));
        submenu->setStyle(&style);
        submenu->addAction(QStringLiteral("JSON"));

        menu.show();
        menu.setFocus();
        menu.setActiveAction(open);
        QCoreApplication::processEvents();
        QVERIFY(menu.isVisible());
        QCOMPARE(menu.activeAction(), open);

        QTest::keyClick(&menu, Qt::Key_Down);
        QCOMPARE(menu.activeAction(), check);
        QSignalSpy checkTriggeredSpy(check, &QAction::triggered);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(checkTriggeredSpy.count(), 1);
        QVERIFY(check->isChecked());
        QVERIFY(!menu.isVisible());

        menu.show();
        menu.setActiveAction(submenu->menuAction());
        QCoreApplication::processEvents();
        QTest::keyClick(&menu, Qt::Key_Right);
        ZZ_VERIFY_EVENTUALLY(submenu->isVisible());
        QTest::keyClick(submenu, Qt::Key_Escape);
        ZZ_VERIFY_EVENTUALLY(!submenu->isVisible());
        QTest::keyClick(&menu, Qt::Key_Escape);
        ZZ_VERIFY_EVENTUALLY(!menu.isVisible());

        QSignalSpy openTriggeredSpy(open, &QAction::triggered);
        menu.show();
        menu.setFocus();
        QCoreApplication::processEvents();
#if defined(Q_OS_MACOS)
        menu.setActiveAction(open);
        QTest::keyClick(&menu, Qt::Key_Return);
#else
        QTest::keyClick(&menu, Qt::Key_O);
#endif
        ZZ_COMPARE_EVENTUALLY(openTriggeredSpy.count(), 1);
        QVERIFY(!menu.isVisible());
    }

    void preservesMenuBarNavigationAndAccessibility()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        QMenuBar menuBar(&host);
        menuBar.setNativeMenuBar(false);
        menuBar.setStyle(&style);
        QMenu *fileMenu = menuBar.addMenu(QStringLiteral("&File"));
        QMenu *editMenu = menuBar.addMenu(QStringLiteral("&Edit"));
        fileMenu->setStyle(&style);
        editMenu->setStyle(&style);
        QAction *open = fileMenu->addAction(QStringLiteral("&Open"));
        editMenu->addAction(QStringLiteral("&Undo"));
        QAction *disabled = menuBar.addAction(QStringLiteral("Disabled"));
        disabled->setEnabled(false);
        host.resize(360, 80);
        menuBar.resize(360, 32);
        host.show();
        menuBar.setFocus();
        menuBar.setActiveAction(fileMenu->menuAction());
        QCoreApplication::processEvents();

        QCOMPARE(menuBar.activeAction(), fileMenu->menuAction());
        QTest::keyClick(&menuBar, Qt::Key_Right);
        QCOMPARE(menuBar.activeAction(), editMenu->menuAction());
        QTest::keyClick(&menuBar, Qt::Key_Left);
        QCOMPARE(menuBar.activeAction(), fileMenu->menuAction());
        QTest::keyClick(&menuBar, Qt::Key_Return);
        ZZ_VERIFY_EVENTUALLY(fileMenu->isVisible());
        QTest::keyClick(fileMenu, Qt::Key_Escape);
        ZZ_VERIFY_EVENTUALLY(!fileMenu->isVisible());

        QAccessibleInterface *menuInterface =
            QAccessible::queryAccessibleInterface(fileMenu);
        QAccessibleInterface *barInterface =
            QAccessible::queryAccessibleInterface(&menuBar);
        if (menuInterface == nullptr || barInterface == nullptr) {
            QFAIL("标准菜单或菜单栏缺少无障碍接口");
            return;
        }
        QCOMPARE(menuInterface->role(), QAccessible::PopupMenu);
        QCOMPARE(barInterface->role(), QAccessible::MenuBar);
        QVERIFY(menuInterface->childCount() >= 1);
        QVERIFY(barInterface->childCount() >= 2);
        QAccessibleInterface *openInterface = menuInterface->child(0);
        QVERIFY(openInterface != nullptr);
        QCOMPARE(openInterface->role(), QAccessible::MenuItem);
        QVERIFY(openInterface->text(QAccessible::Name).contains(
            QStringLiteral("Open")));
        QCOMPARE(open->text(), QStringLiteral("&Open"));
        QVERIFY(disabled->isEnabled() == false);
    }

    void preservesStandardToolTipLifecycleAndAccessibility()
    {
        QWidget host;
        host.resize(240, 80);
        host.setToolTip(QStringLiteral("Host tooltip"));
        host.show();
        QCoreApplication::processEvents();
        const QPoint position = host.mapToGlobal(QPoint(20, 20));
        const QString plainText = QStringLiteral(
            "A standard tooltip with enough text to exercise sizing.");
        QToolTip::showText(position, plainText, &host, host.rect(), 2500);
        ZZ_VERIFY_EVENTUALLY(zzVisibleToolTipWindow() != nullptr);
        QWidget *tipWindow = zzVisibleToolTipWindow();
        QVERIFY(tipWindow != nullptr);
        auto *label = qobject_cast<QLabel *>(tipWindow);
        QVERIFY(label != nullptr);
        QCOMPARE(label->text(), plainText);
        QCOMPARE(tipWindow->windowType(), Qt::ToolTip);
        QAccessibleInterface *tipInterface =
            QAccessible::queryAccessibleInterface(tipWindow);
        if (tipInterface == nullptr) {
            QFAIL("标准工具提示缺少无障碍接口");
            return;
        }
        QCOMPARE(tipInterface->role(), QAccessible::ToolTip);
        QVERIFY(tipInterface->text(QAccessible::Name).contains(
            QStringLiteral("standard tooltip")));

        const QString richText = QStringLiteral("<b>Build</b> complete");
        QToolTip::showText(position, richText, &host, host.rect(), 2500);
        ZZ_VERIFY_EVENTUALLY(zzVisibleToolTipWindow() != nullptr);
        QWidget *richTipWindow = zzVisibleToolTipWindow();
        QVERIFY(richTipWindow != nullptr);
        auto *richLabel = qobject_cast<QLabel *>(richTipWindow);
        QVERIFY(richLabel != nullptr);
        QCOMPARE(richLabel->text(), richText);
        QToolTip::hideText();
        ZZ_VERIFY_EVENTUALLY(zzVisibleToolTipWindow() == nullptr);
        QCOMPARE(host.toolTip(), QStringLiteral("Host tooltip"));
    }

    void keepsPerInstanceInfrastructureStable()
    {
        constexpr int menuCount = 50;
        constexpr int menuBarCount = 50;
        constexpr int stateChangeRounds = 1000;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        std::vector<QMenu *> menus;
        std::vector<QMenuBar *> menuBars;
        std::vector<QAction *> primaryActions;
        std::vector<QAction *> secondaryActions;
        menus.reserve(menuCount);
        menuBars.reserve(menuBarCount);
        primaryActions.reserve(menuCount + menuBarCount);
        secondaryActions.reserve(menuCount + menuBarCount);

        for (int index = 0; index < menuCount; ++index) {
            auto *menu = new QMenu(&host);
            menu->setStyle(&style);
            QAction *primary = menu->addAction(QStringLiteral("Primary"));
            QAction *secondary = menu->addAction(QStringLiteral("Secondary"));
            primary->setCheckable(true);
            menu->setDefaultAction(primary);
            menu->setActiveAction(secondary);
            menu->ensurePolished();
            menu->adjustSize();
            menus.push_back(menu);
            primaryActions.push_back(primary);
            secondaryActions.push_back(secondary);
        }
        for (int index = 0; index < menuBarCount; ++index) {
            auto *menuBar = new QMenuBar(&host);
            menuBar->setNativeMenuBar(false);
            menuBar->setStyle(&style);
            QAction *primary = menuBar->addAction(QStringLiteral("Primary"));
            QAction *secondary = menuBar->addAction(QStringLiteral("Secondary"));
            menuBar->setActiveAction(primary);
            menuBar->ensurePolished();
            menuBar->adjustSize();
            menuBars.push_back(menuBar);
            primaryActions.push_back(primary);
            secondaryActions.push_back(secondary);
        }
        zzFlushDeferredObjects();
        const qsizetype initialObjects =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();

        for (int round = 0; round < stateChangeRounds; ++round) {
            const int index = round % (menuCount + menuBarCount);
            QAction *primary = primaryActions[static_cast<std::size_t>(index)];
            QAction *secondary = secondaryActions[static_cast<std::size_t>(index)];
            primary->setEnabled(false);
            primary->setChecked(true);
            primary->setText(QString::number(round));
            secondary->setVisible(false);
            if (index < menuCount) {
                QMenu *menu = menus[static_cast<std::size_t>(index)];
                menu->setActiveAction(primary);
                menu->setDefaultAction(secondary);
                menu->setLayoutDirection(Qt::RightToLeft);
                menu->setLayoutDirection(Qt::LeftToRight);
                menu->setDefaultAction(primary);
                menu->setActiveAction(secondary);
            } else {
                QMenuBar *menuBar = menuBars[
                    static_cast<std::size_t>(index - menuCount)];
                menuBar->setActiveAction(secondary);
                menuBar->setLayoutDirection(Qt::RightToLeft);
                menuBar->setLayoutDirection(Qt::LeftToRight);
                menuBar->setActiveAction(primary);
            }
            secondary->setVisible(true);
            primary->setText(QStringLiteral("Primary"));
            primary->setChecked(false);
            primary->setEnabled(true);
        }
        zzFlushDeferredObjects();
        QCOMPARE(host.findChildren<QObject *>().size(), initialObjects);
        QCOMPARE(host.findChildren<QAbstractAnimation *>().size(),
                 initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
    }
};

QTEST_MAIN(ZzPopupSurfacesTest)

#include "ZzPopupSurfacesTest.moc"
