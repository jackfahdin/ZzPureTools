#include <QtTest/QTest>
#include <ZzTestEventLoop.h>

#include <cstdlib>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
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

#if defined(Q_OS_WIN)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

/**
 * @brief 验证真实窗口后端在两种对象销毁顺序下的生命周期安全。
 */
class ZzWindowKitLifecycleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void alwaysOnTopAllowsReentrantMaximize()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        QWidget window;
        window.resize(640, 480);
        ZzWindowKit::ZzWindowAgent agent;
        QVERIFY(agent.attach(&window));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QObject::connect(window.windowHandle(), &QWindow::flagsChanged, &window,
            [&window](Qt::WindowFlags flags) {
                if (flags.testFlag(Qt::WindowStaysOnTopHint)) window.showMaximized();
            });
        QVERIFY(agent.setAlwaysOnTop(true));
        ZZ_VERIFY_EVENTUALLY(window.isMaximized());
#if defined(Q_OS_WIN)
        if (QGuiApplication::platformName() == QStringLiteral("windows")) {
            const auto hwnd = reinterpret_cast<HWND>(window.winId());
            QVERIFY(IsZoomed(hwnd));
            QVERIFY((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
        }
#endif
#else
        QSKIP("QWindow::flagsChanged requires Qt 6.10 or newer");
#endif
    }

    void alwaysOnTopPreservesWindowPlacement_data()
    {
        QTest::addColumn<bool>("maximized");
        QTest::newRow("normal") << false;
        QTest::newRow("maximized") << true;
    }

    /** @brief 置顶只改变层级，保留最大化状态、原生边框与客户区位置。 */
    void alwaysOnTopPreservesWindowPlacement()
    {
        QFETCH(bool, maximized);
        QWidget window;
        window.setGeometry(100, 100, 640, 480);
        ZzWindowKit::ZzWindowAgent agent;
        QVERIFY(agent.attach(&window));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QRect restoredGeometry = window.geometry();
        if (maximized) {
            window.showMaximized();
            ZZ_VERIFY_EVENTUALLY(window.isMaximized());
        }
        QCoreApplication::processEvents();
        const auto windowId = window.winId();
        const auto state = window.windowState();
        const auto geometry = window.geometry();
        const auto frameGeometry = window.frameGeometry();
        const auto normalGeometry = window.normalGeometry();

#if defined(Q_OS_WIN)
        const bool nativeWindows = QGuiApplication::platformName()
            .compare(QStringLiteral("windows"), Qt::CaseInsensitive) == 0;
        const auto hwnd = reinterpret_cast<HWND>(windowId);
        RECT originalWindowRect{};
        RECT originalClientRect{};
        POINT originalClientOrigin{};
        if (nativeWindows) {
            QCOMPARE(IsZoomed(hwnd) != FALSE, maximized);
            QVERIFY(GetWindowRect(hwnd, &originalWindowRect));
            QVERIFY(GetClientRect(hwnd, &originalClientRect));
            QVERIFY(ClientToScreen(hwnd, &originalClientOrigin));
        }
#endif

        // Repeated requests must also be harmless; exercise both pin and unpin.
        for (const bool requested : {true, true, false, false, true, false}) {
            QVERIFY(agent.setAlwaysOnTop(requested));
            QCoreApplication::processEvents();
            QCOMPARE(window.winId(), windowId);
            QVERIFY(window.isVisible());
            QCOMPARE(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint), requested);
            QCOMPARE(window.windowHandle()->flags().testFlag(Qt::WindowStaysOnTopHint), requested);
            QCOMPARE(window.windowState(), state);
            QCOMPARE(window.geometry(), geometry);
            QCOMPARE(window.frameGeometry(), frameGeometry);
            QCOMPARE(window.normalGeometry(), normalGeometry);
#if defined(Q_OS_WIN)
            if (nativeWindows) {
                QCOMPARE(IsZoomed(hwnd) != FALSE, maximized);
                QCOMPARE((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0,
                    requested);
                RECT currentWindowRect{};
                RECT currentClientRect{};
                POINT currentClientOrigin{};
                QVERIFY(GetWindowRect(hwnd, &currentWindowRect));
                QVERIFY(GetClientRect(hwnd, &currentClientRect));
                QVERIFY(ClientToScreen(hwnd, &currentClientOrigin));
                QVERIFY(EqualRect(&currentWindowRect, &originalWindowRect));
                QVERIFY(EqualRect(&currentClientRect, &originalClientRect));
                QCOMPARE(currentClientOrigin.x, originalClientOrigin.x);
                QCOMPARE(currentClientOrigin.y, originalClientOrigin.y);
                if (requested) {
                    // Qt's lower() consults QWindow flags, not QWidget flags.
                    window.lower();
                    QCoreApplication::processEvents();
                    QVERIFY((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
                    QCOMPARE(IsZoomed(hwnd) != FALSE, maximized);
                }
            }
#endif
        }
#if defined(Q_OS_WIN)
        if (nativeWindows) {
            QVERIFY(agent.setAlwaysOnTop(true));
            window.hide();
            QVERIFY(agent.setAlwaysOnTop(false));
            QVERIFY(!window.isVisible());
            QVERIFY(!window.windowHandle()->flags().testFlag(Qt::WindowStaysOnTopHint));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QCOMPARE(window.winId(), windowId);
            QCOMPARE(window.windowState(), state);
            QCOMPARE(window.geometry(), geometry);
            QCOMPARE(IsZoomed(hwnd) != FALSE, maximized);
            QVERIFY((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0);
        }
#endif
        if (maximized) {
            window.showNormal();
            ZZ_VERIFY_EVENTUALLY(!window.isMaximized());
            // Offscreen has no window manager to restore the normal rectangle.
            if (QGuiApplication::platformName() == QStringLiteral("windows"))
                ZZ_COMPARE_EVENTUALLY(window.geometry(), restoredGeometry);
        }
    }

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
