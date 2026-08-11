#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 标识内容对话框中可作为默认操作的按钮。 */
enum class ZzContentDialogButton : std::uint8_t
{
    None,
    Primary,
    Secondary,
    Close
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzContentDialogButton)
