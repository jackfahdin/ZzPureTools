#pragma once

#include <cstdint>

#include <QtCore/QFlags>
#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 标识消费者需要执行的主题更新类别。 */
enum class ZzThemeChangeKind : std::uint8_t
{
    None = 0,
    Colors = 1U << 0U,
    Geometry = 1U << 1U,
    Motion = 1U << 2U,
    Accessibility = 1U << 3U
};
Q_DECLARE_FLAGS(ZzThemeChangeKinds, ZzThemeChangeKind)
Q_DECLARE_OPERATORS_FOR_FLAGS(ZzThemeChangeKinds)

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzThemeChangeKinds)
