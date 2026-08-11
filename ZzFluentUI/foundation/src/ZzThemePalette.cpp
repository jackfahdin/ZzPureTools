#include <ZzFluentUI/ZzThemePalette.h>

#include <cmath>
#include <QtCore/QtGlobal>

namespace ZzFluentUI {

namespace {

using ZzColors = std::array<
    QColor,
    static_cast<std::size_t>(ZzColorToken::Count)>;

/** @brief 按 WCAG 相对亮度为任意强调色选择高对比黑色或白色文字。 */
QColor zzAccentTextColor(const QColor &accent)
{
    const auto linearChannel = [](qreal channel) {
        return channel <= 0.04045
            ? channel / 12.92
            : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const qreal luminance = 0.2126 * linearChannel(accent.redF())
        + 0.7152 * linearChannel(accent.greenF())
        + 0.0722 * linearChannel(accent.blueF());
    return luminance > 0.179 ? QColor(Qt::black) : QColor(Qt::white);
}

ZzColors zzLightColors(const QColor &accent)
{
    return {
        QColor("#1a1a1a"),
        QColor("#5d5d5d"),
        QColor("#ffffff"),
        QColor("#f5f5f5"),
        QColor("#e8e8e8"),
        QColor("#f0f0f0"),
        QColor("#d1d1d1"),
        accent,
        zzAccentTextColor(accent),
        QColor("#000000"),
        QColor("#f9f9f9"),
        QColor("#ffffff"),
        QColor("#c42b1c"),
        QColor(0, 0, 0, 115),
        QColor("#005fb8"),
        QColor("#0f7b0f"),
        QColor("#9d5d00")};
}

ZzColors zzDarkColors(const QColor &accent)
{
    return {
        QColor("#ffffff"),
        QColor("#c5c5c5"),
        QColor("#323232"),
        QColor("#3b3b3b"),
        QColor("#454545"),
        QColor("#2a2a2a"),
        QColor("#5a5a5a"),
        accent,
        zzAccentTextColor(accent),
        QColor("#ffffff"),
        QColor("#202020"),
        QColor("#2b2b2b"),
        QColor("#ff99a4"),
        QColor(0, 0, 0, 140),
        QColor("#60cdff"),
        QColor("#6ccb5f"),
        QColor("#fce100")};
}

ZzColors zzHighContrastColors()
{
    return {
        QColor(Qt::white),
        QColor(Qt::white),
        QColor(Qt::black),
        QColor(Qt::black),
        QColor(Qt::black),
        QColor(Qt::black),
        QColor(Qt::white),
        QColor(Qt::yellow),
        QColor(Qt::black),
        QColor(Qt::yellow),
        QColor(Qt::black),
        QColor(Qt::black),
        QColor(Qt::red),
        QColor(0, 0, 0, 191),
        QColor(Qt::cyan),
        QColor(Qt::green),
        QColor(Qt::yellow)};
}

} // namespace

ZzThemePalette::ZzThemePalette(const ZzColors &colors)
    : colors_(colors)
{
}

ZzThemePalette ZzThemePalette::create(ZzThemeMode mode, QColor accent)
{
    if (!accent.isValid()) {
        accent = QColor("#0067c0");
    }
    if (mode == ZzThemeMode::HighContrast) {
        return ZzThemePalette(zzHighContrastColors());
    }
    if (mode == ZzThemeMode::Dark) {
        return ZzThemePalette(zzDarkColors(accent));
    }
    return ZzThemePalette(zzLightColors(accent));
}

QColor ZzThemePalette::color(ZzColorToken token) const noexcept
{
    const auto index = static_cast<std::size_t>(token);
    if (index >= colors_.size()) {
        Q_ASSERT(false);
        return QColor(Qt::transparent);
    }
    return colors_[index];
}

} // namespace ZzFluentUI
