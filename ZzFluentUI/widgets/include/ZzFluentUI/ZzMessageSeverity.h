#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 标识消息条的纯展示严重性。 */
enum class ZzMessageSeverity : std::uint8_t
{
    Information,
    Success,
    Warning,
    Error
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzMessageSeverity)
