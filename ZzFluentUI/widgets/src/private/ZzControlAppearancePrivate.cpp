#include "ZzControlAppearancePrivate.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {

namespace {

/** @brief 使用固定 sRGB 查表计算相对亮度，不在绘制热路径调用 pow。 */
double zzRelativeLuminance(const QColor &color)
{
    static const auto linear = [] {
        std::array<double, 256> values{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            const double channel = static_cast<double>(index) / 255.0;
            values[index] = channel <= 0.04045
                ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        }
        return values;
    }();
    return 0.2126 * linear[static_cast<std::size_t>(color.red())]
        + 0.7152 * linear[static_cast<std::size_t>(color.green())]
        + 0.0722 * linear[static_cast<std::size_t>(color.blue())];
}

} // namespace

QColor ZzControlAppearancePrivate::accent(const QPalette &palette)
{
    const auto group = palette.currentColorGroup();
    return palette.color(palette.isBrushSet(group, QPalette::Accent)
            || !palette.isBrushSet(group, QPalette::Highlight)
        ? QPalette::Accent : QPalette::Highlight);
}

QColor ZzControlAppearancePrivate::contrastingText(const QColor &accent)
{
    return zzRelativeLuminance(accent) > 0.179 ? QColorConstants::Black : QColorConstants::White;
}

double ZzControlAppearancePrivate::contrastRatio(const QColor &first, const QColor &second)
{
    const double firstLuminance = zzRelativeLuminance(first);
    const double secondLuminance = zzRelativeLuminance(second);
    return (std::max(firstLuminance, secondLuminance) + 0.05)
        / (std::min(firstLuminance, secondLuminance) + 0.05);
}

QColor ZzControlAppearancePrivate::text(const QPalette &palette)
{
    return palette.isBrushSet(palette.currentColorGroup(), QPalette::HighlightedText)
        ? palette.color(QPalette::HighlightedText) : contrastingText(accent(palette));
}

QColor ZzControlAppearancePrivate::fill(
    const QPalette &palette, bool hovered, bool pressed, const QColor &foreground)
{
    const QColor base = accent(palette);
    const int amount = pressed ? 16 : (hovered ? 8 : 0);
    int target = (foreground.isValid() ? foreground : text(palette)).lightness() > 127 ? 0 : 255;
    // 默认文字配色的纯黑白可反向混色以提供反馈。显式前景可能搭配
    // 半透明轨道，反向混色会降低合成后的对比；开关改由圆点形变提供反馈。
    if (!foreground.isValid()
        && base.red() == target && base.green() == target && base.blue() == target) {
        target = 255 - target;
    }
    const auto blend = [amount, target](int channel) {
        return (channel * (100 - amount) + target * amount + 50) / 100;
    };
    return QColor::fromRgb(blend(base.red()), blend(base.green()), blend(base.blue()), base.alpha());
}

} // namespace ZzFluentUI
