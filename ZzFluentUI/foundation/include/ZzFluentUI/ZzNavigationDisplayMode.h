#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定导航面板的常规、紧凑或自适应展示策略。 */
enum class ZzNavigationDisplayMode : std::uint8_t
{
    /** @brief 始终展示图标、标题和徽标。 */
    Regular,
    /** @brief 只展示图标和紧凑徽标提示。 */
    Compact,
    /** @brief 根据所属顶层窗口的逻辑宽度选择常规或紧凑模式。 */
    Adaptive
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzNavigationDisplayMode)
