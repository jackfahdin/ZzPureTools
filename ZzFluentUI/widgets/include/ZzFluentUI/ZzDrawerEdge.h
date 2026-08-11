#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 标识 Drawer 使用的物理宿主边缘，不随布局方向反转。 */
enum class ZzDrawerEdge : std::uint8_t
{
    Left,
    Right
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzDrawerEdge)
