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
    /** @brief 尊重显式 HighlightedText，否则由有效强调色计算前景。 */
    [[nodiscard]] static QColor text(
        const QPalette &palette);
    /** @brief 计算悬停与按下填充，朝远离前景色的方向混色以保持文字对比度。 */
    [[nodiscard]] static QColor fill(
        const QPalette &palette, bool hovered, bool pressed);
};

} // namespace ZzFluentUI
