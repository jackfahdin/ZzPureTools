#pragma once

#include <QtGui/QPalette>

namespace ZzFluentUI {

/** @brief 无状态的强调色解析与状态颜色计算；不分配控件或逐帧建立主题快照。 */
class ZzControlAppearancePrivate final
{
public:
    /** @brief 优先使用显式 Accent，兼容仅覆盖 Highlight 的 Qt palette。 */
    [[nodiscard]] static QColor accent(
        const QPalette &palette);
    /** @brief 根据 WCAG 相对亮度选择对比度更高的黑色或白色。 */
    [[nodiscard]] static QColor contrastingText(
        const QColor &accent);
    /** @brief 返回两个不透明颜色的 WCAG 相对亮度对比值，复用固定查表。 */
    [[nodiscard]] static double contrastRatio(const QColor &first, const QColor &second);
    /** @brief 尊重显式 HighlightedText，否则由有效强调色计算前景。 */
    [[nodiscard]] static QColor text(
        const QPalette &palette);
    /** @brief 计算悬停与按下填充；可指定实际前景，默认沿用强调色文字。 */
    [[nodiscard]] static QColor fill(
        const QPalette &palette, bool hovered, bool pressed, const QColor &foreground = {});
};

} // namespace ZzFluentUI
