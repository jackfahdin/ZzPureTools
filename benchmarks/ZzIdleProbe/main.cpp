#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#endif

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzThemeController.h>

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

namespace {

constexpr int zzWarmupMilliseconds = 5000;
constexpr int zzMeasurementMilliseconds = 30000;
constexpr int zzExposeTimeoutMilliseconds = 2000;

/** @brief 保存 Linux 进程 user 与 system CPU tick。 */
struct ZzProcessCpuTimes final
{
    quint64 userTicks = 0;
    quint64 systemTicks = 0;
};

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出空闲 probe 失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 创建包含一个即时页面且无业务模块的应用配置。 */
ZzCore::ZzResult<void> zzConfigureApplication(
    ZzPureTools::ZzApplicationBuilder *builder)
{
    const ZzPureTools::ZzRouteId route(QStringLiteral("idle"));
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
        QStringLiteral("ZzIdleProbe"),
        QStringLiteral("Idle"),
        {}});
    if (!result) {
        return result;
    }
    return builder->setInitialRoute(route);
}

/** @brief 在不轮询的 Qt 事件循环中等待指定毫秒。 */
void zzWaitWithEventLoop(int milliseconds)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer.start(milliseconds);
    eventLoop.exec();
}

/** @brief 在有界时间内等待首窗 native handle exposed。 */
bool zzWaitForExposed(ZzPureTools::ZzApplicationWindow *window)
{
    QElapsedTimer timeout;
    timeout.start();
    while (window != nullptr
           && (window->windowHandle() == nullptr
               || !window->windowHandle()->isExposed())
           && timeout.elapsed() < zzExposeTimeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return window != nullptr && window->windowHandle() != nullptr
        && window->windowHandle()->isExposed();
}

/** @brief 按 comm 最后一个右括号解析 /proc/self/stat CPU 字段。 */
ZzCore::ZzResult<ZzProcessCpuTimes> zzReadCpuTimes()
{
#if !defined(Q_OS_LINUX)
    return ZzCore::ZzResult<ZzProcessCpuTimes>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Unsupported,
        QStringLiteral("idle CPU sampling is supported on Linux only")));
#else
    QFile stat(QStringLiteral("/proc/self/stat"));
    if (!stat.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ZzCore::ZzResult<ZzProcessCpuTimes>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to open /proc/self/stat"),
            stat.errorString()));
    }
    const QByteArray content = stat.readAll().trimmed();
    const qsizetype closeParenthesis = content.lastIndexOf(')');
    if (closeParenthesis < 0 || closeParenthesis + 2 >= content.size()
        || content.at(closeParenthesis + 1) != ' ') {
        return ZzCore::ZzResult<ZzProcessCpuTimes>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("invalid /proc/self/stat comm field")));
    }

    const QList<QByteArray> fields = content.sliced(closeParenthesis + 2)
                                         .simplified().split(' ');
    constexpr qsizetype userTimeIndex = 11;
    constexpr qsizetype systemTimeIndex = 12;
    if (fields.size() <= systemTimeIndex) {
        return ZzCore::ZzResult<ZzProcessCpuTimes>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("/proc/self/stat has too few fields")));
    }

    bool userValid = false;
    bool systemValid = false;
    const quint64 userTicks = fields.at(userTimeIndex).toULongLong(
        &userValid);
    const quint64 systemTicks = fields.at(systemTimeIndex).toULongLong(
        &systemValid);
    if (!userValid || !systemValid) {
        return ZzCore::ZzResult<ZzProcessCpuTimes>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("invalid CPU fields in /proc/self/stat")));
    }
    return ZzCore::ZzResult<ZzProcessCpuTimes>::success(
        {userTicks, systemTicks});
#endif
}

/** @brief 从 /proc/self/status 读取严格正数的 VmRSS 字节数。 */
ZzCore::ZzResult<qint64> zzReadResidentBytes()
{
#if !defined(Q_OS_LINUX)
    return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Unsupported,
        QStringLiteral("idle RSS sampling is supported on Linux only")));
