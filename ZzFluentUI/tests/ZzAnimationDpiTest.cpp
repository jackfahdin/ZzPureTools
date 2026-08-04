#include <limits>

#include <QtCore/QSet>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzIconCacheKey.h>

/**
 * @brief 验证动效、DPR 和图标缓存键的跨设备确定性契约。
 */
class ZzAnimationDpiTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesReducedMotion()
    {
        QCOMPARE(
            ZzFluentUI::ZzAnimationPolicy::adjustedDuration(
                167, false, false),
            167);
        QCOMPARE(
            ZzFluentUI::ZzAnimationPolicy::adjustedDuration(
                167, true, false),
            0);
        QCOMPARE(
            ZzFluentUI::ZzAnimationPolicy::adjustedDuration(
                167, true, true),
            50);
        QCOMPARE(
            ZzFluentUI::ZzAnimationPolicy::adjustedDuration(
                -1, false, true),
            0);
    }

    void quantizesDprWithoutZeroSizes()
    {
        QCOMPARE(ZzFluentUI::ZzDpiScale::bucket(1.25), quint16{125});
        QCOMPARE(ZzFluentUI::ZzDpiScale::physicalPixels(1.0, 1.25), 2);
        QCOMPARE(ZzFluentUI::ZzDpiScale::bucket(0.0), quint16{100});
        QCOMPARE(
            ZzFluentUI::ZzDpiScale::bucket(
                std::numeric_limits<qreal>::infinity()),
            quint16{100});
        QCOMPARE(
            ZzFluentUI::ZzDpiScale::physicalPixels(
                std::numeric_limits<qreal>::max(), 8.0),
            std::numeric_limits<int>::max());
    }

    void distinguishesEveryIconInput()
    {
        const ZzFluentUI::ZzIconCacheKey first(
            QStringLiteral(":/icons/first.svg"),
            false,
            QSize(16, 16),
            125,
            0xff0067c0U,
            4);
        const ZzFluentUI::ZzIconCacheKey differentDpr(
            QStringLiteral(":/icons/first.svg"),
            false,
            QSize(16, 16),
            150,
            0xff0067c0U,
            4);
        const ZzFluentUI::ZzIconCacheKey differentResource(
            QStringLiteral(":/icons/second.svg"),
            false,
            QSize(16, 16),
            125,
            0xff0067c0U,
            4);
        const ZzFluentUI::ZzIconCacheKey mirrored(
            QStringLiteral(":/icons/first.svg"),
            true,
            QSize(16, 16),
            125,
            0xff0067c0U,
            4);
        QVERIFY(first != differentDpr);
        QVERIFY(first != differentResource);
        QVERIFY(first != mirrored);
        QSet<ZzFluentUI::ZzIconCacheKey> keys;
        keys.insert(first);
        keys.insert(differentDpr);
        keys.insert(differentResource);
        keys.insert(mirrored);
        QCOMPARE(keys.size(), 4);
    }
};

QTEST_GUILESS_MAIN(ZzAnimationDpiTest)

#include "ZzAnimationDpiTest.moc"
