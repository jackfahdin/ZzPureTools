#pragma once

#include <QtCore/QtTypes>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 提供稳定的设备像素比量化和像素对齐。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzDpiScale final
{
public:
    ZzDpiScale() = delete;

    /**
     * @brief 将设备像素比量化为百分之一单位。
     * @param devicePixelRatio 有限正数；其他值按 1.0 处理。
     * @return 50 到 800 之间的稳定桶值。
     */
    [[nodiscard]] static quint16 bucket(
        qreal devicePixelRatio) noexcept;

    /**
     * @brief 将逻辑像素向上对齐为物理像素。
     * @param logicalPixels 有限正逻辑像素；其他值返回零。
     * @param devicePixelRatio 设备像素比。
     * @return 至少一个且不超过 int 上限的物理像素数。
     */
    [[nodiscard]] static int physicalPixels(
        qreal logicalPixels,
        qreal devicePixelRatio) noexcept;
};

} // namespace ZzFluentUI
