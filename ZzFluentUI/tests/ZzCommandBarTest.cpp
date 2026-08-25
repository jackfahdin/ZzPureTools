#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtGui/QAccessible>
#include <QtGui/QAction>
#include <QtGui/QKeySequence>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzCommandBar.h>

namespace {

/** @brief 刷新命令栏重建和延迟删除事件。 */
void zzFlushEvents()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

/** @brief 返回命令栏的唯一更多按钮。 */
QToolButton *zzMoreButton(ZzFluentUI::ZzCommandBar *bar)
{
    const auto buttons = bar->findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button->parent() == bar && button->menu() != nullptr) {
            return button;
        }
    }
    return nullptr;
}

/** @brief 返回命令栏的固定工具栏。 */
QToolBar *zzToolBar(ZzFluentUI::ZzCommandBar *bar)
{
    return bar->findChild<QToolBar *>();
}

/** @brief 创建使紧凑模式仍有可辨识图标的 action。 */
QAction *zzInsertPrimary(
    ZzFluentUI::ZzCommandBar *bar,
    int index,
    const QString &text)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::black);
    return bar->insertPrimaryAction(index, QIcon(pixmap), text);
}

} // namespace

/** @brief 验证自适应命令栏保持 QAction 原生协议。 */
class ZzCommandBarTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /** @brief 捕捉溢出时克隆 action 或修改外部 action parent 的破坏。 */
    void keepsOneActionIdentityAcrossOverflow()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction build(QStringLiteral("Build"), nullptr);
        build.setCheckable(true);
        QAction deploy(QStringLiteral("Deploy"), nullptr);
        QVERIFY(bar.insertPrimaryAction(0, &build));
        QVERIFY(bar.insertSecondaryAction(0, &deploy));
        bar.resize(120, 40);
        bar.show();
        zzFlushEvents();

        QVERIFY(bar.primaryActions().contains(&build));
        QVERIFY(bar.secondaryActions().contains(&deploy));
        build.trigger();
        QVERIFY(build.isChecked());
        QCOMPARE(build.parent(), nullptr);
    }

    /** @brief 捕捉同一 action 在多个分组出现的破坏。 */
    void rejectsDuplicateAndCrossGroupInsertion()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction action(QStringLiteral("Build"), nullptr);

        QVERIFY(bar.insertPrimaryAction(0, &action));
        QVERIFY(!bar.insertPrimaryAction(1, &action));
        QVERIFY(!bar.insertSecondaryAction(0, &action));
        QCOMPARE(bar.primaryActions(), QList<QAction *>({&action}));
        QVERIFY(bar.secondaryActions().isEmpty());
    }

    /** @brief 捕捉便利重载忘记让命令栏拥有新 action 的破坏。 */
    void ownsActionsCreatedByConvenienceOverload()
    {
        ZzFluentUI::ZzCommandBar bar;
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::black);

        QAction *action = bar.insertPrimaryAction(
            0,
            QIcon(pixmap),
            QStringLiteral("Build"));

        QVERIFY(action != nullptr);
        QCOMPARE(action->parent(), &bar);
        QCOMPARE(bar.primaryActions(), QList<QAction *>({action}));
    }

    /** @brief 捕捉追加重载破坏逻辑顺序、所有权或分组排他性的破坏。 */
    void appendsPrimaryAndSecondaryActionsThroughPublicOverloads()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction externalPrimary(QStringLiteral("External primary"), nullptr);
        QAction externalSecondary(QStringLiteral("External secondary"), nullptr);
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::black);

        bar.addPrimaryAction(nullptr);
        bar.addPrimaryAction(&externalPrimary);
        bar.addPrimaryAction(&externalPrimary);
        bar.addSecondaryAction(&externalPrimary);
        bar.addSecondaryAction(&externalSecondary);
        bar.addSecondaryAction(&externalSecondary);
        bar.addPrimaryAction(&externalSecondary);
        QAction *ownedPrimary = bar.addPrimaryAction(
            QIcon(pixmap), QStringLiteral("Owned primary"));
        QAction *ownedSecondary = bar.addSecondaryAction(
            QIcon(pixmap), QStringLiteral("Owned secondary"));

        QCOMPARE(
            bar.primaryActions(),
            QList<QAction *>({&externalPrimary, ownedPrimary}));
        QCOMPARE(
            bar.secondaryActions(),
            QList<QAction *>({&externalSecondary, ownedSecondary}));
        QCOMPARE(externalPrimary.parent(), nullptr);
        QCOMPARE(externalSecondary.parent(), nullptr);
        QCOMPARE(ownedPrimary->parent(), &bar);
        QCOMPARE(ownedSecondary->parent(), &bar);
    }

    /** @brief 捕捉原 action 触发时遗漏、重复或移除后继续通知的破坏。 */
    void forwardsOriginalActionTriggerExactlyOnceAcrossPresentationChanges()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction primary(QStringLiteral("Build"), nullptr);
        QAction secondary(QStringLiteral("Deploy"), nullptr);
        QSignalSpy triggered(&bar, &ZzFluentUI::ZzCommandBar::actionTriggered);
        QVERIFY(bar.insertPrimaryAction(0, &primary));
        QVERIFY(bar.insertSecondaryAction(0, &secondary));
        bar.resize(600, 40);
        bar.show();
        zzFlushEvents();

        primary.trigger();
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(triggered.at(0).at(0).value<QAction *>(), &primary);

        bar.resize(40, 40);
        zzFlushEvents();
        QVERIFY(zzMoreButton(&bar)->menu()->actions().contains(&primary));
        primary.trigger();
        QCOMPARE(triggered.count(), 2);
        QCOMPARE(triggered.at(1).at(0).value<QAction *>(), &primary);

        QVERIFY(bar.removeAction(&primary));
        primary.trigger();
        QCOMPARE(triggered.count(), 2);
    }

    /** @brief 捕捉可见主命令计数与真实工具栏或去重通知不一致的破坏。 */
    void reportsVisiblePrimaryActionCountAfterEachRealPresentationChange()
    {
        ZzFluentUI::ZzCommandBar bar;
        zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        zzInsertPrimary(&bar, 1, QStringLiteral("Test"));
        zzInsertPrimary(&bar, 2, QStringLiteral("Deploy"));
        bar.resize(600, 40);
        bar.show();
        zzFlushEvents();

        QCOMPARE(bar.visiblePrimaryActionCount(), 3);
        QCOMPARE(
            bar.visiblePrimaryActionCount(),
            zzToolBar(&bar)->actions().size());
        QSignalSpy changed(
            &bar,
            &ZzFluentUI::ZzCommandBar::visiblePrimaryActionCountChanged);
        QList<int> valuesVisibleToSlots;
        QObject::connect(
            &bar,
            &ZzFluentUI::ZzCommandBar::visiblePrimaryActionCountChanged,
            &bar,
            [&bar, &valuesVisibleToSlots](int) {
                valuesVisibleToSlots.append(bar.visiblePrimaryActionCount());
            });

        bar.setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Compact);
        zzFlushEvents();
        QCOMPARE(bar.visiblePrimaryActionCount(), 3);
        QCOMPARE(bar.visiblePrimaryActionCount(), zzToolBar(&bar)->actions().size());
        QCOMPARE(changed.count(), 0);

        bar.setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Auto);
        zzFlushEvents();
        QCOMPARE(changed.count(), 0);

        bar.resize(40, 40);
        zzFlushEvents();
        const int overflowCount = bar.visiblePrimaryActionCount();
        QVERIFY(overflowCount < 3);
        QCOMPARE(overflowCount, zzToolBar(&bar)->actions().size());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toInt(), overflowCount);
        QCOMPARE(valuesVisibleToSlots, QList<int>({overflowCount}));

        bar.resize(40, 40);
        zzFlushEvents();
        QCOMPARE(changed.count(), 1);

        bar.resize(600, 40);
        zzFlushEvents();
        QCOMPARE(bar.visiblePrimaryActionCount(), 3);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(valuesVisibleToSlots, QList<int>({overflowCount, 3}));

        QAction external(QStringLiteral("External"), nullptr);
        bar.addPrimaryAction(&external);
        QCOMPARE(bar.visiblePrimaryActionCount(), 4);
        QCOMPARE(changed.count(), 3);
        QVERIFY(bar.removeAction(&external));
        QCOMPARE(bar.visiblePrimaryActionCount(), 3);
        QCOMPARE(changed.count(), 4);

        QAction *destroyed = new QAction(QStringLiteral("Destroyed"), nullptr);
        bar.addPrimaryAction(destroyed);
        QCOMPARE(bar.visiblePrimaryActionCount(), 4);
        QCOMPARE(changed.count(), 5);
        delete destroyed;
        zzFlushEvents();
        QCOMPARE(bar.visiblePrimaryActionCount(), 3);
        QCOMPARE(changed.count(), 6);
        QCOMPARE(
            valuesVisibleToSlots,
            QList<int>({overflowCount, 3, 4, 3, 4, 3}));
    }

    /** @brief 捕捉外部 action 析构后遗留悬空记录的破坏。 */
    void removesDestroyedExternalAction()
    {
        ZzFluentUI::ZzCommandBar bar;
        auto *action = new QAction(QStringLiteral("Build"), nullptr);
        QVERIFY(bar.insertPrimaryAction(0, action));
        QPointer<QAction> guard(action);

        delete action;
        zzFlushEvents();

        QVERIFY(guard.isNull());
        QVERIFY(bar.primaryActions().isEmpty());
        QVERIFY(bar.secondaryActions().isEmpty());

        QAction replacement(QStringLiteral("Deploy"), nullptr);
        QVERIFY(!bar.insertPrimaryAction(1, &replacement));
        QVERIFY(bar.insertPrimaryAction(0, &replacement));
    }

    /** @brief 捕捉模式切换未将文字按钮压缩为图标按钮的破坏。 */
    void appliesExplicitDisplayModes()
    {
        ZzFluentUI::ZzCommandBar bar;
        zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        bar.resize(300, 40);
        bar.show();
        zzFlushEvents();

        bar.setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Expanded);
        zzFlushEvents();
        QCOMPARE(
            zzToolBar(&bar)->toolButtonStyle(),
            Qt::ToolButtonTextBesideIcon);

        bar.setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Compact);
        zzFlushEvents();
        QCOMPARE(zzToolBar(&bar)->toolButtonStyle(), Qt::ToolButtonIconOnly);
    }

    /** @brief 捕捉宽屏时次命令错误进入工具栏的破坏。 */
    void keepsSecondaryActionsInMoreMenuAtWideWidth()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction *build = zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::black);
        QAction *deploy = bar.insertSecondaryAction(
            0,
            QIcon(pixmap),
            QStringLiteral("Deploy"));
        bar.resize(600, 40);
        bar.show();
        zzFlushEvents();

        QToolButton *more = zzMoreButton(&bar);
        QVERIFY(more != nullptr);
        QVERIFY(more->isVisible());
        QCOMPARE(zzToolBar(&bar)->actions().size(), 1);
        QVERIFY(zzToolBar(&bar)->actions().contains(build));
        QVERIFY(!zzToolBar(&bar)->actions().contains(deploy));
        QVERIFY(more->menu()->actions().contains(deploy));
    }

    /** @brief 捕捉 Auto 未依次选择展开、紧凑和溢出的破坏。 */
    void selectsDeterministicAutoPresentationThresholds()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction *build = zzInsertPrimary(
            &bar, 0, QStringLiteral("Compile project"));
        QAction *test = zzInsertPrimary(
            &bar, 1, QStringLiteral("Deploy workspace"));
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::black);
        QAction *secondary = bar.insertSecondaryAction(
            0,
            QIcon(pixmap),
            QStringLiteral("Secondary"));
        QVERIFY(build != nullptr);
        QVERIFY(test != nullptr);
        QVERIFY(secondary != nullptr);

        bar.resize(600, 40);
        bar.show();
        zzFlushEvents();
        QCOMPARE(
            zzToolBar(&bar)->toolButtonStyle(),
            Qt::ToolButtonTextBesideIcon);
        QCOMPARE(zzToolBar(&bar)->actions().size(), 2);
        QVERIFY(zzToolBar(&bar)->actions().contains(build));
        QVERIFY(zzToolBar(&bar)->actions().contains(test));
        QVERIFY(zzMoreButton(&bar)->menu()->actions().contains(secondary));

        bar.resize(120, 40);
        zzFlushEvents();
        QCOMPARE(zzToolBar(&bar)->toolButtonStyle(), Qt::ToolButtonIconOnly);
        QCOMPARE(zzToolBar(&bar)->actions().size(), 2);
        QVERIFY(zzToolBar(&bar)->actions().contains(build));
        QVERIFY(zzToolBar(&bar)->actions().contains(test));
        QVERIFY(zzMoreButton(&bar)->menu()->actions().contains(secondary));

        bar.resize(40, 40);
        zzFlushEvents();
        QCOMPARE(zzToolBar(&bar)->toolButtonStyle(), Qt::ToolButtonIconOnly);
        QVERIFY(zzToolBar(&bar)->actions().size() < 2);
        QVERIFY(!zzToolBar(&bar)->actions().contains(test));
        QVERIFY(zzMoreButton(&bar)->menu()->actions().contains(test));
        QVERIFY(zzMoreButton(&bar)->menu()->actions().contains(secondary));
    }

    /** @brief 捕捉带子菜单 action 漏算箭头区域而错误留在工具栏的破坏。 */
    void accountsForMenuButtonExtentAtCriticalWidth()
    {
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::black);
        QAction plain(QIcon(pixmap), QStringLiteral("Deploy"), nullptr);
        QAction menuAction(QIcon(pixmap), QStringLiteral("Deploy"), nullptr);
        QMenu menu;
        menuAction.setMenu(&menu);
        QToolButton plainButton;
        plainButton.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        plainButton.setDefaultAction(&plain);
        QToolButton menuButton;
        menuButton.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        menuButton.setDefaultAction(&menuAction);
        QVERIFY(menuButton.sizeHint().width() > plainButton.sizeHint().width());

        ZzFluentUI::ZzCommandBar bar;
        QVERIFY(bar.insertPrimaryAction(0, &menuAction));
        bar.setDisplayMode(ZzFluentUI::ZzCommandBarDisplayMode::Expanded);
        bar.resize(plainButton.sizeHint().width(), 40);
        bar.show();
        zzFlushEvents();

        QToolButton *more = zzMoreButton(&bar);
        QVERIFY(more != nullptr);
        QVERIFY(more->isVisible());
        QVERIFY(more->menu()->actions().contains(&menuAction));
        QVERIFY(!zzToolBar(&bar)->actions().contains(&menuAction));
    }

    /** @brief 捕捉 Auto 模式未把逻辑尾部 action 迁移到更多菜单的破坏。 */
    void movesLogicalTailToOverflowInAutoMode()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction *build = zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        QAction *test = zzInsertPrimary(&bar, 1, QStringLiteral("Test"));
        QAction *deploy = zzInsertPrimary(&bar, 2, QStringLiteral("Deploy"));
        QVERIFY(build != nullptr);
        QVERIFY(test != nullptr);
        QVERIFY(deploy != nullptr);
        bar.resize(40, 40);
        bar.show();
        zzFlushEvents();

        QCOMPARE(bar.width(), 40);
        QToolButton *more = zzMoreButton(&bar);
        QVERIFY(more != nullptr);
        QVERIFY(more->isVisible());
        QVERIFY(more->menu()->actions().contains(deploy));
        QVERIFY(!zzToolBar(&bar)->actions().contains(deploy));
    }

    /** @brief 捕捉 overflow 中丢失 checked、enabled、shortcut 或子菜单的破坏。 */
    void preservesNativeActionStateInsideOverflow()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction *build = zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        QAction *deploy = zzInsertPrimary(&bar, 1, QStringLiteral("Deploy"));
        auto *subMenu = new QMenu(QStringLiteral("Deploy targets"), &bar);
        deploy->setCheckable(true);
        deploy->setChecked(true);
        deploy->setEnabled(false);
        deploy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
        deploy->setMenu(subMenu);
        bar.resize(40, 40);
        bar.show();
        zzFlushEvents();

        QVERIFY(build != nullptr);
        QToolButton *more = zzMoreButton(&bar);
        QVERIFY(more != nullptr);
        QVERIFY(more->menu()->actions().contains(deploy));
        QVERIFY(deploy->isChecked());
        QVERIFY(!deploy->isEnabled());
        QCOMPARE(deploy->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_D));
        QCOMPARE(deploy->menu(), subMenu);
    }

    /** @brief 捕捉更多菜单丢失主次命令分隔线的破坏。 */
    void separatesPrimaryAndSecondaryActionsInsideOverflow()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction *build = zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::black);
        QAction *deploy = bar.insertSecondaryAction(
            0,
            QIcon(pixmap),
            QStringLiteral("Deploy"));
        bar.resize(40, 40);
        bar.show();
        zzFlushEvents();

        QToolButton *more = zzMoreButton(&bar);
        QVERIFY(more != nullptr);
        const QList<QAction *> actions = more->menu()->actions();
        const qsizetype buildIndex = actions.indexOf(build);
        const qsizetype deployIndex = actions.indexOf(deploy);
        QVERIFY(buildIndex >= 0);
        QVERIFY(deployIndex > buildIndex + 1);
        QVERIFY(actions.at(deployIndex - 1)->isSeparator());
    }

    /** @brief 捕捉 RTL 下更多按钮仍固定在物理右侧的破坏。 */
    void putsOverflowControlAtVisualTrailingEdgeInRtl()
    {
        ZzFluentUI::ZzCommandBar bar;
        zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        zzInsertPrimary(&bar, 1, QStringLiteral("Deploy"));
        bar.setLayoutDirection(Qt::RightToLeft);
        bar.resize(40, 40);
        bar.show();
        zzFlushEvents();

        QToolButton *more = zzMoreButton(&bar);
        QVERIFY(more != nullptr);
        QVERIFY(more->isVisible());
        QVERIFY(more->geometry().center().x() < bar.width() / 2);
    }

    /** @brief 捕捉工具按钮无法通过键盘触发或丢失无障碍名称的破坏。 */
    void supportsKeyboardAndAccessibleName()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction build(QStringLiteral("Build command"), nullptr);
        QSignalSpy triggered(&build, &QAction::triggered);
        QVERIFY(bar.insertPrimaryAction(0, &build));
        bar.setAccessibleName(QStringLiteral("Workspace commands"));
        bar.resize(300, 40);
        bar.show();
        zzFlushEvents();

        QToolButton *button = qobject_cast<QToolButton *>(
            zzToolBar(&bar)->widgetForAction(&build));
        QVERIFY(button != nullptr);
        button->setFocus(Qt::TabFocusReason);
        QTest::keyClick(button, Qt::Key_Space);
        QCOMPARE(triggered.count(), 1);

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&bar);
        QVERIFY(interface != nullptr);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Workspace commands"));
    }

    /** @brief 捕捉反复迁移时新增 action、菜单或重复触发连接的破坏。 */
    void keepsObjectBudgetStableAcrossRepeatedResizes()
    {
        ZzFluentUI::ZzCommandBar bar;
        QAction *build = zzInsertPrimary(&bar, 0, QStringLiteral("Build"));
        zzInsertPrimary(&bar, 1, QStringLiteral("Test"));
        zzInsertPrimary(&bar, 2, QStringLiteral("Deploy"));
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::black);
        QAction *secondary = bar.insertSecondaryAction(
            0,
            QIcon(pixmap),
            QStringLiteral("Secondary"));
        QSignalSpy triggered(&bar, &ZzFluentUI::ZzCommandBar::actionTriggered);
        bar.resize(40, 40);
        bar.show();
        zzFlushEvents();
        const qsizetype actionCount = bar.findChildren<QAction *>().size();
        const qsizetype menuCount = bar.findChildren<QMenu *>().size();
        QToolButton *more = zzMoreButton(&bar);
        QVERIFY(more != nullptr);
        QVERIFY(more->menu()->actions().contains(build));
        QVERIFY(more->menu()->actions().contains(secondary));
        const qsizetype separatorCount = std::count_if(
            more->menu()->actions().cbegin(),
            more->menu()->actions().cend(),
            [](QAction *action) {
                return action->isSeparator();
            });
        QCOMPARE(separatorCount, 1);

        for (int index = 0; index < 1000; ++index) {
            const bool wide = index % 2 == 0;
            bar.resize(wide ? 1000 : 40, 40);
            zzFlushEvents();
            if (wide) {
                QVERIFY(zzToolBar(&bar)->actions().contains(build));
                QVERIFY(!more->menu()->actions().contains(build));
            } else {
                QVERIFY(!zzToolBar(&bar)->actions().contains(build));
                QVERIFY(more->menu()->actions().contains(build));
            }
        }
        build->trigger();

        QCOMPARE(bar.findChildren<QAction *>().size(), actionCount);
        QCOMPARE(bar.findChildren<QMenu *>().size(), menuCount);
        QCOMPARE(
            std::count_if(
                more->menu()->actions().cbegin(),
                more->menu()->actions().cend(),
                [](QAction *action) {
                    return action->isSeparator();
                }),
            separatorCount);
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(triggered.at(0).at(0).value<QAction *>(), build);
    }
};

QTEST_MAIN(ZzCommandBarTest)

#include "ZzCommandBarTest.moc"
