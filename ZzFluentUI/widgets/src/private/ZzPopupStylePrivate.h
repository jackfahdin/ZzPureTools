#pragma once

#include <QtCore/QRect>

class QWidget;

namespace ZzFluentUI {

/** @brief 菜单栏高亮与关联弹层共享的水平内缩。 */
inline constexpr int zzMenuBarItemHorizontalInset = 2;

/** @brief 用公开 Qt 接口适配标准菜单与组合框弹层，不接管内容或选择。 */
class ZzPopupStylePrivate final
{
public:
    /** @brief 安装透明圆角或恢复控件原有的背景属性。 */
    static void polish(QWidget *widget, bool enabled);

    /** @brief 保留 Qt 屏幕避让，在组合框上下各预留相同可见间隔。 */
    [[nodiscard]] static QRect comboAnchorRect(const QRect &rect);

    /** @brief 标准菜单显示时对齐所属菜单栏的可见高亮边缘。 */
    static void alignMenuBarPopup(QWidget *widget);
};

} // namespace ZzFluentUI
