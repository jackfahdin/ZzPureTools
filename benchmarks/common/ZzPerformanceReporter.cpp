#include "ZzPerformanceReporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <QtCore/QJsonDocument>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>

namespace ZzBenchmarks {

namespace {

struct ZzMetricValues final
{
    QString unit;
    QList<double> values;
};

ZzCore::ZzError invalidArgument(QString message)
{
    return ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidArgument,
        std::move(message));
}

bool isNonEmptyString(const QJsonValue &value)
{
    return value.isString() && !value.toString().trimmed().isEmpty();
}

bool isFinitePositiveNumber(const QJsonValue &value)
{
    return value.isDouble()
        && std::isfinite(value.toDouble())
        && value.toDouble() > 0.0;
}

ZzCore::ZzResult<void> validateEnvironment(const QJsonObject &environment)
{
    static constexpr std::array requiredStrings = {
        "cpu",
        "os",
        "gpu",
        "windowSystem",
        "qtVersion",
        "compiler"
    };
    constexpr qsizetype environmentFieldCount = 10;
    if (environment.size() != environmentFieldCount) {
        return ZzCore::ZzResult<void>::failure(invalidArgument(
            QStringLiteral("environment must contain exactly 10 fields")));
    }
    for (const char *key : requiredStrings) {
        if (!isNonEmptyString(environment.value(QLatin1StringView(key)))) {
            return ZzCore::ZzResult<void>::failure(invalidArgument(
                QStringLiteral("environment.%1 must be a non-empty string")
                    .arg(QLatin1StringView(key))));
        }
    }

    const QString gpu = environment.value(QStringLiteral("gpu"))
                            .toString().trimmed();
    if (gpu.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0) {
        return ZzCore::ZzResult<void>::failure(invalidArgument(
            QStringLiteral("environment.gpu must identify renderer and driver")));
    }

    const QJsonValue memory = environment.value(QStringLiteral("memoryBytes"));
    constexpr qint64 invalidMemory = -1;
    if (!memory.isDouble() || memory.toInteger(invalidMemory) < 0) {
        return ZzCore::ZzResult<void>::failure(invalidArgument(
            QStringLiteral("environment.memoryBytes must be a non-negative integer")));
    }

    for (const QString &key : {QStringLiteral("dpr"),
                               QStringLiteral("refreshRateHz")}) {
        if (!isFinitePositiveNumber(environment.value(key))) {
            return ZzCore::ZzResult<void>::failure(invalidArgument(
                QStringLiteral("environment.%1 must be finite and positive")
                    .arg(key)));
        }
    }

    static const QRegularExpression digestPattern(
        QStringLiteral("^sha256:[0-9a-f]{64}$"));
    const QJsonValue digest =
        environment.value(QStringLiteral("runnerImageDigest"));
    if (!digest.isString()
        || !digestPattern.match(digest.toString()).hasMatch()) {
        return ZzCore::ZzResult<void>::failure(invalidArgument(
            QStringLiteral("environment.runnerImageDigest has invalid format")));
    }

    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> validateBuild(const QJsonObject &build)
{
    constexpr qsizetype buildFieldCount = 6;
    if (build.size() != buildFieldCount) {
        return ZzCore::ZzResult<void>::failure(invalidArgument(
            QStringLiteral("build must contain exactly 6 fields")));
    }

    static const QRegularExpression commitPattern(
        QStringLiteral("^[0-9a-f]{40}$"));
    const QJsonValue commit = build.value(QStringLiteral("commit"));
    if (!commit.isString()
        || !commitPattern.match(commit.toString()).hasMatch()) {
        return ZzCore::ZzResult<void>::failure(invalidArgument(
            QStringLiteral("build.commit must be 40 lowercase hex characters")));
    }

    for (const QString &key : {QStringLiteral("preset"),
                               QStringLiteral("buildType"),
                               QStringLiteral("sanitizers")}) {
        if (!isNonEmptyString(build.value(key))) {
            return ZzCore::ZzResult<void>::failure(invalidArgument(
                QStringLiteral("build.%1 must be a non-empty string").arg(key)));
        }
    }

    const QString injectedPreset = qEnvironmentVariable("ZZ_CMAKE_PRESET");
    if (injectedPreset.isEmpty()
        || build.value(QStringLiteral("preset")).toString()
            != injectedPreset) {
        return ZzCore::ZzResult<void>::failure(invalidArgument(
            QStringLiteral("build.preset must match ZZ_CMAKE_PRESET")));
    }

    for (const QString &key : {QStringLiteral("shared"),
                               QStringLiteral("lto")}) {
        if (!build.value(key).isBool()) {
            return ZzCore::ZzResult<void>::failure(invalidArgument(
                QStringLiteral("build.%1 must be boolean").arg(key)));
        }
    }

    return ZzCore::ZzResult<void>::success();
}

double nearestRank(const QList<double> &sortedValues, double percentile)
{
    const auto count = static_cast<double>(sortedValues.size());
    const auto rank = static_cast<qsizetype>(
        std::ceil(percentile * count));
    return sortedValues.at(std::max<qsizetype>(rank, 1) - 1);
}

} // namespace

void ZzPerformanceReporter::setScenario(QString scenario)
{
    scenario_ = std::move(scenario);
}

void ZzPerformanceReporter::setWarmupIterations(qsizetype count)
{
    warmupIterations_ = count;
}

void ZzPerformanceReporter::addSample(ZzBenchmarkSample sample)
{
    samples_.append(std::move(sample));
}

void ZzPerformanceReporter::addEnvironmentMetadata(
    const QString &key,
    const QJsonValue &value)
{
    environment_.insert(key, value);
}

void ZzPerformanceReporter::addBuildMetadata(
    const QString &key,
    const QJsonValue &value)
{
    build_.insert(key, value);
}

ZzCore::ZzResult<QJsonObject> ZzPerformanceReporter::report() const
{
    if (scenario_.trimmed().isEmpty()) {
        return ZzCore::ZzResult<QJsonObject>::failure(invalidArgument(
            QStringLiteral("scenario must not be empty")));
    }
    if (warmupIterations_ < 0) {
        return ZzCore::ZzResult<QJsonObject>::failure(invalidArgument(
            QStringLiteral("warmupIterations must not be negative")));
    }
    if (samples_.isEmpty()) {
        return ZzCore::ZzResult<QJsonObject>::failure(invalidArgument(
            QStringLiteral("at least one sample is required")));
    }

    QMap<QString, ZzMetricValues> grouped;
    for (const ZzBenchmarkSample &sample : samples_) {
        const QString metric = sample.metric.trimmed();
        const QString unit = sample.unit.trimmed();
        if (metric.isEmpty() || unit.isEmpty() || !std::isfinite(sample.value)) {
            return ZzCore::ZzResult<QJsonObject>::failure(invalidArgument(
                QStringLiteral("sample metric/unit/value is invalid")));
        }

        auto metricIterator = grouped.find(metric);
        if (metricIterator == grouped.end()) {
            metricIterator = grouped.insert(metric, {unit, {}});
        } else if (metricIterator->unit != unit) {
            return ZzCore::ZzResult<QJsonObject>::failure(invalidArgument(
                QStringLiteral("metric unit conflict for %1").arg(metric)));
        }
        metricIterator->values.append(sample.value);
    }

    const auto environmentResult = validateEnvironment(environment_);
    if (!environmentResult) {
        return ZzCore::ZzResult<QJsonObject>::failure(
            environmentResult.error());
    }
    const auto buildResult = validateBuild(build_);
    if (!buildResult) {
        return ZzCore::ZzResult<QJsonObject>::failure(buildResult.error());
    }

    QJsonObject metrics;
    for (auto iterator = grouped.cbegin(); iterator != grouped.cend(); ++iterator) {
        QList<double> values = iterator->values;
        std::ranges::sort(values);

        QJsonObject metric;
        metric.insert(QStringLiteral("unit"), iterator->unit);
        metric.insert(
            QStringLiteral("count"),
            QJsonValue(static_cast<qint64>(values.size())));
        metric.insert(QStringLiteral("p50"), nearestRank(values, 0.50));
        metric.insert(QStringLiteral("p95"), nearestRank(values, 0.95));
        metric.insert(QStringLiteral("max"), values.constLast());
        metrics.insert(iterator.key(), metric);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("scenario"), scenario_.trimmed());
    root.insert(
        QStringLiteral("warmupIterations"),
        QJsonValue(static_cast<qint64>(warmupIterations_)));
    root.insert(QStringLiteral("metrics"), metrics);
    root.insert(QStringLiteral("environment"), environment_);
    root.insert(QStringLiteral("build"), build_);
    return ZzCore::ZzResult<QJsonObject>::success(std::move(root));
}

ZzCore::ZzResult<void> ZzPerformanceReporter::write(const QString &path) const
{
    const auto reportResult = report();
    if (!reportResult) {
        return ZzCore::ZzResult<void>::failure(reportResult.error());
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to open performance report"),
            file.errorString()));
    }

    const QByteArray data = QJsonDocument(reportResult.value())
                                .toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to write complete performance report"),
            file.errorString()));
    }
    if (!file.commit()) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to commit performance report"),
            file.errorString()));
    }

    return ZzCore::ZzResult<void>::success();
}

} // namespace ZzBenchmarks
