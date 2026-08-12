#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定评分控件允许提交的最小数值步长。 */
enum class ZzRatingPrecision : std::uint8_t
{
    /** @brief 评分按一个整星量化。 */
    Whole,

    /** @brief 评分按半星量化。 */
    Half,
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzRatingPrecision)