#else
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to open /proc/self/status"),
            status.errorString()));
    }

    while (true) {
        const QByteArray line = status.readLine();
        if (line.isEmpty()) {
            break;
        }
        if (!line.startsWith("VmRSS:")) {
            continue;
        }
        const QList<QByteArray> fields = line.simplified().split(' ');
        bool converted = false;
        const qint64 kibibytes = fields.size() == 3
            && fields.at(0) == QByteArrayLiteral("VmRSS:")
            && fields.at(2) == QByteArrayLiteral("kB")
            ? fields.at(1).toLongLong(&converted) : 0;
        constexpr qint64 bytesPerKibibyte = 1024;
        if (!converted || kibibytes <= 0
            || kibibytes > std::numeric_limits<qint64>::max()
                / bytesPerKibibyte) {
            break;
        }
        return ZzCore::ZzResult<qint64>::success(
            kibibytes * bytesPerKibibyte);
    }
    return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Io,
        QStringLiteral("missing or invalid VmRSS in /proc/self/status")));
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
    application.themeController()->setReducedMotion(true);

    ZzPureTools::ZzApplicationBuilder builder;
    const auto configuration = zzConfigureApplication(&builder);
    if (!configuration) {
        return zzFail(configuration.error().technicalMessage());
    }
    const auto buildResult = builder.build(application);
    if (!buildResult) {
        return zzFail(buildResult.error().technicalMessage());
    }

    ZzPureTools::ZzApplicationWindow *window = nullptr;
    for (QWidget *candidate : application.topLevelWidgets()) {
        window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(candidate);
        if (window != nullptr) {
            break;
        }
    }
    if (!zzWaitForExposed(window)) {
        return zzFail(QStringLiteral("idle probe window was not exposed"));
    }

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("idle"));
    reporter.setWarmupIterations(5);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, window->screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    zzWaitWithEventLoop(zzWarmupMilliseconds);
    const auto startCpu = zzReadCpuTimes();
    const auto startRss = zzReadResidentBytes();
    if (!startCpu) {
        return zzFail(startCpu.error().technicalMessage());
    }
    if (!startRss) {
        return zzFail(startRss.error().technicalMessage());
    }

    QElapsedTimer wallTimer;
    wallTimer.start();
    zzWaitWithEventLoop(zzMeasurementMilliseconds);
    const qint64 wallNanoseconds = wallTimer.nsecsElapsed();
    const auto endCpu = zzReadCpuTimes();
    const auto endRss = zzReadResidentBytes();
    if (!endCpu) {
        return zzFail(endCpu.error().technicalMessage());
    }
    if (!endRss) {
        return zzFail(endRss.error().technicalMessage());
    }

#if !defined(Q_OS_LINUX)
    return zzFail(QStringLiteral("idle metrics are supported on Linux only"));
#else
    const long clockTicksPerSecond = sysconf(_SC_CLK_TCK);
    const quint64 startTicks = startCpu.value().userTicks
        + startCpu.value().systemTicks;
    const quint64 endTicks = endCpu.value().userTicks
        + endCpu.value().systemTicks;
    constexpr double nanosecondsPerSecond = 1'000'000'000.0;
    const double wallSeconds = static_cast<double>(wallNanoseconds)
        / nanosecondsPerSecond;
    if (clockTicksPerSecond <= 0 || endTicks < startTicks
        || wallSeconds <= 0.0) {
        return zzFail(QStringLiteral("invalid idle CPU measurement interval"));
    }
    const double cpuPercent =
        static_cast<double>(endTicks - startTicks)
        / static_cast<double>(clockTicksPerSecond)
        / wallSeconds * 100.0;
    const double rawGrowthPercent =
        static_cast<double>(endRss.value() - startRss.value())
        * 100.0 / static_cast<double>(startRss.value());

    reporter.addSample({
        QStringLiteral("average-cpu-percent"),
        QStringLiteral("percent"),
        cpuPercent});
    reporter.addSample({
        QStringLiteral("rss-start-bytes"),
        QStringLiteral("bytes"),
        static_cast<double>(startRss.value())});
    reporter.addSample({
        QStringLiteral("rss-end-bytes"),
        QStringLiteral("bytes"),
        static_cast<double>(endRss.value())});
    reporter.addSample({
        QStringLiteral("rss-growth-percent"),
        QStringLiteral("percent"),
        std::max(0.0, rawGrowthPercent)});

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
#endif
}
