#include <QtTest/QTest>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <ZzCore/ZzApplicationPaths.h>

class ZzApplicationPathsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void cleanupTestCase()
    {
        QStandardPaths::setTestModeEnabled(false);
    }

    void resolvesAndCreatesDirectories()
    {
        const ZzCore::ZzApplicationPaths paths(
            QStringLiteral("ZzTests"),
            QStringLiteral("CorePaths"));

        QVERIFY(!paths.configDirectory().isEmpty());
        QVERIFY(!paths.dataDirectory().isEmpty());
        QVERIFY(!paths.cacheDirectory().isEmpty());
        QCOMPARE(
            paths.logDirectory(),
            QDir(paths.dataDirectory()).filePath(QStringLiteral("logs")));
        QVERIFY(paths.configDirectory().endsWith(
            QDir::cleanPath(QStringLiteral("ZzTests/CorePaths"))));
        QVERIFY(paths.dataDirectory().endsWith(
            QDir::cleanPath(QStringLiteral("ZzTests/CorePaths"))));
        QVERIFY(paths.cacheDirectory().endsWith(
            QDir::cleanPath(QStringLiteral("ZzTests/CorePaths"))));
        QVERIFY(paths.ensureDirectories());
        QVERIFY(QDir(paths.logDirectory()).exists());
    }
};

QTEST_GUILESS_MAIN(ZzApplicationPathsTest)

#include "ZzApplicationPathsTest.moc"
