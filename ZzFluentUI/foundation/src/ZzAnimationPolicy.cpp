#include <ZzFluentUI/ZzAnimationPolicy.h>

#include <algorithm>

namespace ZzFluentUI {

int ZzAnimationPolicy::adjustedDuration(
    int durationMilliseconds,
    bool reducedMotion,
    bool essential) noexcept
{
    const int bounded = std::clamp(durationMilliseconds, 0, 10000);
    if (!reducedMotion) {
        return bounded;
    }
    return essential ? std::min(bounded, 50) : 0;
}

} // namespace ZzFluentUI
