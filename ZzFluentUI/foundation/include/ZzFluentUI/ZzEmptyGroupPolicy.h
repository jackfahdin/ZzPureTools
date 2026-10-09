#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 定义工作区空组的处理策略。 */
enum class ZzEmptyGroupPolicy : std::uint8_t
{
    /** @brief 保留空组。 */
    Keep,
    /** @brief 移除空组。 */
    Remove,
    /** @brief 除非这是最后一个组，否则移除空组。 */
    RemoveUnlessLast
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzEmptyGroupPolicy)
