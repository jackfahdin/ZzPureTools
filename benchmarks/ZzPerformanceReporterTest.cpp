#include <QtTest/QTest>

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QTemporaryDir>

#include "ZzPerformanceReporter.h"

class ZzPerformanceReporterTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void calculatesNearestRankAndWritesSchema()
    {
        ZzBenchmarks::ZzPerformanceReporter reporter;
        reporter.setScenario(QStringLiteral("contract"));
        reporter.setWarmupIterations(10);
        for (const double value : {1.0, 2.0, 3.0, 4.0, 100.0}) {
            reporter.addSample({QStringLiteral("latency"),
                                QStringLiteral("ms"), value});
        }
        reporter.addEnvironmentMetadata(
            QStringLiteral("cpu"), QStringLiteral("test-cpu"));
        reporter.addEnvironmentMetadata(QStringLiteral("memoryBytes"), 1024);
        reporter.addEnvironmentMetadata(
            QStringLiteral("os"), QStringLiteral("test-os"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("gpu"), QStringLiteral("test-gpu/test-driver"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("windowSystem"), QStringLiteral("test-qpa"));
        reporter.addEnvironmentMetadata(QStringLiteral("dpr"), 1.0);
        reporter.addEnvironmentMetadata(QStringLiteral("refreshRateHz"), 60.0);
        reporter.addEnvironmentMetadata(
            QStringLiteral("qtVersion"), QStringLiteral("6.8.3"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("compiler"), QStringLiteral("GNU 13.2.0"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("runnerImageDigest"),
            QStringLiteral("sha256:0000000000000000000000000000000000000000000000000000000000000000"));
        reporter.addBuildMetadata(
            QStringLiteral("commit"),
            QStringLiteral("0123456789abcdef0123456789abcdef01234567"));
        const QString preset = qEnvironmentVariable("ZZ_CMAKE_PRESET");
        QVERIFY(!preset.isEmpty());
        reporter.addBuildMetadata(QStringLiteral("preset"), preset);
        reporter.addBuildMetadata(
            QStringLiteral("buildType"), QStringLiteral("Release"));
        reporter.addBuildMetadata(QStringLiteral("shared"), true);
        reporter.addBuildMetadata(QStringLiteral("lto"), true);
        reporter.addBuildMetadata(
            QStringLiteral("sanitizers"), QStringLiteral("none"));

        const auto reportResult = reporter.report();
        QVERIFY(reportResult);
        const QJsonObject &root = reportResult.value();
        const QJsonObject latency = root.value(QStringLiteral("metrics"))
            .toObject().value(QStringLiteral("latency")).toObject();
        QCOMPARE(latency.value(QStringLiteral("count")).toInt(), 5);
        QCOMPARE(latency.value(QStringLiteral("p50")), QJsonValue(3.0));
        QCOMPARE(latency.value(QStringLiteral("p95")), QJsonValue(100.0));
        QCOMPARE(latency.value(QStringLiteral("max")), QJsonValue(100.0));
        QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QVERIFY(root.value(QStringLiteral("environment")).isObject());
        QVERIFY(root.value(QStringLiteral("build")).isObject());

        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString outputPath =
            temporaryDirectory.filePath(QStringLiteral("report.json"));
        QVERIFY(reporter.write(outputPath));
        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(output.readAll()).object(), root);

        reporter.addBuildMetadata(QStringLiteral("unexpected"), true);
        const auto extendedResult = reporter.report();
        QVERIFY(!extendedResult);
        QCOMPARE(
            extendedResult.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
    }

    void rejectsConflictingMetricUnits()
    {
        ZzBenchmarks::ZzPerformanceReporter reporter;
        reporter.setScenario(QStringLiteral("contract"));
        reporter.addSample({QStringLiteral("latency"),
                            QStringLiteral("ms"), 1.0});
        reporter.addSample({QStringLiteral("latency"),
                            QStringLiteral("bytes"), 2.0});

        const auto result = reporter.report();
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(result.error().technicalMessage().contains(
            QStringLiteral("unit"), Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(ZzPerformanceReporterTest)

#include "ZzPerformanceReporterTest.moc"
