#include <array>
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
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QFrame>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
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
    /** @brief 标题栏所有入口均不参与 Tab / Shift+Tab，焦点只在内容控件间循环。 */
    void keepsTabNavigationInsideWindowContent()
    {
        QMainWindow window;
        auto *bar = new ZzFluentUI::ZzFluentTitleBar(&window);
        bar->menuBar()->addMenu(QStringLiteral("File"));
        window.setMenuWidget(bar);
        auto *content = new QWidget(&window);
        auto *layout = new QVBoxLayout(content);
        auto *first = new QLineEdit(content);
        auto *second = new QLineEdit(content);
        layout->addWidget(first);
        layout->addWidget(second);
        window.setCentralWidget(content);
        window.show();
        window.activateWindow();
        for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            window.setLayoutDirection(direction);
            for (auto mode : {ZzFluentUI::ZzTitleBarMenuDisplayMode::Expanded,
                              ZzFluentUI::ZzTitleBarMenuDisplayMode::Compact,
                              ZzFluentUI::ZzTitleBarMenuDisplayMode::Adaptive}) {
                bar->setMenuDisplayMode(mode);
                for (int width : {420, 1200}) {
                    window.resize(width, 300);
                    first->setFocus();
                    QCoreApplication::processEvents();
                    QVERIFY(first->hasFocus());
                    QTest::keyClick(first, Qt::Key_Tab);
                    QVERIFY(second->hasFocus());
                    QTest::keyClick(second, Qt::Key_Tab);
                    QVERIFY(first->hasFocus());
                    QTest::keyClick(first, Qt::Key_Tab, Qt::ShiftModifier);
                    QVERIFY(second->hasFocus());
                    QTest::keyClick(second, Qt::Key_Tab, Qt::ShiftModifier);
                    QVERIFY(first->hasFocus());
                }
            }
        }
    }

    /** @brief 每个按钮独立显隐，重复设置不发通知，刷新后配置与命令状态保持不变。 */
    void controlsEachButtonIndependently()
    {
        using Bar = ZzFluentUI::ZzFluentTitleBar;
        using Button = Bar::Button;
        Bar bar;
        bar.resize(900, bar.height());
        bar.show();
        const std::array<Button, 5> buttons{
            Button::Minimize, Button::Maximize, Button::Close,
            Button::AlwaysOnTop, Button::Theme};
        const std::array<QWidget *, 5> widgets{
            bar.minimizeButton(), bar.maximizeButton(), bar.closeButton(),
            bar.findChild<QToolButton *>("zzTitleBarAlwaysOnTopButton"),
            bar.findChild<QToolButton *>("zzTitleBarThemeButton")};
        QSignalSpy visibility(&bar, &Bar::buttonVisibleChanged);
        QVERIFY(visibility.isValid());
        bar.setAlwaysOnTop(true);
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            QVERIFY(widgets[index]);
            QVERIFY(bar.isButtonVisible(buttons[index]));
            QVERIFY(widgets[index]->isVisible());
            bar.setButtonVisible(buttons[index], false);
            bar.setButtonVisible(buttons[index], false);
            QCOMPARE(visibility.count(), 1);
            QCOMPARE(visibility.at(0).at(0).value<ZzFluentUI::ZzTitleBarButton>(), buttons[index]);
            QCOMPARE(visibility.at(0).at(1).toBool(), false);
            bar.setMaximized(true);
            bar.setThemeMode(ZzFluentUI::ZzThemeMode::Dark);
            QEvent languageChange(QEvent::LanguageChange);
            QCoreApplication::sendEvent(&bar, &languageChange);
            for (std::size_t other = 0; other < buttons.size(); ++other) {
                QCOMPARE(bar.isButtonVisible(buttons[other]), other != index);
                QCOMPARE(widgets[other]->isHidden(), other == index);
            }
            QVERIFY(bar.isAlwaysOnTop());
            bar.setButtonVisible(buttons[index], true);
            QCOMPARE(visibility.count(), 2);
            visibility.clear();
        }
    }

    /** @brief 覆盖全部组合及 RTL：隐藏不占宽度，剩余按钮排列紧凑且可单个恢复。 */
    void packsAllButtonVisibilityCombinations()
    {
        using Bar = ZzFluentUI::ZzFluentTitleBar;
        using Button = Bar::Button;
        Bar bar;
        bar.resize(1000, bar.height());
        bar.show();
        const std::array<Button, 5> buttons{
            Button::Theme, Button::AlwaysOnTop, Button::Minimize,
            Button::Maximize, Button::Close};
        const std::array<QWidget *, 5> widgets{
            bar.findChild<QToolButton *>("zzTitleBarThemeButton"),
            bar.findChild<QToolButton *>("zzTitleBarAlwaysOnTopButton"),
            bar.minimizeButton(), bar.maximizeButton(), bar.closeButton()};
        for (auto *widget : widgets) QVERIFY(widget);
        const int spacing = widgets[4]->x() - widgets[3]->geometry().right() - 1;
        const int expandedWidth = bar.minimumExpandedWidth();
        for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            bar.setLayoutDirection(direction);
            for (unsigned int mask = 0; mask < 32; ++mask) {
                for (std::size_t index = 0; index < buttons.size(); ++index)
                    bar.setButtonVisible(buttons[index], (mask & (1U << index)) != 0);
                QWidget *previous = nullptr;
                for (std::size_t index = 0; index < buttons.size(); ++index) {
                    const bool visible = (mask & (1U << index)) != 0;
                    QCOMPARE(widgets[index]->isVisible(), visible);
                    if (!visible) continue;
                    if (previous) {
                        const int gap = direction == Qt::LeftToRight
                            ? widgets[index]->x() - previous->geometry().right() - 1
                            : previous->x() - widgets[index]->geometry().right() - 1;
                        QCOMPARE(gap, spacing);
                    }
                    previous = widgets[index];
                }
                if (previous) {
                    QCOMPARE(direction == Qt::LeftToRight ? previous->geometry().right()
                                                         : previous->x(),
                             direction == Qt::LeftToRight ? bar.width() - 1 : 0);
                }
                QVERIFY(bar.minimumExpandedWidth() <= expandedWidth);
                QVERIFY(bar.isVisible());
            }
        }
    }

    /** @brief 隐藏父窗口不改变展示配置，无效枚举不修改按钮，省略 bool 时恢复显示。 */
    void keepsVisibilityConfigurationWithoutAVisibleHost()
    {
        using Bar = ZzFluentUI::ZzFluentTitleBar;
        using Button = Bar::Button;
        Bar bar;
        QVERIFY(!bar.isVisible());
        QVERIFY(bar.isButtonVisible(Button::Theme));
        bar.setButtonVisible(Button::Theme, false);
        QVERIFY(!bar.isButtonVisible(Button::Theme));
        bar.setButtonVisible(Button::Theme);
        QVERIFY(bar.isButtonVisible(Button::Theme));
        QSignalSpy changes(&bar, &Bar::buttonVisibleChanged);
        const auto invalid = static_cast<Button>(255);
        QVERIFY(!bar.isButtonVisible(invalid));
        bar.setButtonVisible(invalid, false);
        bar.setButtonVisible(invalid, true);
        QCOMPARE(changes.count(), 0);
        QVERIFY(bar.isButtonVisible(Button::Theme));
        QVERIFY(bar.isButtonVisible(Button::Minimize));
    }

    /** @brief 隐藏按钮同步释放禁止菜单折叠时的宿主最小宽度，同时保留外部约束。 */
    void releasesWindowMinimumWidthWhenHidingButtons()
    {
        using Bar = ZzFluentUI::ZzFluentTitleBar;
        using Button = Bar::Button;
        QMainWindow window;
        window.setMinimumWidth(100);
        auto *bar = new Bar(&window);
        window.setMenuWidget(bar);
        window.show();
        bar->setMenuCollapseEnabled(false);
        const int before = window.minimumWidth();
        bar->setButtonVisible(Button::Close, false);
        QVERIFY(bar->minimumExpandedWidth() < before);
        QCOMPARE(window.minimumWidth(), bar->minimumExpandedWidth());
        const int externalMinimum = before + 80;
        window.setMinimumWidth(externalMinimum);
        bar->setButtonVisible(Button::Theme, false);
        QCOMPARE(window.minimumWidth(), externalMinimum);
        bar->setMenuCollapseEnabled(true);
        QCOMPARE(window.minimumWidth(), externalMinimum);
    }

    void defaultThemeButtonRequestsToggleWithoutMenu()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        auto *button = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarThemeButton"));
        QVERIFY(button != nullptr);
        QVERIFY(button->menu() == nullptr);
        QSignalSpy spy(
            &titleBar, &ZzFluentUI::ZzFluentTitleBar::themeToggleRequested);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(titleBar.themeMode(), ZzFluentUI::ZzThemeMode::System);
        QVERIFY(!button->isChecked());
    }

    /** @brief 没有可展示菜单时，所有模式与布局方向均不显示完整或折叠菜单。 */
    void hidesEmptyMenuInEveryDisplayMode()
    {
        ZzFluentUI::ZzFluentTitleBar bar;
        auto *compact = bar.findChild<QToolButton *>("zzTitleBarCompactMenuButton");
        QVERIFY(compact);
        bar.show();
        for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            bar.setLayoutDirection(direction);
            for (auto mode : {ZzFluentUI::ZzTitleBarMenuDisplayMode::Adaptive,
                              ZzFluentUI::ZzTitleBarMenuDisplayMode::Compact,
                              ZzFluentUI::ZzTitleBarMenuDisplayMode::Expanded}) {
                bar.setMenuDisplayMode(mode);
                for (int width : {420, 1200}) {
                    bar.resize(width, bar.height());
                    QCoreApplication::processEvents();
                    QVERIFY(bar.menuBar()->isHidden());
                    QVERIFY(compact->isHidden());
                    QVERIFY(bar.menuBar()->geometry().isEmpty());
                    QVERIFY(compact->geometry().isEmpty());
                }
            }
        }
        bar.setMenuCollapseEnabled(false);
        QVERIFY(bar.menuBar()->isHidden());
        QVERIFY(compact->isHidden());
    }

    /** @brief 增删、隐藏或恢复最后一个菜单项时同步入口，禁用项仍保留展示。 */
    void tracksDisplayableMenuActions()
    {
        ZzFluentUI::ZzFluentTitleBar bar;
        bar.setMenuDisplayMode(ZzFluentUI::ZzTitleBarMenuDisplayMode::Compact);
        bar.resize(420, bar.height());
        bar.show();
        auto *compact = bar.findChild<QToolButton *>("zzTitleBarCompactMenuButton");
        QVERIFY(compact);
        auto *separator = bar.menuBar()->addSeparator();
        QCoreApplication::processEvents();
        QVERIFY(compact->isHidden());
        auto *menu = bar.menuBar()->addMenu(QStringLiteral("File"));
        auto *action = menu->menuAction();
        QCoreApplication::processEvents();
        QVERIFY(compact->isVisible());
        QCOMPARE(compact->menu()->actions().last(), action);
        action->setVisible(false);
        QCoreApplication::processEvents();
        QVERIFY(compact->isHidden());
        action->setEnabled(false);
        action->setVisible(true);
        QCoreApplication::processEvents();
        QVERIFY(compact->isVisible());
        bar.menuBar()->removeAction(action);
        QCoreApplication::processEvents();
        QVERIFY(compact->isHidden());
        QVERIFY(bar.menuBar()->isHidden());
        bar.menuBar()->removeAction(separator);
        QCoreApplication::processEvents();
        QVERIFY(compact->isHidden());
    }

    /** @brief 最后一个可见动作消失时关闭已打开的折叠菜单并释放最小宽度。 */
    void closesEmptyCompactPopupAndReleasesMenuWidth()
    {
        ZzFluentUI::ZzFluentTitleBar bar;
        bar.show();
        bar.setMenuCollapseEnabled(false);
        const int emptyMinimum = bar.minimumExpandedWidth();
        auto *action = bar.menuBar()->addAction(QString(100, QLatin1Char('W')));
        QCoreApplication::processEvents();
        QVERIFY(bar.minimumExpandedWidth() > emptyMinimum);
        action->setVisible(false);
        QCoreApplication::processEvents();
        QCOMPARE(bar.minimumExpandedWidth(), emptyMinimum);
        QCOMPARE(bar.minimumWidth(), emptyMinimum);
        bar.setMenuCollapseEnabled(true);
        bar.setMenuDisplayMode(ZzFluentUI::ZzTitleBarMenuDisplayMode::Compact);
        action->setVisible(true);
        QCoreApplication::processEvents();
        auto *compact = bar.findChild<QToolButton *>("zzTitleBarCompactMenuButton");
        QVERIFY(compact);
        compact->menu()->popup(compact->mapToGlobal(compact->rect().bottomLeft()));
        QCoreApplication::processEvents();
        QVERIFY(compact->menu()->isVisible());
        action->setVisible(false);
        QCoreApplication::processEvents();
        QVERIFY(compact->isHidden());
        QVERIFY(!compact->menu()->isVisible());
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
            &ZzFluentUI::ZzFluentTitleBar::themeToggleRequested);
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
        QVERIFY(themeButton->menu() == nullptr);
        QVERIFY(themeButton->isCheckable());
        QVERIFY(themeButton->isChecked());
        QVERIFY(!themeButton->toolTip().isEmpty());
        QVERIFY(!themeButton->accessibleName().isEmpty());
        QVERIFY(!alwaysOnTopButton->toolTip().isEmpty());
        QVERIFY(!alwaysOnTopButton->accessibleName().isEmpty());
        QVERIFY(alwaysOnTopButton->isCheckable());
        QVERIFY(alwaysOnTopButton->isChecked());

        QTest::mouseClick(themeButton, Qt::LeftButton);
        QTest::mouseClick(alwaysOnTopButton, Qt::LeftButton);

        QCOMPARE(themeSpy.count(), 1);
        QCOMPARE(alwaysOnTopSpy.count(), 1);
        QCOMPARE(alwaysOnTopSpy.first().first().toBool(), false);
        QCOMPARE(
            titleBar.themeMode(), ZzFluentUI::ZzThemeMode::HighContrast);
        QVERIFY(themeButton->isChecked());
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

        using Button = ZzFluentUI::ZzFluentTitleBar::Button;
        titleBar.setButtonVisible(Button::Minimize, false);
        titleBar.setButtonVisible(Button::Maximize, false);
        titleBar.setButtonVisible(Button::Close, false);
        QVERIFY(titleBar.minimizeButton()->isHidden());
        QVERIFY(titleBar.maximizeButton()->isHidden());
        QVERIFY(titleBar.closeButton()->isHidden());

        titleBar.setButtonVisible(Button::Close, true);
        QVERIFY(titleBar.minimizeButton()->isHidden());
        QVERIFY(titleBar.maximizeButton()->isHidden());
        QVERIFY(titleBar.closeButton()->isVisible());
    }

    void supportsACompactSecondaryWindowChrome()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.show();
        QCoreApplication::processEvents();

        using Button = ZzFluentUI::ZzFluentTitleBar::Button;
        titleBar.setButtonVisible(Button::Theme, false);
        titleBar.setButtonVisible(Button::AlwaysOnTop, false);

        QVERIFY(titleBar.findChild<QToolButton *>(
                    QStringLiteral("zzTitleBarThemeButton"))->isHidden());
        QVERIFY(titleBar.findChild<QToolButton *>(
                    QStringLiteral("zzTitleBarAlwaysOnTopButton"))->isHidden());
        QVERIFY(titleBar.minimizeButton()->isVisible());
        QVERIFY(titleBar.maximizeButton()->isVisible());
        QVERIFY(titleBar.closeButton()->isVisible());

        titleBar.setButtonVisible(Button::Minimize, false);
        titleBar.setButtonVisible(Button::Maximize, false);
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

    void mapsThemeIconsToCurrentLightAndDarkModes()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setStyle(&style);
        auto *const themeButton = qobject_cast<QToolButton *>(
            titleBar.findChild<QWidget *>(
                QStringLiteral("zzTitleBarThemeButton")));
        QVERIFY(themeButton != nullptr);
        if (themeButton == nullptr) {
            return;
        }

        const auto iconImage = [&titleBar](QToolButton *button) {
            return button->icon()
                .pixmap(QSize(16, 16), titleBar.devicePixelRatioF())
                .toImage();
        };
        const auto expectedImage = [&titleBar, &style](
                                       ZzFluentUI::ZzBundledSvgIcon icon) {
            return style.iconPixmap(
                       ZzFluentUI::ZzIconDescriptor::fromBundledSvg(icon),
                       QSize(16, 16),
                       titleBar.devicePixelRatioF(),
                       titleBar.palette().color(QPalette::ButtonText),
                       titleBar.layoutDirection())
                .toImage();
        };

        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::Light);
        QCOMPARE(
            iconImage(themeButton),
            expectedImage(ZzFluentUI::ZzBundledSvgIcon::Sun));
        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(
            iconImage(themeButton),
            expectedImage(ZzFluentUI::ZzBundledSvgIcon::Moon));

        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::Light);
        QCOMPARE(
            iconImage(themeButton),
            expectedImage(ZzFluentUI::ZzBundledSvgIcon::Sun));
        QCOMPARE(themeButton->toolTip(), QStringLiteral("切换到深色"));
        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(
            iconImage(themeButton),
            expectedImage(ZzFluentUI::ZzBundledSvgIcon::Moon));
        QCOMPARE(themeButton->toolTip(), QStringLiteral("切换到浅色"));

        // 请求模式保持 System 时，图标仍应随实际配色改变。
        titleBar.setThemeMode(ZzFluentUI::ZzThemeMode::System);
        controller.setMode(ZzFluentUI::ZzThemeMode::Light);
        QEvent paletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(&titleBar, &paletteChange);
        QCOMPARE(iconImage(themeButton), expectedImage(ZzFluentUI::ZzBundledSvgIcon::Sun));
        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCoreApplication::sendEvent(&titleBar, &paletteChange);
        QCOMPARE(iconImage(themeButton), expectedImage(ZzFluentUI::ZzBundledSvgIcon::Moon));
    }

    /** @brief 置顶复用同一线框图标，仅以强调色区分，主题和强调色变化后立即刷新。 */
    void pinUsesAccentOnlyForConfirmedAlwaysOnTop()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setStyle(&style);
        auto *pin = titleBar.findChild<QToolButton *>(
            QStringLiteral("zzTitleBarAlwaysOnTopButton"));
        QVERIFY(pin);
        for (const auto mode : {ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark, ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            for (const QColor accent : {QColor("#0067c0"), QColor("#c239b3")}) {
                controller.setAccentColor(accent);
                titleBar.setPalette(style.standardPalette());
                // 保持置顶跨越强调色和主题变化，先验证刷新，再验证取消与重新置顶。
                for (const bool pinned : {true, false, true}) {
                    titleBar.setAlwaysOnTop(pinned);
                    const QColor color = titleBar.palette().color(
                        pinned ? QPalette::Accent : QPalette::ButtonText);
                    const auto expected = style.iconPixmap(
                        ZzFluentUI::ZzIconDescriptor::fromBundledSvg(ZzFluentUI::ZzBundledSvgIcon::Pin),
                        QSize(16, 16), titleBar.devicePixelRatioF(), color, titleBar.layoutDirection());
                    QCOMPARE(pin->icon().pixmap(QSize(16, 16), titleBar.devicePixelRatioF()).toImage(),
                        expected.toImage());
                    QCOMPARE(pin->isChecked(), pinned);
                }
            }
        }
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
