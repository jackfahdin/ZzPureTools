#pragma once

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 将主题动效偏好转换为有界动画时长。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzAnimationPolicy final
{
public:
    ZzAnimationPolicy() = delete;

    /**
     * @brief 调整非负时长。
     * @param durationMilliseconds 原始毫秒数，负值按零处理。
     * @param reducedMotion 是否减少非必要动效。
     * @param essential 是否为表达即时状态反馈所必需的过渡。
     * @return 0 到 10000 毫秒；减少动效时必要动画最多 50 毫秒。
     */
    [[nodiscard]] static int adjustedDuration(
        int durationMilliseconds,
        bool reducedMotion,
        bool essential) noexcept;
};

} // namespace ZzFluentUI
