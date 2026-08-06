#include <array>
#include <cstdlib>

#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTextStream>
#include <QtGui/QGuiApplication>

#include "ZzBenchmarkMetadata.h"
#include "ZzExampleExecutablePath.h"
#include "ZzPerformanceReporter.h"

namespace {

constexpr int zzWarmupIterations = 5;
constexpr int zzMeasuredIterations = 30;
constexpr double zzNanosecondsPerMillisecond = 1'000'000.0;

/** @brief 返回命令行中 --report 后的非空输出路径。 */
[[nodiscard]] QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出综合示例启动基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 严格解析完整综合示例唯一一行启动标记。 */
[[nodiscard]] bool zzParseMarkers(
    const QByteArray &output,
    QJsonObject *markers,
    QString *failure)
{
    const QList<QByteArray> lines = output.trimmed().split('\n');
    QList<QByteArray> markerLines;
    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
            markerLines.append(trimmed);
        }
    }
    if (markerLines.size() != 1) {
        *failure = QStringLiteral(
            "integrated startup probe must output exactly one JSON marker line");
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        markerLines.constFirst(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        *failure = QStringLiteral("invalid integrated startup JSON: %1")
                       .arg(error.errorString());
        return false;
    }

    static constexpr std::array markerNames = {
        "process-entry", "first-paint"};
    const QJsonObject parsed = document.object();
    if (parsed.size() != static_cast<qsizetype>(markerNames.size())
        || parsed.value(QStringLiteral("process-entry")).toInteger(-1) != 0
        || !parsed.value(QStringLiteral("first-paint")).isDouble()
        || parsed.value(QStringLiteral("first-paint")).toInteger(0) <= 0) {
        *failure = QStringLiteral(
            "integrated startup marker schema is invalid");
        return false;
    }
    *markers = parsed;
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }
    const QString examplePath = QString::fromUtf8(ZzExampleExecutablePath);
    if (!QFileInfo(examplePath).isAbsolute()
        || !QFileInfo(examplePath).isExecutable()) {
        return zzFail(QStringLiteral(
            "invalid integrated example executable: %1").arg(examplePath));
    }

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("example-startup"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, QGuiApplication::primaryScreen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("ZZ_PURETOOLS_EXAMPLE_PERFORMANCE_SCENARIO"),
        QStringLiteral("startup-probe"));
    environment.remove(
        QStringLiteral("ZZ_PURETOOLS_EXAMPLE_PERFORMANCE_REPORT"));
    environment.remove(QStringLiteral("ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO"));
    environment.remove(QStringLiteral("ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS"));

    constexpr int totalIterations = zzWarmupIterations
        + zzMeasuredIterations;
    for (int iteration = 0; iteration < totalIterations; ++iteration) {
        QProcess child;
        child.setProcessEnvironment(environment);
        child.setProcessChannelMode(QProcess::SeparateChannels);
        QElapsedTimer externalTimer;
        externalTimer.start();
        child.start(examplePath, {});
        if (!child.waitForStarted(2000) || !child.waitForFinished(10000)
            || child.exitStatus() != QProcess::NormalExit
            || child.exitCode() != 0) {
            return zzFail(QStringLiteral(
                "integrated startup child failed: %1; stdout=%2; stderr=%3")
                              .arg(
                                  child.errorString(),
                                  QString::fromUtf8(
                                      child.readAllStandardOutput()),
                                  QString::fromUtf8(
                                      child.readAllStandardError())));
        }
        const qint64 externalNanoseconds = externalTimer.nsecsElapsed();
        QJsonObject markers;
        QString failure;
        if (!zzParseMarkers(
                child.readAllStandardOutput(), &markers, &failure)) {
            return zzFail(failure);
        }
        if (iteration >= zzWarmupIterations) {
            reporter.addSample({
                QStringLiteral("external-total"),
                QStringLiteral("ms"),
                static_cast<double>(externalNanoseconds)
                    / zzNanosecondsPerMillisecond});
            reporter.addSample({
                QStringLiteral("first-paint"),
                QStringLiteral("ms"),
                static_cast<double>(markers.value(
                    QStringLiteral("first-paint")).toInteger())
                    / zzNanosecondsPerMillisecond});
        }
    }

    const auto result = reporter.write(reportPath);
    return result ? EXIT_SUCCESS
                  : zzFail(result.error().technicalMessage());
}
