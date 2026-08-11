#include <QtGui/QColor>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

/**
 * @brief 验证主题快照提供确定性且可读的完整令牌集合。
 */
class ZzThemeSnapshotTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesDeterministicTokens()
    {
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::Light,
            QColor(QStringLiteral("#0067c0")),
            7,
            false);

        QCOMPARE(snapshot.revision(), quint64{7});
        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::Accent),
            QColor(QStringLiteral("#0067c0")));
        QVERIFY(snapshot.metric(
                    ZzFluentUI::ZzMetricToken::CornerRadiusMedium)
                > 0.0);
        QVERIFY(snapshot.metric(
                    ZzFluentUI::ZzMetricToken::DialogMinWidth)
                < snapshot.metric(
                    ZzFluentUI::ZzMetricToken::DialogMaxWidth));
        QCOMPARE(
            snapshot.metric(
                ZzFluentUI::ZzMetricToken::SelectionIndicatorThickness),
            3.0);
        QCOMPARE(
            snapshot.metric(
                ZzFluentUI::ZzMetricToken::SelectionIndicatorExtent),
            16.0);
        QVERIFY(snapshot.color(ZzFluentUI::ZzColorToken::OverlayScrim).alpha()
                > 0);
        QVERIFY(snapshot.color(ZzFluentUI::ZzColorToken::Information).isValid());
        QVERIFY(snapshot.color(ZzFluentUI::ZzColorToken::Success).isValid());
        QVERIFY(snapshot.color(ZzFluentUI::ZzColorToken::Warning).isValid());
        QVERIFY(snapshot.duration(ZzFluentUI::ZzMotionToken::Fast) > 0);
        QVERIFY(!snapshot.font(
                    ZzFluentUI::ZzTypographyToken::Body)
                     .family()
                     .isEmpty());
    }

    void keepsHighContrastLegible()
    {
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::HighContrast,
            QColor(Qt::black),
            1,
            true);

        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::Surface),
            QColor(Qt::black));
        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::TextPrimary),
            QColor(Qt::white));
        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::Accent),
            QColor(Qt::yellow));
        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::AccentText),
            QColor(Qt::black));
        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::FocusStroke),
            QColor(Qt::yellow));
        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::Information),
            QColor(Qt::cyan));
        QCOMPARE(snapshot.duration(ZzFluentUI::ZzMotionToken::Fast), 0);
        QVERIFY(snapshot.reducedMotion());
    }

    void selectsLegibleTextForArbitraryAccentColors()
    {
        const auto darkAccent = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::Dark,
            QColor(QStringLiteral("#0067c0")),
            1,
            false);
        const auto brightAccent = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::Light,
            QColor(QStringLiteral("#ffff00")),
            2,
            false);

        QCOMPARE(
            darkAccent.color(ZzFluentUI::ZzColorToken::AccentText),
            QColor(Qt::white));
        QCOMPARE(
            brightAccent.color(ZzFluentUI::ZzColorToken::AccentText),
            QColor(Qt::black));
    }
};

QTEST_MAIN(ZzThemeSnapshotTest)

#include "ZzThemeSnapshotTest.moc"
