#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定信息徽章展示为圆点、数字或图标。 */
enum class ZzInfoBadgeKind : std::uint8_t
{
    Dot,
    Number,
    Icon
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzInfoBadgeKind)
