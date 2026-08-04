#include <ZzFluentUI/ZzDpiScale.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ZzFluentUI {

quint16 ZzDpiScale::bucket(qreal devicePixelRatio) noexcept
{
    const qreal valid = std::isfinite(devicePixelRatio)
            && devicePixelRatio > 0.0
        ? devicePixelRatio
        : 1.0;
    const qreal bounded = std::clamp(valid, qreal{0.5}, qreal{8.0});
    return static_cast<quint16>(std::lround(bounded * 100.0));
}

int ZzDpiScale::physicalPixels(
    qreal logicalPixels,
    qreal devicePixelRatio) noexcept
{
    if (!std::isfinite(logicalPixels) || logicalPixels <= 0.0) {
        return 0;
    }
    const qreal ratio = static_cast<qreal>(bucket(devicePixelRatio)) / 100.0;
    const qreal scaled = logicalPixels * ratio;
    const qreal maximum = static_cast<qreal>(
        std::numeric_limits<int>::max());
    if (!std::isfinite(scaled) || scaled >= maximum) {
        return std::numeric_limits<int>::max();
    }
    return std::max(1, static_cast<int>(std::ceil(scaled)));
}

} // namespace ZzFluentUI
