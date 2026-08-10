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

    void coversPublishedCodePointRange()
    {
        const QRawFont rawFont = QRawFont::fromFont(
            ZzFluentUI::ZzIconFont::font(24));
        QVERIFY(rawFont.isValid());

        constexpr quint32 firstCodePoint = 0xe800U;
        constexpr quint32 lastCodePoint = 0xf4cfU;
        QString characters;
        characters.reserve(
            static_cast<qsizetype>(lastCodePoint)
                - static_cast<qsizetype>(firstCodePoint)
                + qsizetype{1});
        for (quint32 codePoint = firstCodePoint;
             codePoint <= lastCodePoint;
             ++codePoint) {
            characters.append(QChar(static_cast<char16_t>(codePoint)));
        }

        const auto glyphs = rawFont.glyphIndexesForString(characters);
        QCOMPARE(glyphs.size(), characters.size());
        for (qsizetype index = 0; index < glyphs.size(); ++index) {
            QVERIFY2(
                glyphs.at(index) != 0U,
                qPrintable(QStringLiteral("缺少字体码点 U+%1")
                               .arg(
                                   firstCodePoint
                                       + static_cast<quint32>(index),
                                   4,
                                   16,
                                   QLatin1Char('0'))));
        }
    }
};

QTEST_MAIN(ZzIconFontTest)

#include "ZzIconFontTest.moc"
