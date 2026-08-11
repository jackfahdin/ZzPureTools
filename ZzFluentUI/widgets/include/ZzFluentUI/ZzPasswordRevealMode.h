#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 定义密码文本的显示策略。 */
enum class ZzPasswordRevealMode : std::uint8_t
{
    /** @brief 始终隐藏密码且不显示查看按钮。 */
    Hidden,

    /** @brief 仅在按住查看按钮期间显示密码。 */
    Peek,

    /** @brief 始终以普通文本显示密码且不显示查看按钮。 */
    Visible,
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzPasswordRevealMode)
