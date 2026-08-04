#include <ZzFluentUI/ZzThemePalette.h>

#include <utility>

#include <QtCore/QtGlobal>

namespace ZzFluentUI {

namespace {

using ZzColors = std::array<
    QColor,
    static_cast<std::size_t>(ZzColorToken::Count)>;

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
        QColor("#ffffff"),
        QColor("#000000"),
        QColor("#f9f9f9"),
        QColor("#ffffff"),
        QColor("#c42b1c")};
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
        QColor("#000000"),
        QColor("#ffffff"),
        QColor("#202020"),
        QColor("#2b2b2b"),
        QColor("#ff99a4")};
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
        QColor(Qt::red)};
}

} // namespace

ZzThemePalette::ZzThemePalette(ZzColors colors)
    : colors_(std::move(colors))
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
