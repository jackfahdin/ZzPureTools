#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定标题栏菜单的展开、折叠或自适应展示策略。 */
enum class ZzTitleBarMenuDisplayMode : std::uint8_t
{
    /** @brief 始终显示横向菜单栏。 */
    Expanded,
    /** @brief 始终显示单个折叠菜单按钮。 */
    Compact,
    /** @brief 根据可用宽度和迟滞区自动选择展示方式。 */
    Adaptive
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzTitleBarMenuDisplayMode)
