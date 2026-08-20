#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定 Side Pane 的物理停靠边缘，不随 RTL 交换。 */
enum class ZzSidePaneEdge : std::uint8_t
{
    /** @brief 位于宿主左侧。 */
    Left,
    /** @brief 位于宿主右侧。 */
    Right
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzSidePaneEdge)
