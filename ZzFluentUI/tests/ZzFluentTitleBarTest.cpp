#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTranslator>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPalette>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QFrame>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTitleBarThemeInteractionMode.h>
#include <ZzFluentUI/ZzTitleBarMenuDisplayMode.h>

/** @brief 为标题栏 LanguageChange 测试提供确定翻译。 */
class ZzTitleBarTranslator final : public QTranslator
{
public:
    /** @brief 为所有标题栏静态命令添加测试前缀。 */
    [[nodiscard]] QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int plural = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(plural)
        if (sourceText == nullptr) {
            return {};
        }
        if (std::strcmp(sourceText, "最小化") == 0
            || std::strcmp(sourceText, "最大化") == 0
            || std::strcmp(sourceText, "还原") == 0
            || std::strcmp(sourceText, "关闭") == 0) {
            return QStringLiteral("T:") + QString::fromUtf8(sourceText);
        }
        return {};
    }
};

/** @brief 验证标题栏只发窗口意图并维护 chrome 可访问状态。 */
class ZzFluentTitleBarTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesConfigurableThemeInteractionMode()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        QCOMPARE(
            titleBar.themeInteractionMode(),
            ZzFluentUI::ZzTitleBarThemeInteractionMode::Menu);
        QSignalSpy spy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::themeInteractionModeChanged);
        titleBar.setThemeInteractionMode(
            ZzFluentUI::ZzTitleBarThemeInteractionMode::Toggle);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(
            spy.first()
                .first()
                .value<ZzFluentUI::ZzTitleBarThemeInteractionMode>(),
            ZzFluentUI::ZzTitleBarThemeInteractionMode::Toggle);
    }

    void emitsThemeIntentForSelectedInteractionMode()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *button = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarThemeButton"));
        QVERIFY(button != nullptr);
        if (button == nullptr) {
            return;
        }
        QSignalSpy modeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::themeModeRequested);
        QSignalSpy toggleSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::themeToggleRequested);
        titleBar.setThemeInteractionMode(
            ZzFluentUI::ZzTitleBarThemeInteractionMode::Toggle);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(toggleSpy.count(), 1);
    }

    void emitsThemeModeIntentFromMenuAction()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        QSignalSpy modeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::themeModeRequested);
        QVERIFY(titleBar.themeMenu() != nullptr);
        if (titleBar.themeMenu() == nullptr) {
            return;
        }
        QAction *highContrastAction = nullptr;
        for (QAction *action : titleBar.themeMenu()->actions()) {
            if (action->data().toInt()
                == static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)) {
                highContrastAction = action;
                break;
            }
        }
        QVERIFY(highContrastAction != nullptr);
        if (highContrastAction == nullptr) {
            return;
        }
        highContrastAction->trigger();
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(
            modeSpy.first().first().toInt(),
            static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast));
    }

    void preservesCompactMenuOrderForMiddleInsertions()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *menuBar = titleBar.menuBar();
        auto *compactButton = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarCompactMenuButton"));
        QVERIFY(menuBar != nullptr);
        QVERIFY(compactButton != nullptr);
        if (menuBar == nullptr || compactButton == nullptr) {
            return;
        }
        QVERIFY(compactButton->menu() != nullptr);
        if (compactButton->menu() == nullptr) {
            return;
        }

        QAction first(QStringLiteral("First"), &titleBar);
        QAction middle(QStringLiteral("Middle"), &titleBar);
        QAction last(QStringLiteral("Last"), &titleBar);
        menuBar->addAction(&first);
        menuBar->addAction(&last);
        menuBar->insertAction(&last, &middle);
        QCoreApplication::processEvents();

        QCOMPARE(
            menuBar->actions(),
            QList<QAction *>({&first, &middle, &last}));
        QCOMPARE(
            compactButton->menu()->actions(),
            QList<QAction *>({&first, &middle, &last}));
    }

    void rebuildsCompactMenuWithoutChangingSourceActions()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *menuBar = titleBar.menuBar();
        auto *compactButton = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarCompactMenuButton"));
        QVERIFY(menuBar != nullptr);
        QVERIFY(compactButton != nullptr);
        if (menuBar == nullptr || compactButton == nullptr) {
            return;
        }
        QVERIFY(compactButton->menu() != nullptr);
        if (compactButton->menu() == nullptr) {
            return;
        }

        auto *first = menuBar->addAction(QStringLiteral("First"));
        auto *second = menuBar->addAction(QStringLiteral("Second"));
        QVERIFY(first != nullptr);
        QVERIFY(second != nullptr);
        first->setCheckable(true);
        first->setChecked(true);
        first->setData(QStringLiteral("first-data"));
        second->setEnabled(false);
        const QPointer<QAction> firstGuard(first);
        const QPointer<QAction> secondGuard(second);

        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&titleBar, &languageChange);
        QCoreApplication::processEvents();

        QVERIFY(!firstGuard.isNull());
        QVERIFY(!secondGuard.isNull());
        QCOMPARE(
            compactButton->menu()->actions(),
            QList<QAction *>({first, second}));
        QVERIFY(first->isCheckable());
        QVERIFY(first->isChecked());
        QCOMPARE(first->data().toString(), QStringLiteral("first-data"));
        QVERIFY(!second->isEnabled());
    }

    void presentsConfirmedHighContrastWithThemeAction()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *themeButton = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarThemeButton"));
        QVERIFY(themeButton != nullptr);
        if (themeButton == nullptr) {
            return;
        }
        QVERIFY(themeButton->menu() != nullptr);
        if (themeButton->menu() == nullptr) {
            return;
        }
        QCOMPARE(themeButton->menu()->actions().size(), 4);

        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::HighContrast);

        QCOMPARE(
            titleBar.themeMode(), ZzFluentUI::ZzThemeMode::HighContrast);
        QVERIFY(themeButton->isChecked());
        QVERIFY(themeButton->toolTip().contains(QStringLiteral("高对比度")));
        QVERIFY(themeButton->accessibleName().contains(
            QStringLiteral("高对比度")));
        QCOMPARE(themeButton->menu()->actions().size(), 4);
        for (QAction *action : themeButton->menu()->actions()) {
            QVERIFY(!action->icon().isNull());
            const auto mode = static_cast<ZzFluentUI::ZzThemeMode>(
                action->data().toInt());
            QCOMPARE(action->isChecked(), mode == ZzFluentUI::ZzThemeMode::HighContrast);
        }
    }

    void adaptsMenuWithoutCopyingActionsOrAllocatingMenus()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *menuBar = titleBar.menuBar();
        QVERIFY(menuBar != nullptr);
        if (menuBar == nullptr) {
            return;
        }
        QVERIFY(!menuBar->isNativeMenuBar());
        auto *fileMenu = menuBar->addMenu(QStringLiteral("File"));
        auto *editMenu = menuBar->addMenu(QStringLiteral("Edit"));
        QVERIFY(fileMenu != nullptr);
        QVERIFY(editMenu != nullptr);
        if (fileMenu == nullptr || editMenu == nullptr) {
            return;
        }
        QAction dynamicAction(QStringLiteral("Help"), &titleBar);
        auto *compactButton = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarCompactMenuButton"));
        QVERIFY(compactButton != nullptr);
        if (compactButton == nullptr) {
            return;
        }
        QVERIFY(compactButton->menu() != nullptr);
        if (compactButton->menu() == nullptr) {
            return;
        }

        titleBar.setMenuDisplayMode(
            ZzFluentUI::ZzTitleBarMenuDisplayMode::Adaptive);
        titleBar.resize(1200, titleBar.height());
        titleBar.show();
        QCoreApplication::processEvents();
        QVERIFY(menuBar->isVisible());
        QVERIFY(compactButton->isHidden());

        titleBar.resize(420, titleBar.height());
        QCoreApplication::processEvents();
        QVERIFY(menuBar->isHidden());
        QVERIFY(compactButton->isVisible());
        QVERIFY(compactButton->menu()->actions().contains(
            fileMenu->menuAction()));
        QVERIFY(compactButton->menu()->actions().contains(
            editMenu->menuAction()));

        const qsizetype menuObjectCount =
            titleBar.findChildren<QMenu *>().size();
        menuBar->addAction(&dynamicAction);
        QCoreApplication::processEvents();
        QVERIFY(compactButton->menu()->actions().contains(&dynamicAction));
        menuBar->removeAction(&dynamicAction);
        QCoreApplication::processEvents();
        QVERIFY(!compactButton->menu()->actions().contains(&dynamicAction));

        for (const int width : {1200, 520, 1200, 520}) {
            titleBar.resize(width, titleBar.height());
            QCoreApplication::processEvents();
        }
        titleBar.setMenuDisplayMode(
            ZzFluentUI::ZzTitleBarMenuDisplayMode::Expanded);
        QVERIFY(menuBar->isVisible());
        QVERIFY(compactButton->isHidden());
        titleBar.setMenuDisplayMode(
            ZzFluentUI::ZzTitleBarMenuDisplayMode::Compact);
        QVERIFY(menuBar->isHidden());
        QVERIFY(compactButton->isVisible());

        titleBar.setMenuDisplayMode(
            ZzFluentUI::ZzTitleBarMenuDisplayMode::Adaptive);
        titleBar.resize(1200, titleBar.height());
        QCoreApplication::processEvents();
        int firstCompactWidth = -1;
        for (int width = 1199; width >= 300; --width) {
            titleBar.resize(width, titleBar.height());
            QCoreApplication::processEvents();
            if (compactButton->isVisible()) {
                firstCompactWidth = width;
                break;
            }
        }
        QVERIFY(firstCompactWidth > 0);
        titleBar.resize(firstCompactWidth + 10, titleBar.height());
        QCoreApplication::processEvents();
        QVERIFY(compactButton->isVisible());
        titleBar.resize(firstCompactWidth + 30, titleBar.height());
        QCoreApplication::processEvents();
        QVERIFY(menuBar->isVisible());
        QCOMPARE(
            titleBar.findChildren<QMenu *>().size(), menuObjectCount);
    }

    void keepsCompleteTitleWhenInteractiveSafeAreaFitsText()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        const QString title = QStringLiteral("ZzPureToolsExample");
        titleBar.setTitle(title);
        titleBar.resize(1280, titleBar.height());
        titleBar.show();
        QCoreApplication::processEvents();

        auto *titleLabel = titleBar.findChild<QLabel *>(
            QStringLiteral("zzTitleBarTitle"));
        QVERIFY(titleLabel != nullptr);
        if (titleLabel == nullptr) {
            return;
        }
        QVERIFY(titleLabel->isVisible());
        QCOMPARE(titleLabel->text(), title);
    }

    void expandsMenuAfterWindowReceivesFinalGeometry()
    {
        QMainWindow window;
        auto *titleBar = new ZzFluentUI::ZzFluentTitleBar(&window);
        window.setMenuWidget(titleBar);
        window.resize(620, 560);
        titleBar->setTitle(QStringLiteral("ZzPureToolsExample"));
        titleBar->menuBar()->addMenu(QStringLiteral("文件"));
        titleBar->menuBar()->addMenu(QStringLiteral("导航"));
        titleBar->menuBar()->addMenu(QStringLiteral("视图"));

        window.show();
        QCoreApplication::processEvents();

        auto *compactButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarCompactMenuButton"));
        QVERIFY(compactButton != nullptr);
        if (compactButton == nullptr) {
            return;
        }
        QVERIFY(titleBar->menuBar()->isVisible());
        QVERIFY(compactButton->isHidden());
    }

    void disablesAdaptiveCollapseAndConstrainsHostMinimumWidth()
    {
        QMainWindow window;
        window.setMinimumWidth(0);
        auto *titleBar = new ZzFluentUI::ZzFluentTitleBar(&window);
        window.setMenuWidget(titleBar);
        titleBar->setTitle(QStringLiteral("ZzPureToolsExample"));
        titleBar->menuBar()->addMenu(QStringLiteral("文件"));
        titleBar->menuBar()->addMenu(QStringLiteral("导航"));
        titleBar->menuBar()->addMenu(QStringLiteral("视图"));
        window.resize(420, 560);
        window.show();
        QCoreApplication::processEvents();

        auto *compactButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarCompactMenuButton"));
        QVERIFY(compactButton != nullptr);
        if (compactButton == nullptr) {
            return;
        }
        QVERIFY(titleBar->menuBar()->isHidden());
        QVERIFY(compactButton->isVisible());

        QSignalSpy collapseSpy(
            titleBar,
            &ZzFluentUI::ZzFluentTitleBar::menuCollapseEnabledChanged);
        const int originalMinimumWidth = window.minimumWidth();
        titleBar->setMenuCollapseEnabled(false);
        QCoreApplication::processEvents();

        QVERIFY(!titleBar->isMenuCollapseEnabled());
        QCOMPARE(collapseSpy.count(), 1);
        QCOMPARE(collapseSpy.at(0).at(0).toBool(), false);
        QVERIFY(titleBar->menuBar()->isVisible());
        QVERIFY(compactButton->isHidden());
        QVERIFY(titleBar->minimumExpandedWidth() > originalMinimumWidth);
        QCOMPARE(window.minimumWidth(), titleBar->minimumExpandedWidth());
        QVERIFY(window.width() >= window.minimumWidth());

        titleBar->setMenuCollapseEnabled(true);
        QCoreApplication::processEvents();
        QVERIFY(titleBar->isMenuCollapseEnabled());
        QCOMPARE(collapseSpy.count(), 2);
        QCOMPARE(collapseSpy.at(1).at(0).toBool(), true);
        QCOMPARE(window.minimumWidth(), originalMinimumWidth);

        titleBar->setMenuCollapseEnabled(true);
        QCOMPARE(collapseSpy.count(), 2);

        window.resize(420, 560);
        QCoreApplication::processEvents();
        QVERIFY(titleBar->menuBar()->isHidden());
        QVERIFY(compactButton->isVisible());
    }

    void preservesExternalMinimumWidthAcrossRequirementChanges()
    {
        QMainWindow window;
        window.setMinimumWidth(0);
        auto *titleBar = new ZzFluentUI::ZzFluentTitleBar(&window);
        window.setMenuWidget(titleBar);
        titleBar->setTitle(QStringLiteral("Workspace"));
        titleBar->menuBar()->addMenu(QStringLiteral("文件"));
        window.resize(420, 560);
        window.show();
        QCoreApplication::processEvents();

        titleBar->setMenuCollapseEnabled(false);
        QCoreApplication::processEvents();
        const int enforcedMinimum = titleBar->minimumExpandedWidth();
        const int externalMinimum = enforcedMinimum + 80;
        window.setMinimumWidth(externalMinimum);

        titleBar->menuBar()->addMenu(QString(300, QLatin1Char('x')));
        QCoreApplication::processEvents();
        QVERIFY(titleBar->minimumExpandedWidth() > externalMinimum);

        titleBar->setMenuCollapseEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(window.minimumWidth(), externalMinimum);
    }

    void transfersMinimumWidthConstraintAfterReparenting()
    {
        auto *titleBar = new ZzFluentUI::ZzFluentTitleBar;
        titleBar->setTitle(QStringLiteral("Workspace"));
        titleBar->menuBar()->addMenu(QStringLiteral("文件"));
        titleBar->resize(420, titleBar->height());
        titleBar->show();
        QCoreApplication::processEvents();

        titleBar->setMenuCollapseEnabled(false);
        QCoreApplication::processEvents();
        const int titleBarMinimum = titleBar->minimumWidth();
        QVERIFY(titleBarMinimum >= titleBar->minimumExpandedWidth());

        QMainWindow window;
        window.setMinimumWidth(0);
        window.setMenuWidget(titleBar);
        window.resize(720, 560);
        window.show();
        QCoreApplication::processEvents();
        QVERIFY(window.minimumWidth() >= titleBar->minimumExpandedWidth());

        titleBar->setMenuCollapseEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(window.minimumWidth(), 0);
    }

    void keepsTitleCenteredInsideInteractiveSafeArea()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.menuBar()->addMenu(QStringLiteral("File"));
        titleBar.menuBar()->addMenu(QStringLiteral("Edit"));
        titleBar.setTitle(QStringLiteral(
            "A very long workspace title that must be elided safely"));
        titleBar.resize(720, titleBar.height());
        titleBar.show();
        QCoreApplication::processEvents();

        auto *titleLabel = titleBar.findChild<QLabel *>(
            QStringLiteral("zzTitleBarTitle"));
        QVERIFY(titleLabel != nullptr);
        if (titleLabel == nullptr) {
            return;
        }
        QVERIFY(titleLabel->isVisible());
        QCOMPARE(titleLabel->geometry().center().x(), titleBar.rect().center().x());
        QVERIFY(titleLabel->text() != titleBar.title());
        for (QWidget *widget : titleBar.hitTestVisibleWidgets()) {
            if (widget->isVisible()) {
                QVERIFY(!titleLabel->geometry().intersects(widget->geometry()));
            }
        }
        for (QWidget *widget : {
                 titleBar.minimizeButton(),
                 titleBar.maximizeButton(),
                 titleBar.closeButton()}) {
            if (widget->isVisible()) {
                QVERIFY(!titleLabel->geometry().intersects(widget->geometry()));
            }
        }
    }

    void emitsThemeAndAlwaysOnTopIntentWithoutMutatingConfirmedState()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::HighContrast);
        titleBar.setAlwaysOnTop(true);
        QSignalSpy themeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::themeModeRequested);
        QSignalSpy alwaysOnTopSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::alwaysOnTopRequested);
        auto *themeButton = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarThemeButton"));
        auto *alwaysOnTopButton = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarAlwaysOnTopButton"));
        QVERIFY(themeButton != nullptr);
        QVERIFY(alwaysOnTopButton != nullptr);
        if (themeButton == nullptr || alwaysOnTopButton == nullptr) {
            return;
        }
        QVERIFY(themeButton->menu() != nullptr);
        if (themeButton->menu() == nullptr) {
            return;
        }
        QVERIFY(themeButton->isCheckable());
        QVERIFY(themeButton->isChecked());
        QVERIFY(!themeButton->toolTip().isEmpty());
        QVERIFY(!themeButton->accessibleName().isEmpty());
        QVERIFY(!alwaysOnTopButton->toolTip().isEmpty());
        QVERIFY(!alwaysOnTopButton->accessibleName().isEmpty());
        QVERIFY(alwaysOnTopButton->isCheckable());
        QVERIFY(alwaysOnTopButton->isChecked());

        QAction *lightAction = nullptr;
        for (QAction *action : themeButton->menu()->actions()) {
            if (action->data().toInt()
                == static_cast<int>(ZzFluentUI::ZzThemeMode::Light)) {
                lightAction = action;
                break;
            }
        }
        QVERIFY(lightAction != nullptr);
        if (lightAction == nullptr) {
            return;
        }
        lightAction->trigger();
        QTest::mouseClick(alwaysOnTopButton, Qt::LeftButton);

        QCOMPARE(themeSpy.count(), 1);
        QCOMPARE(
            themeSpy.first().first().toInt(),
            static_cast<int>(ZzFluentUI::ZzThemeMode::Light));
        QCOMPARE(alwaysOnTopSpy.count(), 1);
        QCOMPARE(alwaysOnTopSpy.first().first().toBool(), false);
        QCOMPARE(
            titleBar.themeMode(), ZzFluentUI::ZzThemeMode::HighContrast);
        QVERIFY(titleBar.isAlwaysOnTop());
        QVERIFY(alwaysOnTopButton->isChecked());
    }

    void doesNotPaintCheckedSurfaceForThemeAndAlwaysOnTop()
    {
        ZzFluentUI::ZzThemeController themeController;
        ZzFluentUI::ZzFluentStyle fluentStyle(&themeController);
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setStyle(&fluentStyle);
        auto *themeButton = qobject_cast<QToolButton *>(
            titleBar.findChild<QWidget *>(
                QStringLiteral("zzTitleBarThemeButton")));
        auto *alwaysOnTopButton = qobject_cast<QToolButton *>(
            titleBar.findChild<QWidget *>(
                QStringLiteral("zzTitleBarAlwaysOnTopButton")));
        QVERIFY(themeButton != nullptr);
        QVERIFY(alwaysOnTopButton != nullptr);
        if (themeButton == nullptr || alwaysOnTopButton == nullptr) {
            return;
        }

        const auto renderPanel = [&fluentStyle](
                                    QToolButton *button,
                                    bool checked) {
            QImage image(button->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            QStyleOptionToolButton option;
            option.initFrom(button);
            option.rect = button->rect();
            option.state.setFlag(QStyle::State_On, checked);
            fluentStyle.drawPrimitive(
                QStyle::PE_PanelButtonTool,
                &option,
                &painter,
                button);
            return image;
        };

        QCOMPARE(
            renderPanel(themeButton, true),
            renderPanel(themeButton, false));
        QCOMPARE(
            renderPanel(alwaysOnTopButton, true),
            renderPanel(alwaysOnTopButton, false));
    }

    void mirrorsChromeGroupsForRightToLeftLayouts()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.menuBar()->addMenu(QStringLiteral("File"));
        titleBar.setTitle(QStringLiteral("Workspace"));
        titleBar.resize(900, titleBar.height());
        titleBar.show();
        QCoreApplication::processEvents();
        const int leftToRightIconX =
            titleBar.windowIconWidget()->geometry().center().x();
        const int leftToRightCloseX =
            titleBar.closeButton()->geometry().center().x();
        QVERIFY(leftToRightIconX < titleBar.rect().center().x());
        QVERIFY(leftToRightCloseX > titleBar.rect().center().x());

        titleBar.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        QVERIFY(
            titleBar.windowIconWidget()->geometry().center().x()
            > titleBar.rect().center().x());
        QVERIFY(
            titleBar.closeButton()->geometry().center().x()
            < titleBar.rect().center().x());
        auto *titleLabel = titleBar.findChild<QLabel *>(
            QStringLiteral("zzTitleBarTitle"));
        QVERIFY(titleLabel != nullptr);
        if (titleLabel == nullptr) {
            return;
        }
        QCOMPARE(titleLabel->geometry().center().x(), titleBar.rect().center().x());
    }

    void emitsSystemButtonIntentOnly()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setTitle(QStringLiteral("Workspace"));
        titleBar.show();
        QCoreApplication::processEvents();
        auto *minimize = qobject_cast<QToolButton *>(
            titleBar.minimizeButton());
        auto *maximize = qobject_cast<QToolButton *>(
            titleBar.maximizeButton());
        auto *close = qobject_cast<QToolButton *>(titleBar.closeButton());
        QVERIFY(minimize != nullptr);
        QVERIFY(maximize != nullptr);
        QVERIFY(close != nullptr);
        if (minimize == nullptr || maximize == nullptr || close == nullptr) {
            return;
        }
        QSignalSpy minimizeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::minimizeRequested);
        QSignalSpy maximizeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::maximizeRestoreRequested);
        QSignalSpy closeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::closeRequested);

        minimize->setFocus();
        QTest::keyClick(minimize, Qt::Key_Space);
        maximize->setFocus();
        QTest::keyClick(maximize, Qt::Key_Space);
        close->setFocus();
        QTest::keyClick(close, Qt::Key_Space);

        QCOMPARE(minimizeSpy.count(), 1);
        QCOMPARE(maximizeSpy.count(), 1);
        QCOMPARE(closeSpy.count(), 1);
        QVERIFY(titleBar.isVisible());
    }

    void exposesOnlyDescendantChromeWidgets()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.show();
        QCoreApplication::processEvents();
        const QList<QWidget *> chrome{
            titleBar.windowIconWidget(),
            titleBar.minimizeButton(),
            titleBar.maximizeButton(),
            titleBar.closeButton()};
        for (QWidget *widget : chrome) {
            QVERIFY(widget != nullptr);
            if (widget == nullptr) {
                return;
            }
            QVERIFY(titleBar.isAncestorOf(widget));
        }
        const QList<QWidget *> interactive = titleBar.interactiveWidgets();
        QCOMPARE(interactive.size(), 3);
        QVERIFY(interactive.contains(titleBar.minimizeButton()));
        QVERIFY(interactive.contains(titleBar.maximizeButton()));
        QVERIFY(interactive.contains(titleBar.closeButton()));

        const QList<QWidget *> hitTestVisible =
            titleBar.hitTestVisibleWidgets();
        QCOMPARE(hitTestVisible.size(), 4);
        for (QWidget *widget : hitTestVisible) {
            QVERIFY(widget != nullptr);
            if (widget == nullptr) {
                return;
            }
            QVERIFY(titleBar.isAncestorOf(widget));
            QVERIFY(!interactive.contains(widget));
        }

        titleBar.setSystemButtonsVisible(false);
        QVERIFY(titleBar.minimizeButton()->isHidden());
        QVERIFY(titleBar.maximizeButton()->isHidden());
        QVERIFY(titleBar.closeButton()->isHidden());

        titleBar.setWindowButtonsVisible(false, false, true);
        QVERIFY(titleBar.minimizeButton()->isHidden());
        QVERIFY(titleBar.maximizeButton()->isHidden());
        QVERIFY(titleBar.closeButton()->isVisible());
    }

    void supportsACompactSecondaryWindowChrome()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.show();
        QCoreApplication::processEvents();

        titleBar.setCommandButtonsVisible(false);

        QVERIFY(titleBar.findChild<QToolButton *>(
                    QStringLiteral("zzTitleBarThemeButton"))->isHidden());
        QVERIFY(titleBar.findChild<QToolButton *>(
                    QStringLiteral("zzTitleBarAlwaysOnTopButton"))->isHidden());
        QVERIFY(titleBar.minimizeButton()->isVisible());
        QVERIFY(titleBar.maximizeButton()->isVisible());
        QVERIFY(titleBar.closeButton()->isVisible());

        titleBar.setWindowButtonsVisible(false, false, true);
        QCoreApplication::processEvents();
        QVERIFY(titleBar.minimizeButton()->isHidden());
        QVERIFY(titleBar.maximizeButton()->isHidden());
        QVERIFY(titleBar.closeButton()->isVisible());
        QCOMPARE(
            titleBar.closeButton()->geometry().right(),
            titleBar.width() - 1);
    }

    void placesCloseButtonAtTheEdgeAndDrawsBodySeparator()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.resize(900, titleBar.height());
        titleBar.show();
        QCoreApplication::processEvents();

        QCOMPARE(titleBar.closeButton()->geometry().right(), titleBar.width() - 1);
        auto *separator = titleBar.findChild<QFrame *>(
            QStringLiteral("zzTitleBarSeparator"));
        QVERIFY(separator != nullptr);
        if (separator == nullptr) {
            return;
        }
        QCOMPARE(separator->geometry().bottom(), titleBar.height() - 1);
        QCOMPARE(separator->frameShape(), QFrame::HLine);
    }

    void drawsTitleSeparatorAsSubtleSurfaceStroke()
    {
        ZzFluentUI::ZzThemeController themeController;
        themeController.setMode(ZzFluentUI::ZzThemeMode::Light);
        ZzFluentUI::ZzFluentStyle fluentStyle(&themeController);
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setStyle(&fluentStyle);
        titleBar.resize(900, titleBar.height());
        titleBar.show();
        QCoreApplication::processEvents();

        auto *const separator = titleBar.findChild<QFrame *>(
            QStringLiteral("zzTitleBarSeparator"));
        QVERIFY(separator != nullptr);
        if (separator == nullptr) {
            return;
        }
        QImage image(titleBar.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(fluentStyle.themeSnapshot()->color(
            ZzFluentUI::ZzColorToken::Surface));
        titleBar.render(&image);
        const QColor line = image.pixelColor(
            image.width() / 2,
            separator->geometry().center().y());
        const QColor stroke = fluentStyle.themeSnapshot()->color(
            ZzFluentUI::ZzColorToken::ControlStroke);
        QVERIFY(line.isValid());
        QVERIFY(line != stroke);
        QVERIFY(line.lightness() > stroke.lightness());
    }

    void updatesTitleIconAndMaximizedAccessibility()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        QSignalSpy titleSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::titleChanged);
        titleBar.setTitle(QStringLiteral("Editor"));
        titleBar.setTitle(QStringLiteral("Editor"));
        QPixmap pixmap(QSize(16, 16));
        pixmap.fill(Qt::green);
        titleBar.setWindowIcon(QIcon(pixmap));

        QCOMPARE(titleBar.title(), QStringLiteral("Editor"));
        QCOMPARE(titleSpy.count(), 1);
        QCOMPARE(
            titleBar.maximizeButton()->accessibleName(),
            QStringLiteral("最大化"));
        titleBar.setMaximized(true);
        QCOMPARE(
            titleBar.maximizeButton()->accessibleName(),
            QStringLiteral("还原"));
    }

    void switchesBundledSvgIconsWithSystemStates()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *themeButton = qobject_cast<QToolButton *>(
            titleBar.findChild<QWidget *>(
                QStringLiteral("zzTitleBarThemeButton")));
        auto *pinButton = qobject_cast<QToolButton *>(
            titleBar.findChild<QWidget *>(
                QStringLiteral("zzTitleBarAlwaysOnTopButton")));
        auto *maximizeButton = qobject_cast<QToolButton *>(
            titleBar.findChild<QWidget *>(
                QStringLiteral("zzTitleBarMaximizeButton")));
        QVERIFY(themeButton != nullptr);
        QVERIFY(pinButton != nullptr);
        QVERIFY(maximizeButton != nullptr);
        if (themeButton == nullptr
            || pinButton == nullptr
            || maximizeButton == nullptr) {
            return;
        }
        QVERIFY(!themeButton->icon().isNull());
        QVERIFY(!pinButton->icon().isNull());
        QVERIFY(!maximizeButton->icon().isNull());

        const qint64 systemThemeKey = themeButton->icon().cacheKey();
        const qint64 unpinnedKey = pinButton->icon().cacheKey();
        const qint64 normalKey = maximizeButton->icon().cacheKey();

        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::Light);
        titleBar.setAlwaysOnTop(true);
        titleBar.setMaximized(true);

        QVERIFY(themeButton->icon().cacheKey() != systemThemeKey);
        QVERIFY(pinButton->icon().cacheKey() != unpinnedKey);
        QVERIFY(maximizeButton->icon().cacheKey() != normalKey);
    }

    void refreshesTranslatedChromeText()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setMaximized(true);
        ZzTitleBarTranslator translator;
        QCoreApplication::installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&titleBar, &languageChange);

        QCOMPARE(
            titleBar.minimizeButton()->accessibleName(),
            QStringLiteral("T:最小化"));
        QCOMPARE(
            titleBar.maximizeButton()->accessibleName(),
            QStringLiteral("T:还原"));
        QCOMPARE(
            titleBar.closeButton()->accessibleName(),
            QStringLiteral("T:关闭"));
        QCOMPARE(
            titleBar.closeButton()->toolTip(),
            QStringLiteral("T:关闭"));

        QCoreApplication::removeTranslator(&translator);
    }

    void refreshesIconsFromTitleBarPalette()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *close = qobject_cast<QToolButton *>(titleBar.closeButton());
        QVERIFY(close != nullptr);

        QPalette staleButtonPalette = close->palette();
        staleButtonPalette.setColor(QPalette::ButtonText, Qt::black);
        close->setPalette(staleButtonPalette);
        QPalette currentTitleBarPalette = titleBar.palette();
        currentTitleBarPalette.setColor(
            QPalette::ButtonText,
            QColor(240, 241, 242));
        titleBar.setPalette(currentTitleBarPalette);
        QEvent paletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(&titleBar, &paletteChange);

        const QImage icon = close->icon()
                                .pixmap(QSize(16, 16), 1.0)
                                .toImage()
                                .convertToFormat(QImage::Format_ARGB32);
        bool foundGlyph = false;
        for (int y = 0; y < icon.height(); ++y) {
            const auto *line = reinterpret_cast<const QRgb *>(
                icon.constScanLine(y));
            for (int x = 0; x < icon.width(); ++x) {
                if (qAlpha(line[x]) == 0) {
                    continue;
                }
                foundGlyph = true;
                QVERIFY(qRed(line[x]) >= 220);
                QVERIFY(qGreen(line[x]) >= 220);
                QVERIFY(qBlue(line[x]) >= 220);
            }
        }
        QVERIFY(foundGlyph);
    }
};

QTEST_MAIN(ZzFluentTitleBarTest)

#include "ZzFluentTitleBarTest.moc"
