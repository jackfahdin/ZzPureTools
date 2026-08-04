#include <QtTest/QTest>

#include <optional>
#include <thread>

#include <QtCore/QTemporaryDir>

#include <ZzCore/ZzQtSettingsStore.h>

class ZzQtSettingsStoreTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsWritesRemovesAndSyncs()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());

        ZzCore::ZzQtSettingsStore store(
            temporary.filePath(QStringLiteral("settings.ini")));
        QVERIFY(store.write(
            QStringLiteral("theme/mode"), QStringLiteral("dark")));
        QVERIFY(store.sync());

        auto readResult = store.read(
            QStringLiteral("theme/mode"),
            QStringLiteral("light"));
        QVERIFY(readResult);
        QCOMPARE(
            std::move(readResult).value(),
            QVariant(QStringLiteral("dark")));

        QVERIFY(store.remove(QStringLiteral("theme/mode")));
        auto defaultResult = store.read(
            QStringLiteral("theme/mode"),
            QStringLiteral("light"));
        QVERIFY(defaultResult);
        QCOMPARE(
            std::move(defaultResult).value(),
            QVariant(QStringLiteral("light")));
        QVERIFY(!store.write(QString(), 1));
    }

    void readRejectsWrongThread()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        ZzCore::ZzQtSettingsStore store(
            temporary.filePath(QStringLiteral("settings.ini")));
        std::optional<ZzCore::ZzResult<QVariant>> workerResult;

        std::jthread worker([&store, &workerResult] {
            workerResult.emplace(store.read(QStringLiteral("theme/mode")));
        });
        worker.join();

        QVERIFY(workerResult.has_value());
        if (!workerResult.has_value()) {
            return;
        }
        QVERIFY(!*workerResult);
        QCOMPARE(
            workerResult->error().code(),
            ZzCore::ZzErrorCode::InvalidState);
    }
};

QTEST_GUILESS_MAIN(ZzQtSettingsStoreTest)

#include "ZzQtSettingsStoreTest.moc"
