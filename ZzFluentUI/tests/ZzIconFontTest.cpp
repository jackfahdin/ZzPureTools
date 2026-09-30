#include <QtCore/QFile>
#include <QtGui/QRawFont>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtGui/QPainter>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconFont.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>

class ZzIconFontTest final : public QObject
{
    Q_OBJECT

private slots:
    /** @brief 非方形目标区域中绘制也必须保留字体图标比例。 */
    void galleryIconsPreserveAspectRatioWhenPainted()
    {
        using namespace ZzFluentUI;
        const auto icon = ZzSegoeIconFont::galleryIcon(ZzSegoeIcon::Home);
        for (qreal dpr : {1.0, 1.25, 1.5, 2.0}) {
            QImage actual(QSize(32, 16) * dpr, QImage::Format_ARGB32_Premultiplied);
            actual.setDevicePixelRatio(dpr);
            actual.fill(Qt::transparent);
            QPainter painter(&actual);
            icon.paint(&painter, QRect(0, 0, 32, 16));
            painter.end();
            const auto expected = icon.pixmap(QSize(16, 16), dpr).toImage();
            QCOMPARE(actual.copy(qRound(8 * dpr), 0, qRound(16 * dpr), qRound(16 * dpr)), expected);
        }
    }

    /** @brief Gallery 小图标保留原版留白，选中状态不重着色，缩放后仍有有效字形。 */
    void galleryIconsKeepCanvasPaddingAndThemeColor()
    {
        using namespace ZzFluentUI;
        const auto gallery = ZzSegoeIconFont::galleryIcon(ZzSegoeIcon::Home);
        const auto pivot = ZzSegoeIconFont::galleryIcon(ZzSegoeIcon::Home, 22);
        QVERIFY(!ZzSegoeIconFont::usesForegroundColor(gallery));
        const auto bounds = [](const QImage &image) {
            QRect result;
            for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x)
                if (image.pixelColor(x, y).alpha() > 60) result = result.united(QRect(x, y, 1, 1));
            return result;
        };
        for (qreal dpr : {1.0, 1.25, 1.5, 2.0}) {
            const auto small = gallery.pixmap(QSize(16, 16), dpr);
            QCOMPARE(small.size(), QSize(qRound(16 * dpr), qRound(16 * dpr)));
            QCOMPARE(small.devicePixelRatio(), dpr);
            const auto padded = bounds(small.toImage());
            const auto full = bounds(pivot.pixmap(QSize(16, 16), dpr).toImage());
            QVERIFY(!padded.isEmpty());
            QVERIFY(padded.width() < full.width());
            QCOMPARE(gallery.pixmap(QSize(16, 16), dpr, QIcon::Normal, QIcon::Off).toImage(),
                gallery.pixmap(QSize(16, 16), dpr, QIcon::Normal, QIcon::On).toImage());
        }
        const auto contrast = ZzSegoeIconFont::withForegroundColor(gallery, Qt::red).pixmap(QSize(16, 16)).toImage();
        int redPixels = 0;
        for (int y = 0; y < contrast.height(); ++y) for (int x = 0; x < contrast.width(); ++x) {
            const auto pixel = contrast.pixelColor(x, y);
            if (pixel.alpha() > 30) {
                QVERIFY2(pixel.red() > 250 && pixel.green() == 0 && pixel.blue() == 0,
                    qPrintable(pixel.name(QColor::HexArgb)));
                ++redPixels;
            }
        }
        QVERIFY(redPixels > 5);
    }

    void resolvesSegoeGlyphs()
    {
        using namespace ZzFluentUI;
        QVERIFY(ZzSegoeIconFont::ensureRegistered());
        const auto font = ZzSegoeIconFont::font(22);
        QCOMPARE(font.family(), QStringLiteral("Segoe Fluent Icons"));
        QCOMPARE(font.pixelSize(), 22);
        const auto raw = QRawFont::fromFont(font);
        QVERIFY(raw.isValid());
        for (auto glyph : {ZzSegoeIcon::Home, ZzSegoeIcon::Search, ZzSegoeIcon::Settings,
                 ZzSegoeIcon::Help, ZzSegoeIcon::Info, ZzSegoeIcon::Folder, ZzSegoeIcon::History,
                 ZzSegoeIcon::CompanionApp, ZzSegoeIcon::PlayerSettings, ZzSegoeIcon::Robot,
                 ZzSegoeIcon::RingerSilent, ZzSegoeIcon::TrafficCongestionSolid,
                 ZzSegoeIcon::Camera, ZzSegoeIcon::Video, ZzSegoeIcon::MusicInfo, ZzSegoeIcon::Cloud,
                 ZzSegoeIcon::Unknown, ZzSegoeIcon::Close}) {
            QVERIFY(raw.supportsCharacter(static_cast<uint>(glyph)));
        }
    }

    void rendersSegoeAtRequestedDpr()
    {
        using namespace ZzFluentUI;
        const auto icon = ZzSegoeIconFont::icon(ZzSegoeIcon::Home, QColor("#da2653"));
        QVERIFY(!icon.isNull());
        QVERIFY(!ZzSegoeIconFont::usesForegroundColor(icon));
        for (qreal dpr : {1.0, 1.25, 1.5, 2.0}) {
            const auto pixmap = icon.pixmap(QSize(24, 24), dpr);
            QCOMPARE(pixmap.size(), QSize(qRound(24 * dpr), qRound(24 * dpr)));
            QCOMPARE(pixmap.devicePixelRatio(), dpr);
            const auto image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
            int ink = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    if (image.pixelColor(x, y).alpha() > 240) {
                        ++ink;
                        const auto color = image.pixelColor(x, y);
                        QVERIFY(qAbs(color.red() - 218) <= 1);
                        QVERIFY(qAbs(color.green() - 38) <= 1);
                        QVERIFY(qAbs(color.blue() - 83) <= 1);
                    }
                }
            }
            QVERIFY(ink > 10);
        }
    }

    void updatesSegoePaletteWithoutRecreatingIcon()
    {
        using namespace ZzFluentUI;
        const auto previous = QGuiApplication::palette();
        const auto automatic = ZzSegoeIconFont::icon(ZzSegoeIcon::Settings);
        const auto fixed = ZzSegoeIconFont::icon(ZzSegoeIcon::Settings, Qt::green);
        QVERIFY(ZzSegoeIconFont::usesForegroundColor(automatic));
        auto palette = previous;
        palette.setColor(QPalette::Active, QPalette::WindowText, Qt::red);
        palette.setColor(QPalette::Disabled, QPalette::WindowText, Qt::blue);
        QGuiApplication::setPalette(palette);
        const auto red = automatic.pixmap(QSize(24, 24)).toImage();
        const auto disabled = automatic.pixmap(QSize(24, 24), QIcon::Disabled).toImage();
        const auto fixedBefore = fixed.pixmap(QSize(24, 24)).toImage();
        palette.setColor(QPalette::Active, QPalette::WindowText, Qt::blue);
        QGuiApplication::setPalette(palette);
        const auto blue = automatic.pixmap(QSize(24, 24)).toImage();
        const auto fixedAfter = fixed.pixmap(QSize(24, 24)).toImage();
        QGuiApplication::setPalette(previous);
        QVERIFY(red != blue);
        QCOMPARE(disabled, blue);
        QCOMPARE(fixedBefore, fixedAfter);
    }

    void appliesForegroundAlphaOnlyOnce()
    {
        using namespace ZzFluentUI;
        const auto glyph = ZzSegoeIcon::Home;
        const auto automatic = ZzSegoeIconFont::icon(glyph);
        const QColor foreground(14, 92, 160, 128);
        const auto expected = ZzSegoeIconFont::icon(glyph, foreground).pixmap(QSize(24, 24)).toImage();
        const auto actual = ZzSegoeIconFont::withForegroundColor(automatic, foreground).pixmap(QSize(24, 24)).toImage();
        QCOMPARE(actual, expected);
        int maxAlpha = 0;
        for (int y = 0; y < actual.height(); ++y)
            for (int x = 0; x < actual.width(); ++x)
                maxAlpha = qMax(maxAlpha, actual.pixelColor(x, y).alpha());
        QCOMPARE(maxAlpha, 128);
        const auto fixed = ZzSegoeIconFont::icon(glyph, Qt::red);
        QCOMPARE(ZzSegoeIconFont::withForegroundColor(fixed, Qt::blue).cacheKey(), fixed.cacheKey());
        QVERIFY(ZzSegoeIconFont::withForegroundColor(QIcon(), foreground).isNull());
    }

    void exposesBundledResources()
    {
        QVERIFY(QFile::exists(
            QStringLiteral(":/zzfluent/fonts/ZzAwesome.ttf")));
        QVERIFY(QFile::exists(
            QStringLiteral(":/zzfluent/fonts/SegoeFluentIcons.ttf")));
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
