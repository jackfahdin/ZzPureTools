#ifndef ZZDESIGNTOKENS_H
#define ZZDESIGNTOKENS_H

#include <QColor>

enum class ZzThemeMode {
    Light,
    Dark,
    System,
};

enum class ZzControlState {
    Normal,
    Hovered,
    Pressed,
    Disabled,
};

struct ZzColorTokens {
    QColor windowBase;
    QColor controlFill;
    QColor controlFillHover;
    QColor controlFillPressed;
    QColor controlFillDisabled;
    QColor controlStroke;
    QColor controlStrokeHover;
    QColor textPrimary;
    QColor textSecondary;
    QColor textDisabled;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor popupBackground;
    QColor popupBorder;
    QColor popupItemHover;
    QColor popupItemSelected;
};

[[nodiscard]] constexpr ZzColorTokens zzLightColorTokens() noexcept
{
    return ZzColorTokens{
        .windowBase = QColor(243, 243, 243),
        .controlFill = QColor(251, 251, 251),
        .controlFillHover = QColor(246, 246, 246),
        .controlFillPressed = QColor(237, 237, 237),
        .controlFillDisabled = QColor(245, 245, 245),
        .controlStroke = QColor(209, 209, 209),
        .controlStrokeHover = QColor(188, 188, 188),
        .textPrimary = QColor(32, 32, 32),
        .textSecondary = QColor(96, 96, 96),
        .textDisabled = QColor(161, 161, 161),
        .accent = QColor(0, 120, 212),
        .accentHover = QColor(26, 140, 228),
        .accentPressed = QColor(0, 90, 158),
        .popupBackground = QColor(252, 252, 252),
        .popupBorder = QColor(210, 210, 210),
        .popupItemHover = QColor(243, 243, 243),
        .popupItemSelected = QColor(234, 244, 255),
    };
}

[[nodiscard]] constexpr ZzColorTokens zzDarkColorTokens() noexcept
{
    return ZzColorTokens{
        .windowBase = QColor(32, 32, 32),
        .controlFill = QColor(59, 59, 59),
        .controlFillHover = QColor(68, 68, 68),
        .controlFillPressed = QColor(76, 76, 76),
        .controlFillDisabled = QColor(48, 48, 48),
        .controlStroke = QColor(76, 76, 76),
        .controlStrokeHover = QColor(96, 96, 96),
        .textPrimary = QColor(255, 255, 255),
        .textSecondary = QColor(200, 200, 200),
        .textDisabled = QColor(110, 110, 110),
        .accent = QColor(96, 205, 255),
        .accentHover = QColor(126, 218, 255),
        .accentPressed = QColor(72, 178, 228),
        .popupBackground = QColor(43, 43, 43),
        .popupBorder = QColor(68, 68, 68),
        .popupItemHover = QColor(58, 58, 58),
        .popupItemSelected = QColor(40, 62, 86),
    };
}

struct ZzMetricTokens {
    int controlRadius = 4;
    int controlHeight = 32;
    int borderWidth = 1;
    int contentPadding = 12;
    int shadowBlur = 6;
    int shadowOffsetY = 2;
};

[[nodiscard]] constexpr ZzMetricTokens zzDefaultMetricTokens() noexcept
{
    return ZzMetricTokens{};
}

#endif // ZZDESIGNTOKENS_H
