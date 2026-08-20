#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定活动入口所属的物理侧与主次分组。 */
enum class ZzActivityArea : std::uint8_t
{
    /** @brief 左侧主分组。 */
    LeftPrimary,
    /** @brief 左侧次分组。 */
    LeftSecondary,
    /** @brief 右侧主分组。 */
    RightPrimary,
    /** @brief 右侧次分组。 */
    RightSecondary
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzActivityArea)
