#include <array>
#include <cstdio>
#include <cstdlib>

#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtGui/QGuiApplication>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"
#include "ZzStartupProbePath.h"

namespace {

constexpr int zzWarmupIterations = 5;
constexpr int zzMeasuredIterations = 30;

/** @brief 返回命令行中 --report 后的非空输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出启动基准失败原因并返回通用失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 严格解析探针唯一一行 JSON 输出。 */
bool zzParseMarkers(
    const QByteArray &output,
    QJsonObject *markers,
    QString *failure)
{
    const QList<QByteArray> lines = output.trimmed().split('\n');
    if (lines.size() != 1 || lines.constFirst().trimmed().isEmpty()) {
        *failure = QStringLiteral("startup probe must output one JSON line");
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        lines.constFirst(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        *failure = QStringLiteral("invalid startup marker JSON: %1")
                       .arg(error.errorString());
        return false;
    }

    static constexpr std::array markerNames = {
        "process-entry",
        "qt-created",
        "modules-started",
        "page-created",
        "first-paint"
    };
    const QJsonObject parsed = document.object();
    if (parsed.size() != static_cast<qsizetype>(markerNames.size())) {
        *failure = QStringLiteral("startup marker schema has extra or missing fields");
        return false;
    }

    qint64 previous = -1;
    for (const char *name : markerNames) {
        const QJsonValue value = parsed.value(QLatin1StringView(name));
        const qint64 nanoseconds = value.toInteger(-1);
        if (!value.isDouble() || nanoseconds < 0
            || (previous >= 0 && nanoseconds <= previous)) {
            *failure = QStringLiteral("startup markers are not strictly increasing at %1")
                           .arg(QLatin1StringView(name));
            return false;
        }
        previous = nanoseconds;
    }
    if (parsed.value(QStringLiteral("process-entry")).toInteger(-1) != 0) {
        *failure = QStringLiteral("process-entry marker must equal zero");
        return false;
    }

    *markers = parsed;
    return true;
}

/** @brief 把除入口外的累计内部标记转换为毫秒样本。 */
void zzAddMarkerSamples(
    ZzBenchmarks::ZzPerformanceReporter *reporter,
    const QJsonObject &markers)
{
    static constexpr std::array markerNames = {
        "qt-created",
        "modules-started",
        "page-created",
        "first-paint"
    };
    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    for (const char *name : markerNames) {
        reporter->addSample({
            QString::fromLatin1(name),
            QStringLiteral("ms"),
            static_cast<double>(
                markers.value(QLatin1StringView(name)).toInteger())
                / nanosecondsPerMillisecond});
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    const QString probePath = QString::fromUtf8(ZzStartupProbePath);
    if (!QFileInfo(probePath).isAbsolute()
        || !QFileInfo(probePath).isExecutable()) {
        return zzFail(
            QStringLiteral("invalid startup probe: %1").arg(probePath));
    }

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("startup"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, QGuiApplication::primaryScreen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    constexpr int totalIterations = zzWarmupIterations
        + zzMeasuredIterations;
    for (int iteration = 0; iteration < totalIterations; ++iteration) {
        QProcess child;
        child.setProcessChannelMode(QProcess::SeparateChannels);
        QElapsedTimer external;
        external.start();
        child.start(probePath, {});
        if (!child.waitForStarted(1000) || !child.waitForFinished(5000)
            || child.exitStatus() != QProcess::NormalExit
            || child.exitCode() != 0) {
            return zzFail(QStringLiteral(
                "startup probe failed: %1; stdout=%2; stderr=%3")
                              .arg(
                                  child.errorString(),
                                  QString::fromUtf8(
                                      child.readAllStandardOutput()),
                                  QString::fromUtf8(
                                      child.readAllStandardError())));
        }
        const qint64 externalNanoseconds = external.nsecsElapsed();

        QJsonObject markers;
        QString parseFailure;
        if (!zzParseMarkers(
                child.readAllStandardOutput(), &markers, &parseFailure)) {
            return zzFail(parseFailure);
        }
        if (iteration >= zzWarmupIterations) {
            constexpr double nanosecondsPerMillisecond = 1'000'000.0;
            reporter.addSample({
                QStringLiteral("external-total"),
                QStringLiteral("ms"),
                static_cast<double>(externalNanoseconds)
                    / nanosecondsPerMillisecond});
            zzAddMarkerSamples(&reporter, markers);
        }
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
