#include <QtTest/QTest>

#include <cstdlib>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowApplyState.h>
#include <ZzWindowKit/ZzWindowBackdrop.h>
#include <ZzWindowKit/ZzWindowCapability.h>
#include <ZzWindowKit/ZzWindowKitBootstrap.h>

/**
 * @brief 验证真实窗口后端在两种对象销毁顺序下的生命周期安全。
 */
class ZzWindowKitLifecycleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
#if defined(Q_OS_MACOS)
    /** @brief 验证非 Cocoa QPA 在创建原生后端前被明确拒绝。 */
    void rejectsOffscreenBeforeCreatingNativeBackend()
    {
        if (QGuiApplication::platformName()
            != QStringLiteral("offscreen")) {
            QSKIP("This scenario requires the offscreen Qt platform");
        }

        QWidget window;
        ZzWindowKit::ZzWindowAgent agent;
        const auto result = agent.attach(&window);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Unsupported);
        QCOMPARE(agent.state(), ZzWindowKit::ZzWindowAgentState::Failed);
    }
#endif

    /** @brief 验证 Automatic 在原生窗口创建后落地且保留唯一后备层。 */
    void automaticSettlesAfterNativeWindowCreation()
    {
        QWidget window;
        window.resize(320, 180);
        ZzWindowKit::ZzWindowAgent agent;
        QVERIFY(agent.attach(&window));
        window.show();
        QVERIFY(window.winId() != 0);
        QVERIFY(QTest::qWaitFor([&window] {
            return window.testAttribute(Qt::WA_WState_Created)
                && window.windowHandle() != nullptr;
        }));

        const auto result = agent.setBackdrop(
            ZzWindowKit::ZzWindowBackdrop::Automatic);
        QVERIFY(result);
        QCOMPARE(result.value(), ZzWindowKit::ZzWindowApplyState::Applied);
        auto *layer = window.findChild<QWidget *>(
            QStringLiteral("zzSoftwareBackdropLayer"));
        QVERIFY(layer != nullptr);
        QCOMPARE(
            window.findChildren<QWidget *>(
                QStringLiteral("zzSoftwareBackdropLayer"))
                .size(),
            qsizetype{1});
        const auto capabilities = agent.capabilities();
        const bool hasNativeBackdrop =
            capabilities.testFlag(ZzWindowKit::ZzWindowCapability::Blur)
            || capabilities.testFlag(
                ZzWindowKit::ZzWindowCapability::Acrylic)
            || capabilities.testFlag(ZzWindowKit::ZzWindowCapability::Mica)
            || capabilities.testFlag(
                ZzWindowKit::ZzWindowCapability::MicaAlt);
        if (!hasNativeBackdrop) {
            QVERIFY(!layer->isHidden());
        }

        const auto disabled = agent.setBackdrop(
            ZzWindowKit::ZzWindowBackdrop::None);
        QVERIFY(disabled);
        QCOMPARE(disabled.value(), ZzWindowKit::ZzWindowApplyState::Applied);
        QVERIFY(layer->isHidden());
    }

    void destroysAgentBeforeWindow()
    {
        for (int index = 0; index < 100; ++index) {
            auto window = std::make_unique<QWidget>();
            auto agent =
                std::make_unique<ZzWindowKit::ZzWindowAgent>();
            QVERIFY(agent->attach(window.get()));
            window->show();
            QCoreApplication::processEvents();
            window->close();
            QCoreApplication::processEvents();
            agent.reset();
            window.reset();
            QCoreApplication::sendPostedEvents(
                nullptr, QEvent::DeferredDelete);
        }
    }

    void destroysWindowBeforeAgent()
    {
        for (int index = 0; index < 100; ++index) {
            auto window = std::make_unique<QWidget>();
            auto agent =
                std::make_unique<ZzWindowKit::ZzWindowAgent>();
            QVERIFY(agent->attach(window.get()));
            window->show();
            QCoreApplication::processEvents();
            window.reset();
            QCOMPARE(
                agent->state(),
                ZzWindowKit::ZzWindowAgentState::Invalidated);
            agent.reset();
            QCoreApplication::sendPostedEvents(
                nullptr, QEvent::DeferredDelete);
        }
    }
};

int main(int argc, char *argv[])
{
    const auto prepared = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!prepared) {
        return EXIT_FAILURE;
    }
    QApplication application(argc, argv);
    ZzWindowKitLifecycleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzWindowKitLifecycleTest.moc"
