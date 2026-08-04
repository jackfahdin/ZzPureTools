#include <memory>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzNavigationView.h>

#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowCapability.h>
#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

namespace {

/** @brief 保存同一应用独占的两个窗口观察指针。 */
struct ZzWindowPair final
{
    ZzPureTools::ZzApplicationWindow *first = nullptr;
    ZzPureTools::ZzApplicationWindow *second = nullptr;
};

/** @brief 返回当前进程唯一的 PureTools 应用。 */
[[nodiscard]] ZzPureTools::ZzPureApplication &zzApplication()
{
    auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
    Q_ASSERT(application != nullptr);
    return *application;
}

/** @brief 创建不访问业务对象的测试页面注册。 */
[[nodiscard]] ZzPureTools::ZzPageRegistration zzPage(QString route)
{
    ZzPureTools::ZzPageRegistration registration;
    registration.routeId = ZzPureTools::ZzRouteId(std::move(route));
    registration.lifetime =
        ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    registration.factory =
        [](QWidget *pageParent)
        -> ZzCore::ZzResult<std::unique_ptr<
            ZzPureTools::ZzPageInstance>> {
            auto *view = new QWidget(pageParent);
            return ZzPureTools::ZzPageInstance::create(
                pageParent,
                view,
                std::make_unique<QObject>(),
                std::make_unique<QObject>());
        };
    return registration;
}

/** @brief 创建用于验证模型行到强类型路由映射的节点。 */
[[nodiscard]] ZzPureTools::ZzNavigationNode zzNode(
    QString route,
    QString title)
{
    return {
        ZzPureTools::ZzRouteId(std::move(route)),
        QStringLiteral("ZzMultiWindowIsolationTest"),
        std::move(title),
        {}};
}

/** @brief 为 Builder 注册两个页面、两个导航节点和初始路由。 */
[[nodiscard]] bool zzConfigure(
    ZzPureTools::ZzApplicationBuilder &builder)
{
    if (!builder.addPage(zzPage(QStringLiteral("home")))) {
        return false;
    }
    if (!builder.addPage(zzPage(QStringLiteral("details")))) {
        return false;
    }
    if (!builder.addNavigationNode(zzNode(
            QStringLiteral("home"), QStringLiteral("Home")))) {
        return false;
    }
    if (!builder.addNavigationNode(zzNode(
            QStringLiteral("details"), QStringLiteral("Details")))) {
        return false;
    }
    const auto initialRouteResult = builder.setInitialRoute(
        ZzPureTools::ZzRouteId(QStringLiteral("home")));
    return static_cast<bool>(initialRouteResult);
}

/** @brief 构建首窗并使用已提交配置创建第二个窗口。 */
[[nodiscard]] bool zzCreateWindowPair(
    ZzPureTools::ZzPureApplication &application,
    ZzWindowPair &pair)
{
    ZzPureTools::ZzApplicationBuilder builder;
    if (!zzConfigure(builder) || !builder.build(application)) {
        return false;
    }

    const auto topLevelWidgets = application.topLevelWidgets();
    for (QWidget *widget : topLevelWidgets) {
        if (auto *window = qobject_cast<
                ZzPureTools::ZzApplicationWindow *>(widget)) {
            pair.first = window;
            break;
        }
    }
    if (pair.first == nullptr) {
        return false;
    }

    auto secondResult = application.createWindow();
    if (!secondResult) {
        return false;
    }
    pair.second = std::move(secondResult).value();
    return pair.second != nullptr && pair.first != pair.second;
}

/** @brief 查找窗口内唯一 Fluent 标题栏。 */
[[nodiscard]] ZzFluentUI::ZzFluentTitleBar *zzTitleBar(
    ZzPureTools::ZzApplicationWindow *window)
{
    return window == nullptr
        ? nullptr
        : window->findChild<ZzFluentUI::ZzFluentTitleBar *>();
}

/** @brief 查找窗口内唯一导航视图。 */
[[nodiscard]] ZzFluentUI::ZzNavigationView *zzNavigationView(
    ZzPureTools::ZzApplicationWindow *window)
{
    return window == nullptr
        ? nullptr
        : window->findChild<ZzFluentUI::ZzNavigationView *>();
}

} // namespace

/** @brief 验证每个顶层窗口的代理、导航和关闭所有权彼此隔离。 */
class ZzMultiWindowIsolationTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyWindowOwnsDifferentWindowAgent()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));

        QVERIFY(windows.first->windowAgent() != nullptr);
        QVERIFY(windows.second->windowAgent() != nullptr);
        QVERIFY(windows.first->windowAgent()
                != windows.second->windowAgent());
        QCOMPARE(windows.first->windowAgent()->parent(), nullptr);
        QCOMPARE(windows.second->windowAgent()->parent(), nullptr);
        application.beginShutdown();
    }

    void everyWindowOwnsDifferentNavigationState()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));

        QVERIFY(windows.first->navigationModel()
                != windows.second->navigationModel());
        QVERIFY(windows.first->navigationController()
                != windows.second->navigationController());
        QVERIFY(windows.first->pageHost() != windows.second->pageHost());
        QCOMPARE(windows.first->navigationModel()->parent(), nullptr);
        QCOMPARE(windows.second->navigationModel()->parent(), nullptr);
        QCOMPARE(windows.first->navigationController()->parent(), nullptr);
        QCOMPARE(windows.second->navigationController()->parent(), nullptr);
        application.beginShutdown();
    }

    void titleBarIntentInvokesOnlyOwningWindow()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));
        auto *firstTitleBar = zzTitleBar(windows.first);
        auto *secondTitleBar = zzTitleBar(windows.second);
        QVERIFY(firstTitleBar != nullptr);
        QVERIFY(secondTitleBar != nullptr);
        auto *secondClose = qobject_cast<QAbstractButton *>(
            secondTitleBar->closeButton());
        QVERIFY(secondClose != nullptr);
        QPointer<ZzPureTools::ZzApplicationWindow> first(windows.first);
        QPointer<ZzPureTools::ZzApplicationWindow> second(windows.second);

        secondClose->click();

        QCOMPARE(application.windowCount(), 2);
        QTRY_COMPARE(application.windowCount(), 1);
        QVERIFY(!first.isNull());
        QVERIFY(second.isNull());
        application.beginShutdown();
    }

    void chromeConfigurationUsesTitleBarChildren()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));

        for (auto *window : {windows.first, windows.second}) {
            auto *titleBar = zzTitleBar(window);
            auto *navigationView = zzNavigationView(window);
            QVERIFY(titleBar != nullptr);
            QVERIFY(navigationView != nullptr);
            QVERIFY(window->isAncestorOf(titleBar));
            QVERIFY(window->isAncestorOf(navigationView));
            QCOMPARE(navigationView->model(), window->navigationModel());
            QCOMPARE(
                window->windowAgent()->state(),
                ZzWindowKit::ZzWindowAgentState::Configured);

            const QList<QWidget *> chromeWidgets{
                titleBar->windowIconWidget(),
                titleBar->minimizeButton(),
                titleBar->maximizeButton(),
                titleBar->closeButton()};
            for (QWidget *widget : chromeWidgets) {
                QVERIFY(widget != nullptr);
                QVERIFY(titleBar->isAncestorOf(widget));
                QVERIFY(window->isAncestorOf(widget));
            }
            for (QWidget *widget : titleBar->interactiveWidgets()) {
                QVERIFY(widget != nullptr);
                QVERIFY(titleBar->isAncestorOf(widget));
                QVERIFY(window->isAncestorOf(widget));
            }
        }
        application.beginShutdown();
    }

    void nativeSystemButtonCapabilityControlsCustomButtons()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));

        for (auto *window : {windows.first, windows.second}) {
            auto *titleBar = zzTitleBar(window);
            QVERIFY(titleBar != nullptr);
            const bool nativeButtons = window->windowAgent()
                ->capabilities()
                .testFlag(
                    ZzWindowKit::ZzWindowCapability::NativeSystemButtons);
            QCOMPARE(
                !titleBar->minimizeButton()->isHidden(), !nativeButtons);
            QCOMPARE(
                !titleBar->maximizeButton()->isHidden(), !nativeButtons);
            QCOMPARE(
                !titleBar->closeButton()->isHidden(), !nativeButtons);
        }
        application.beginShutdown();
    }

    void navigationIntentUsesTheOwningModelRoute()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));
        auto *firstView = zzNavigationView(windows.first);
        QVERIFY(firstView != nullptr);

        const QList<ZzPureTools::ZzNavigationNode> reordered{
            zzNode(QStringLiteral("details"), QStringLiteral("Details")),
            zzNode(QStringLiteral("home"), QStringLiteral("Home"))};
        QVERIFY(windows.first->navigationModel()->setNodes(reordered));
        Q_EMIT firstView->navigationRequested(
            windows.first->navigationModel()->index(0, 0));

        QCOMPARE(
            windows.first->navigationController()->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("details")));
        QCOMPARE(
            windows.second->navigationController()->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("home")));
        application.beginShutdown();
    }

    void topLevelWindowDisablesDeleteOnClose()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));

        for (auto *window : {windows.first, windows.second}) {
            QCOMPARE(window->parentWidget(), nullptr);
            QVERIFY(!window->testAttribute(Qt::WA_DeleteOnClose));
        }
        application.beginShutdown();
    }

    void acceptedCloseQueuesExactlyOneApplicationErase()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));
        QPointer<ZzPureTools::ZzApplicationWindow> closing(windows.second);

        QVERIFY(windows.second->close());
        QCOMPARE(application.windowCount(), 2);
        QVERIFY(!closing.isNull());
        QTRY_COMPARE(application.windowCount(), 1);
        QVERIFY(closing.isNull());

        for (int iteration = 0; iteration < 4; ++iteration) {
            QCoreApplication::processEvents();
        }
        QCOMPARE(application.windowCount(), 1);
        application.beginShutdown();
    }

    void metaObjectSignalWithoutAcceptedCloseCannotEraseWindow()
    {
        auto &application = zzApplication();
        ZzWindowPair windows;
        QVERIFY(zzCreateWindowPair(application, windows));
        QPointer<ZzPureTools::ZzApplicationWindow> observed(windows.second);

        QVERIFY(QMetaObject::invokeMethod(
            windows.second,
            "closeAccepted",
            Qt::DirectConnection));
        for (int iteration = 0; iteration < 4; ++iteration) {
            QCoreApplication::processEvents();
        }
        QCOMPARE(application.windowCount(), 2);
        QVERIFY(!observed.isNull());

        QVERIFY(windows.second->close());
        QTRY_COMPARE(application.windowCount(), 1);
        QVERIFY(observed.isNull());
        application.beginShutdown();
    }
};

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return 1;
    }
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzMultiWindowIsolationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzMultiWindowIsolationTest.moc"
