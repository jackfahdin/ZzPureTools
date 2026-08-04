#include <QtTest/QTest>

#include <memory>
#include <utility>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzResult.h>

class ZzResultTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storesValue()
    {
        auto result = ZzCore::ZzResult<int>::success(42);
        QVERIFY(result.hasValue());
        QCOMPARE(result.value(), 42);
    }

    void storesError()
    {
        const ZzCore::ZzError error(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("negative size"),
            QStringLiteral("size=-1"));
        auto result = ZzCore::ZzResult<int>::failure(error);

        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(
            result.error().technicalMessage(), QStringLiteral("negative size"));
        QCOMPARE(result.error().context(), QStringLiteral("size=-1"));
    }

    void supportsVoid()
    {
        QVERIFY(ZzCore::ZzResult<void>::success());
        auto failure = ZzCore::ZzResult<void>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Cancelled,
                QStringLiteral("cancelled")));
        QVERIFY(!failure);
        QCOMPARE(failure.error().code(), ZzCore::ZzErrorCode::Cancelled);
    }

    void extractsMoveOnlyValue()
    {
        auto result = ZzCore::ZzResult<std::unique_ptr<int>>::success(
            std::make_unique<int>(7));

        auto value = std::move(result).value();

        QVERIFY(value != nullptr);
        QCOMPARE(*value, 7);
    }
};

QTEST_GUILESS_MAIN(ZzResultTest)

#include "ZzResultTest.moc"
