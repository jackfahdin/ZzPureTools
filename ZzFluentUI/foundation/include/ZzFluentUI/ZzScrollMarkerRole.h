#pragma once

#include <QtCore/Qt>

namespace ZzFluentUI {

/** @brief 定义标记滚动条从模型读取数据时使用的角色。 */
enum class ZzScrollMarkerRole : int
{
    Position = Qt::UserRole + 0x200,
    Kind,
    Color,
    Priority
};

} // namespace ZzFluentUI
