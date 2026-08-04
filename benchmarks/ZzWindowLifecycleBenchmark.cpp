#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QPointer>
#include <QtCore/QTextStream>
#include <QtGui/QWindow>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#endif

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"
#include "ZzWindowKitDiagnostics.h"

namespace {

constexpr int zzMeasuredIterations = 100;
constexpr int zzWindowTimeoutMilliseconds = 2000;

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出窗口生命周期基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 配置只包含一个即时页面的最小应用。 */
ZzCore::ZzResult<void> zzConfigureApplication(
    ZzPureTools::ZzApplicationBuilder *builder)
{
    const ZzPureTools::ZzRouteId route(QStringLiteral("lifecycle"));
    ZzPureTools::ZzPageRegistration page;
    page.routeId = route;
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::Persistent;
    page.factory = [](QWidget *parent) {
        return ZzPureTools::ZzPageInstance::create(
            parent,
            new QWidget(parent),
            std::make_unique<QObject>(),
            std::make_unique<QObject>());
    };
    auto result = builder->addPage(std::move(page));
    if (!result) {
        return result;
    }

    result = builder->addNavigationNode({
        route,
        QStringLiteral("ZzWindowLifecycleBenchmark"),
        QStringLiteral("Lifecycle"),
        {}});
    if (!result) {
        return result;
    }
    return builder->setInitialRoute(route);
}

/** @brief 在超时内等待窗口获得真实 exposed native handle。 */
bool zzWaitForExposed(ZzPureTools::ZzApplicationWindow *window)
{
    QElapsedTimer timeout;
    timeout.start();
    while (window != nullptr
           && (window->windowHandle() == nullptr
               || !window->windowHandle()->isExposed())
           && timeout.elapsed() < zzWindowTimeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QTest::qWait(1);
    }
    return window != nullptr && window->windowHandle() != nullptr
        && window->windowHandle()->isExposed();
}

/** @brief 等待应用消费 queued close 并销毁窗口所有权。 */
bool zzWaitForDestroyed(
    const QPointer<ZzPureTools::ZzApplicationWindow> &window)
{
    QElapsedTimer timeout;
    timeout.start();
    while (!window.isNull()
           && timeout.elapsed() < zzWindowTimeoutMilliseconds) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QTest::qWait(1);
    }
    return window.isNull();
}

/** @brief 读取 Linux 当前进程 resident set size 字节数。 */
ZzCore::ZzResult<qint64> zzResidentBytes()
{
#if !defined(Q_OS_LINUX)
    return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Unsupported,
        QStringLiteral("RSS sampling is currently supported on Linux")));
#else
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (!statm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to open /proc/self/statm"),
            statm.errorString()));
    }
    qint64 virtualPages = 0;
    qint64 residentPages = 0;
    QTextStream stream(&statm);
    stream >> virtualPages >> residentPages;
    Q_UNUSED(virtualPages)
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (stream.status() != QTextStream::Ok || residentPages < 0
        || pageSize <= 0) {
        return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("invalid /proc/self/statm contents")));
    }
    return ZzCore::ZzResult<qint64>::success(
        residentPages * static_cast<qint64>(pageSize));
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return zzFail(bootstrap.error().technicalMessage());
    }

    ZzPureTools::ZzPureApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    ZzPureTools::ZzApplicationBuilder builder;
    const auto configuration = zzConfigureApplication(&builder);
    if (!configuration) {
        return zzFail(configuration.error().technicalMessage());
    }
    const auto buildResult = builder.build(application);
    if (!buildResult) {
        return zzFail(buildResult.error().technicalMessage());
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    ZzPureTools::ZzApplicationWindow *initialWindow = nullptr;
    for (QWidget *candidate : application.topLevelWidgets()) {
        initialWindow = qobject_cast<ZzPureTools::ZzApplicationWindow *>(
            candidate);
        if (initialWindow != nullptr) {
            break;
        }
    }
    if (!zzWaitForExposed(initialWindow)) {
        return zzFail(QStringLiteral("initial application window was not exposed"));
    }

    const qsizetype baselineTopLevels =
        QApplication::topLevelWidgets().size();
    const qsizetype baselineWindows = application.windowCount();
    const qsizetype baselineBackends = ZzWindowKit::Internal::
        ZzWindowKitDiagnostics::liveBackendCount();
    const qsizetype baselineAgents = ZzWindowKit::Internal::
        ZzWindowKitDiagnostics::liveAgentCount();

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("window-lifecycle"));
    reporter.setWarmupIterations(0);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, initialWindow->screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    for (int iteration = 0; iteration < zzMeasuredIterations; ++iteration) {
        QElapsedTimer timer;
        timer.start();
        const auto windowResult = application.createWindow();
        if (!windowResult) {
            return zzFail(windowResult.error().technicalMessage());
        }
        QPointer<ZzPureTools::ZzApplicationWindow> window(
            windowResult.value());
        if (!zzWaitForExposed(window.data())) {
            return zzFail(QStringLiteral("created window was not exposed"));
        }
        if (!window->close()) {
            return zzFail(QStringLiteral("created window rejected close"));
        }
        if (!zzWaitForDestroyed(window)) {
            return zzFail(QStringLiteral(
                "application did not destroy a closed owned window"));
        }
        const qint64 elapsedNanoseconds = timer.nsecsElapsed();
        const auto resident = zzResidentBytes();
        if (!resident) {
            return zzFail(resident.error().technicalMessage());
        }

        if (QApplication::topLevelWidgets().size() != baselineTopLevels
            || application.windowCount() != baselineWindows
            || ZzWindowKit::Internal::ZzWindowKitDiagnostics::
                   liveBackendCount() != baselineBackends
            || ZzWindowKit::Internal::ZzWindowKitDiagnostics::
                   liveAgentCount() != baselineAgents) {
            return zzFail(QStringLiteral(
                "window lifecycle counters did not return to baseline at iteration %1")
                              .arg(iteration));
        }

        reporter.addSample({
            QStringLiteral("lifecycle-time"),
            QStringLiteral("ms"),
            static_cast<double>(elapsedNanoseconds)
                / nanosecondsPerMillisecond});
        reporter.addSample({
            QStringLiteral("rss-bytes"),
            QStringLiteral("bytes"),
            static_cast<double>(resident.value())});
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
