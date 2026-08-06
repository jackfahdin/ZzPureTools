#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定导航目标位于主滚动区或固定页脚区。 */
enum class ZzNavigationPlacement : std::uint8_t
{
    /** @brief 在主导航滚动区展示。 */
    Primary,
    /** @brief 在导航面板底部的固定区域展示。 */
    Footer
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzNavigationPlacement)
