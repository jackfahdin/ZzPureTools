#pragma once

#include <QtCore/QMetaType>
#include <QtCore/Qt>

namespace ZzFluentUI {

/** @brief 定义 Activity Bar 所消费的展示模型附加角色。 */
enum class ZzActivityItemRole : int
{
    /** @brief 返回 ZzActivityArea，决定入口出现的固定分组。 */
    Area = Qt::UserRole + 0x180,
    /** @brief 返回非负整数徽标；0 不绘制，大于 99 显示为 99+。 */
    Badge
};

} // namespace ZzFluentUI
