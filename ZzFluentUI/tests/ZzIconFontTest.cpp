#include <QtCore/QFile>
#include <QtGui/QRawFont>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconFont.h>

class ZzIconFontTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesBundledResources()
    {
        QVERIFY(QFile::exists(
            QStringLiteral(":/zzfluent/fonts/ZzAwesome.ttf")));
        QVERIFY(QFile::exists(
            QStringLiteral(":/zzfluent/icons/Close.svg")));
        QVERIFY(QFile::exists(
            QStringLiteral(":/zzfluent/icons/Sun.svg")));
    }

    void registersFontOnlyOnce()
    {
        QVERIFY(ZzFluentUI::ZzIconFont::ensureRegistered());
        QVERIFY(ZzFluentUI::ZzIconFont::ensureRegistered());
        QCOMPARE(
            ZzFluentUI::ZzIconFont::familyName(),
            QStringLiteral("ZzAwesome"));
    }

    void resolvesFirstAndLastNamedGlyphs()
    {
        const QFont font = ZzFluentUI::ZzIconFont::font(24);
        QCOMPARE(font.family(), QStringLiteral("ZzAwesome"));
        QCOMPARE(font.pixelSize(), 24);

        const QRawFont rawFont = QRawFont::fromFont(font);
        QVERIFY(rawFont.isValid());
        const auto firstGlyphs = rawFont.glyphIndexesForString(
            ZzFluentUI::zzFontIconText(
                ZzFluentUI::ZzFontIcon::Broom));
        const auto lastGlyphs = rawFont.glyphIndexesForString(
            ZzFluentUI::zzFontIconText(
                ZzFluentUI::ZzFontIcon::XmarkLarge));
        QCOMPARE(firstGlyphs.size(), 1);
        QCOMPARE(lastGlyphs.size(), 1);
        QVERIFY(firstGlyphs.constFirst() != 0U);
        QVERIFY(lastGlyphs.constFirst() != 0U);
        QVERIFY(ZzFluentUI::zzFontIconText(
                    ZzFluentUI::ZzFontIcon::None)
                    .isEmpty());
    }
};

QTEST_MAIN(ZzIconFontTest)

#include "ZzIconFontTest.moc"
