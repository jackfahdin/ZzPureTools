#include "ZzControlAppearancePrivate.h"

#include <array>
#include <cmath>

namespace ZzFluentUI {

QColor ZzControlAppearancePrivate::accent(const QPalette &palette)
{
    const auto group = palette.currentColorGroup();
    return palette.color(palette.isBrushSet(group, QPalette::Accent)
            || !palette.isBrushSet(group, QPalette::Highlight)
        ? QPalette::Accent : QPalette::Highlight);
}

QColor ZzControlAppearancePrivate::contrastingText(const QColor &accent)
{
    // 线性化表仅初始化一次，绘制热路径只有三次查表和乘加。
    static const auto linear = [] {
        std::array<double, 256> values{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            const double channel = static_cast<double>(index) / 255.0;
            values[index] = channel <= 0.04045
                ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        }
        return values;
    }();
    const double luminance = 0.2126 * linear[static_cast<std::size_t>(accent.red())]
        + 0.7152 * linear[static_cast<std::size_t>(accent.green())]
        + 0.0722 * linear[static_cast<std::size_t>(accent.blue())];
    return luminance > 0.179 ? QColorConstants::Black : QColorConstants::White;
}

QColor ZzControlAppearancePrivate::text(const QPalette &palette)
{
    return palette.isBrushSet(palette.currentColorGroup(), QPalette::HighlightedText)
        ? palette.color(QPalette::HighlightedText) : contrastingText(accent(palette));
}

QColor ZzControlAppearancePrivate::fill(const QPalette &palette, bool hovered, bool pressed)
{
    const QColor base = accent(palette);
    const int amount = pressed ? 16 : (hovered ? 8 : 0);
    int target = text(palette).lightness() > 127 ? 0 : 255;
    // 纯黑、纯白向同色混合不会产生反馈；此时小幅向反方向混合，
    // 仍保留远高于普通文字要求的黑白对比度。
    if (base.red() == target && base.green() == target && base.blue() == target) {
        target = 255 - target;
    }
    const auto blend = [amount, target](int channel) {
        return (channel * (100 - amount) + target * amount + 50) / 100;
    };
    return QColor::fromRgb(blend(base.red()), blend(base.green()), blend(base.blue()), base.alpha());
}

} // namespace ZzFluentUI
