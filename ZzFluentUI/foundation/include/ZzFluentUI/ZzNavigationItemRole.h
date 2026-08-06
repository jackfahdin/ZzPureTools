#pragma once

#include <QtCore/Qt>

namespace ZzFluentUI {

/** @brief 定义与业务路由无关的导航展示模型角色。 */
enum class ZzNavigationItemRole : int
{
    /** @brief 返回 ZzIconDescriptor。 */
    Icon = Qt::UserRole + 0x100,
    /** @brief 返回在当前项前显示的可选 QString 分区标题。 */
    Section,
    /** @brief 返回 ZzNavigationPlacement。 */
    Placement,
    /** @brief 返回可选的短 QString 徽标文本。 */
    Badge
};

} // namespace ZzFluentUI
