#ifndef ZZPURETOOLS_CORE_ZZTOKEN_HPP
#define ZZPURETOOLS_CORE_ZZTOKEN_HPP

#include <QColor>

/**
 * @brief 主题模式枚举。
 */
enum class ZzThemeMode
{
    Light,   ///< 浅色模式
    Dark,    ///< 深色模式
    System,  ///< 跟随系统
};

/**
 * @brief 控件状态枚举。
 */
enum class ZzControlState
{
    Normal,    ///< 正常状态
    Hovered,   ///< 悬停状态
    Pressed,   ///< 按下状态
    Disabled,  ///< 禁用状态
};

/**
 * @brief 颜色设计令牌。
 *
 * 集中定义 Fluent UI 风格所需的全局颜色变量。
 */
struct ZzColorTokens
{
    QColor windowBase;           ///< 窗口背景色
    QColor controlFill;          ///< 控件填充色
    QColor controlFillHover;     ///< 控件悬停填充色
    QColor controlFillPressed;   ///< 控件按下填充色
    QColor controlFillDisabled;  ///< 控件禁用填充色
    QColor controlStroke;        ///< 控件边框色
    QColor controlStrokeHover;   ///< 控件悬停边框色
    QColor textPrimary;          ///< 主文本色
    QColor textSecondary;        ///< 次要文本色
    QColor textDisabled;         ///< 禁用文本色
    QColor accent;               ///< 强调色
    QColor accentHover;          ///< 强调色悬停
    QColor accentPressed;        ///< 强调色按下
    QColor popupBackground;      ///< 弹出层背景色
    QColor popupBorder;          ///< 弹出层边框色
    QColor popupItemHover;       ///< 弹出项悬停色
    QColor popupItemSelected;    ///< 弹出项选中色
};

/**
 * @brief 尺寸设计令牌。
 *
 * 集中定义控件圆角、高度、间距、动画时长等尺寸参数。
 */
struct ZzMetricTokens
{
    int controlRadius = 4;          ///< 控件圆角半径（像素）
    int controlHeight = 32;         ///< 标准控件高度（像素）
    int borderWidth = 1;            ///< 边框宽度（像素）
    int contentPadding = 12;        ///< 内容内边距（像素）
    int animationDurationMs = 120;  ///< 动画时长（毫秒）
};

/**
 * @brief 获取浅色主题颜色令牌。
 * @return 浅色模式颜色值。
 */
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

/**
 * @brief 获取深色主题颜色令牌。
 * @return 深色模式颜色值。
 */
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

/**
 * @brief 获取默认尺寸令牌。
 * @return 使用默认值的尺寸令牌。
 */
[[nodiscard]] constexpr ZzMetricTokens zzDefaultMetricTokens() noexcept
{
    return ZzMetricTokens{};
}

#endif  // ZZPURETOOLS_CORE_ZZTOKEN_HPP
