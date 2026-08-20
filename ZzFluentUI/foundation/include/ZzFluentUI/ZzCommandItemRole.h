#pragma once

#include <QtCore/Qt>

namespace ZzFluentUI {

/** @brief 定义命令面板消费的平面模型附加数据角色。 */
enum class ZzCommandItemRole : int
{
    /** @brief 返回 QStringList 类型的可搜索关键词。 */
    Keywords = Qt::UserRole + 0x1c0,
    /** @brief 返回 QKeySequence 或可显示的快捷键字符串。 */
    Shortcut,
    /** @brief 返回可选的 QString 分组名称。 */
    Group,
    /** @brief 返回可选整数；数值越大排序越靠前。 */
    Priority
};

} // namespace ZzFluentUI
