#include "ZzBenchmarkMetadata.h"

#include <cmath>
#include <limits>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QSysInfo>
#include <QtCore/QStringView>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include "ZzPerformanceReporter.h"

namespace ZzBenchmarks {

namespace {

ZzCore::ZzResult<QString> readProcValue(
    const QString &path,
    const QByteArray &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ZzCore::ZzResult<QString>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to open Linux process metadata"),
            path));
    }

    while (true) {
        const QByteArray line = file.readLine();
        if (line.isEmpty()) {
            break;
        }
        const qsizetype separator = line.indexOf(':');
        if (separator < 0 || line.first(separator).trimmed() != key) {
            continue;
        }
        const QString value = QString::fromUtf8(
            line.sliced(separator + 1).trimmed());
        if (!value.isEmpty()) {
            return ZzCore::ZzResult<QString>::success(value);
        }
    }

    return ZzCore::ZzResult<QString>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Io,
        QStringLiteral("missing Linux process metadata field: %1:%2")
            .arg(path, QString::fromLatin1(key)),
        QStringLiteral("%1:%2").arg(path, QString::fromLatin1(key))));
}

ZzCore::ZzResult<qint64> readMemoryBytes()
{
    const auto memoryResult = readProcValue(
        QStringLiteral("/proc/meminfo"),
        QByteArrayLiteral("MemTotal"));
    if (!memoryResult) {
        return ZzCore::ZzResult<qint64>::failure(memoryResult.error());
    }

    static const QRegularExpression memoryPattern(
        QStringLiteral("^([0-9]+)\\s+kB$"));
    const auto match = memoryPattern.match(memoryResult.value());
    bool converted = false;
    const qint64 kibibytes = match.hasMatch()
        ? match.captured(1).toLongLong(&converted)
        : 0;
    constexpr qint64 bytesPerKibibyte = 1024;
    if (!converted
        || kibibytes > std::numeric_limits<qint64>::max()
            / bytesPerKibibyte) {
        return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("invalid MemTotal value"),
            memoryResult.value()));
    }
    return ZzCore::ZzResult<qint64>::success(
        kibibytes * bytesPerKibibyte);
}

ZzCore::ZzResult<void> invalidEnvironment(QString message)
{
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidArgument,
        std::move(message)));
}

} // namespace

ZzCore::ZzResult<void> ZzBenchmarkMetadata::populate(
    ZzPerformanceReporter &reporter,
    const QScreen *screen)
{
#if !defined(Q_OS_LINUX)
    Q_UNUSED(reporter);
    Q_UNUSED(screen);
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Unsupported,
        QStringLiteral("benchmark metadata is currently supported on Linux")));
#else
    if (screen == nullptr) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("benchmark requires an active screen")));
    }

    const auto cpuResult = readProcValue(
        QStringLiteral("/proc/cpuinfo"),
        QByteArrayLiteral("model name"));
    if (!cpuResult) {
        return ZzCore::ZzResult<void>::failure(cpuResult.error());
    }
    const auto memoryResult = readMemoryBytes();
    if (!memoryResult) {
        return ZzCore::ZzResult<void>::failure(memoryResult.error());
    }

    const QString commit = qEnvironmentVariable("ZZ_BENCHMARK_COMMIT");
    const QString preset = qEnvironmentVariable("ZZ_CMAKE_PRESET");
    const QString runnerDigest =
        qEnvironmentVariable("ZZ_RUNNER_IMAGE_DIGEST");
    const QString gpu = qEnvironmentVariable("ZZ_GPU_IDENTITY").trimmed();
    static const QRegularExpression commitPattern(
        QStringLiteral("^[0-9a-f]{40}$"));
    static const QRegularExpression digestPattern(
        QStringLiteral("^sha256:[0-9a-f]{64}$"));
    if (!commitPattern.match(commit).hasMatch()) {
        return invalidEnvironment(
            QStringLiteral("ZZ_BENCHMARK_COMMIT has invalid format"));
    }
    if (preset.isEmpty()) {
        return invalidEnvironment(
            QStringLiteral("ZZ_CMAKE_PRESET must not be empty"));
    }
    if (!digestPattern.match(runnerDigest).hasMatch()) {
        return invalidEnvironment(
            QStringLiteral("ZZ_RUNNER_IMAGE_DIGEST has invalid format"));
    }
    if (gpu.isEmpty()
        || gpu.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0) {
        return invalidEnvironment(
            QStringLiteral("ZZ_GPU_IDENTITY must identify renderer and driver"));
    }

    const double dpr = screen->devicePixelRatio();
    const double refreshRate = screen->refreshRate();
    if (!std::isfinite(dpr) || dpr <= 0.0
        || !std::isfinite(refreshRate) || refreshRate <= 0.0) {
        return invalidEnvironment(
            QStringLiteral("screen DPR and refresh rate must be positive"));
    }

    const QString os = QSysInfo::prettyProductName().trimmed();
    const QString windowSystem = QGuiApplication::platformName().trimmed();
    if (os.isEmpty() || windowSystem.isEmpty()) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Qt platform metadata is incomplete")));
    }

    reporter.addEnvironmentMetadata(QStringLiteral("cpu"), cpuResult.value());
    reporter.addEnvironmentMetadata(
        QStringLiteral("memoryBytes"),
        QJsonValue(memoryResult.value()));
    reporter.addEnvironmentMetadata(QStringLiteral("os"), os);
    reporter.addEnvironmentMetadata(QStringLiteral("gpu"), gpu);
    reporter.addEnvironmentMetadata(
        QStringLiteral("windowSystem"), windowSystem);
    reporter.addEnvironmentMetadata(QStringLiteral("dpr"), dpr);
    reporter.addEnvironmentMetadata(
        QStringLiteral("refreshRateHz"), refreshRate);
    reporter.addEnvironmentMetadata(
        QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    reporter.addEnvironmentMetadata(
        QStringLiteral("compiler"),
        QString::fromUtf8(ZZ_BENCHMARK_COMPILER));
    reporter.addEnvironmentMetadata(
        QStringLiteral("runnerImageDigest"), runnerDigest);

    reporter.addBuildMetadata(QStringLiteral("commit"), commit);
    reporter.addBuildMetadata(QStringLiteral("preset"), preset);
    reporter.addBuildMetadata(
        QStringLiteral("buildType"),
        QString::fromUtf8(ZZ_BENCHMARK_BUILD_TYPE));
    reporter.addBuildMetadata(
        QStringLiteral("shared"),
        static_cast<bool>(ZZ_BENCHMARK_SHARED));
    reporter.addBuildMetadata(
        QStringLiteral("lto"),
        static_cast<bool>(ZZ_BENCHMARK_LTO));
    reporter.addBuildMetadata(
        QStringLiteral("sanitizers"),
        QString::fromUtf8(ZZ_BENCHMARK_SANITIZERS));
    return ZzCore::ZzResult<void>::success();
#endif
}

} // namespace ZzBenchmarks
