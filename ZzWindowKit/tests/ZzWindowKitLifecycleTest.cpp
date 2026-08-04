#include <QtTest/QTest>

#include <cstdlib>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowKitBootstrap.h>

/**
 * @brief 验证真实窗口后端在两种对象销毁顺序下的生命周期安全。
 */
class ZzWindowKitLifecycleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
