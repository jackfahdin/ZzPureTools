#include <QtTest/QTest>

#include <atomic>
#include <memory>
#include <thread>

#include <QtCore/QPoint>
#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzWindowKit/ZzWindowAgent.h>

#include "ZzWindowAgentTestAccess.h"
#include "private/ZzFakeWindowBackend.h"

/**
 * @brief 验证 ZzWindowAgent 状态机、对象归属与后端调用边界。
 */
class ZzWindowAgentTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void attachesOnlyOnce()
    {
        QWidget host;
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));

        QVERIFY(agent->attach(&host));
        QCOMPARE(agent->state(), ZzWindowKit::ZzWindowAgentState::Attached);
        QCOMPARE(backendPointer->attachCalls(), 1);
        QVERIFY(!agent->attach(&host));
        QCOMPARE(backendPointer->attachCalls(), 1);
    }

    void rejectsNonWindowHost()
    {
        QWidget host;
        QWidget child(&host);
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));

        const auto result = agent->attach(&child);
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(agent->state(), ZzWindowKit::ZzWindowAgentState::Detached);
        QCOMPARE(backendPointer->attachCalls(), 0);
    }

    void rejectsNonOwnerThread()
    {
        QWidget host;
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));
        std::atomic<ZzCore::ZzErrorCode> errorCode{
            ZzCore::ZzErrorCode::None};

        std::thread worker([&] {
            const auto result = agent->attach(&host);
            if (!result) {
                errorCode.store(
                    result.error().code(), std::memory_order_release);
            }
        });
        worker.join();

        QCOMPARE(
            errorCode.load(std::memory_order_acquire),
            ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(agent->state(), ZzWindowKit::ZzWindowAgentState::Detached);
        QCOMPARE(backendPointer->attachCalls(), 0);
    }

    void rejectsChromeFromAnotherWindow()
    {
        QWidget host;
        QWidget otherHost;
        QWidget foreignTitleBar(&otherHost);
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));
        QVERIFY(agent->attach(&host));

        ZzWindowKit::ZzWindowChromeConfiguration configuration;
        configuration.titleBar = &foreignTitleBar;
        const auto result = agent->configureChrome(configuration);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(agent->state(), ZzWindowKit::ZzWindowAgentState::Attached);
        QCOMPARE(backendPointer->configureCalls(), 0);
    }

    void configuresBackendInAttachedState()
    {
        QWidget host;
        QWidget titleBar(&host);
        QWidget icon(&titleBar);
        QWidget minimize(&titleBar);
        QWidget maximize(&titleBar);
        QWidget close(&titleBar);
        QWidget menu(&titleBar);
        menu.setObjectName(QStringLiteral("menu"));

        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));
        QVERIFY(agent->attach(&host));

        const ZzWindowKit::ZzWindowChromeConfiguration configuration{
            .titleBar = &titleBar,
            .windowIcon = &icon,
            .minimizeButton = &minimize,
            .maximizeButton = &maximize,
            .closeButton = &close,
            .interactiveWidgets = {&menu}};
        QVERIFY(agent->configureChrome(configuration));

        QCOMPARE(agent->state(), ZzWindowKit::ZzWindowAgentState::Configured);
        QCOMPARE(backendPointer->configureCalls(), 1);
        QCOMPARE(
            backendPointer->calls(),
            QStringList({
                QStringLiteral("attach"),
                QStringLiteral("title-bar"),
                QStringLiteral("icon"),
                QStringLiteral("minimize"),
                QStringLiteral("maximize"),
                QStringLiteral("close"),
                QStringLiteral("interactive:menu")}));
        QCOMPARE(backendPointer->lastConfiguration().titleBar, &titleBar);
    }

    void rejectsDuplicateChromeWidgets()
    {
        QWidget host;
        QWidget titleBar(&host);
        QWidget close(&titleBar);
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));
        QVERIFY(agent->attach(&host));

        ZzWindowKit::ZzWindowChromeConfiguration configuration;
        configuration.titleBar = &titleBar;
        configuration.closeButton = &close;
        configuration.interactiveWidgets = {&close};
        const auto result = agent->configureChrome(configuration);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(backendPointer->configureCalls(), 0);
    }

    void invalidatesWhenHostIsDestroyed()
    {
        auto host = std::make_unique<QWidget>();
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));
        QVERIFY(agent->attach(host.get()));

        host.reset();
        QCOMPARE(
            agent->state(),
            ZzWindowKit::ZzWindowAgentState::Invalidated);
        QVERIFY(!agent->setBackdrop(ZzWindowKit::ZzWindowBackdrop::None));
        QVERIFY(!agent->setColorScheme(
            ZzWindowKit::ZzWindowColorScheme::System));
        QVERIFY(!agent->showSystemMenu(QPoint(1, 2)));
        QCOMPARE(backendPointer->backdropCalls(), 0);
        QCOMPARE(backendPointer->colorSchemeCalls(), 0);
        QCOMPARE(backendPointer->systemMenuCalls(), 0);
    }

    void propagatesBackendFailure()
    {
        QWidget host;
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        backendPointer->setAttachResult(
            ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                ZzCore::ZzErrorCode::Backend,
                QStringLiteral("fake attach failure"))));
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));

        const auto result = agent->attach(&host);
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Backend);
        QCOMPARE(agent->state(), ZzWindowKit::ZzWindowAgentState::Failed);
        QCOMPARE(backendPointer->attachCalls(), 1);

        QWidget secondHost;
        QWidget titleBar(&secondHost);
        auto configureBackend =
            std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *configureBackendPointer = configureBackend.get();
        configureBackendPointer->setConfigureResult(
            ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                ZzCore::ZzErrorCode::Backend,
                QStringLiteral("fake configure failure"))));
        auto configureAgent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(configureBackend));
        QVERIFY(configureAgent->attach(&secondHost));
        ZzWindowKit::ZzWindowChromeConfiguration configuration;
        configuration.titleBar = &titleBar;

        const auto configureResult =
            configureAgent->configureChrome(configuration);
        QVERIFY(!configureResult);
        QCOMPARE(
            configureResult.error().code(),
            ZzCore::ZzErrorCode::Backend);
        QCOMPARE(
            configureAgent->state(),
            ZzWindowKit::ZzWindowAgentState::Failed);
        QCOMPARE(configureBackendPointer->configureCalls(), 1);
    }

    void forwardsEffectsAndMenuAfterAttach()
    {
        QWidget host;
        auto backend = std::make_unique<ZzWindowKit::ZzFakeWindowBackend>();
        auto *backendPointer = backend.get();
        backendPointer->setCapabilities(
            ZzWindowKit::ZzWindowCapability::SystemMenu
            | ZzWindowKit::ZzWindowCapability::Blur);
        auto agent = ZzWindowKit::ZzWindowAgentTestAccess::create(
            std::move(backend));
        QVERIFY(agent->attach(&host));

        QVERIFY(agent->capabilities().testFlag(
            ZzWindowKit::ZzWindowCapability::SystemMenu));
        const auto backdrop = agent->setBackdrop(
            ZzWindowKit::ZzWindowBackdrop::Blur);
        QVERIFY(backdrop);
        QCOMPARE(backdrop.value(), ZzWindowKit::ZzWindowApplyState::Applied);
        QVERIFY(agent->setColorScheme(
            ZzWindowKit::ZzWindowColorScheme::Dark));
        QVERIFY(agent->showSystemMenu(QPoint(10, 20)));
        QCOMPARE(backendPointer->backdropCalls(), 1);
        QCOMPARE(backendPointer->colorSchemeCalls(), 1);
        QCOMPARE(backendPointer->systemMenuCalls(), 1);
    }
};

QTEST_MAIN(ZzWindowAgentTest)

#include "ZzWindowAgentTest.moc"
