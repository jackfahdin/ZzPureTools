#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QRect>

#include <ZzFluentUI/ZzButtonAppearance.h>

#include "ZzWidgetTheme.h"

class QMenu;
class QPainter;
class QStyleOptionButton;

namespace ZzFluentUI {

class ZzSplitButton;

/** @brief 保存分割按钮计算后的两个可视命中区域。 */
struct ZzSplitButtonRegions final
{
    QRect main;
    QRect menu;
};

/** @brief 持有分割按钮的派生交互状态和非拥有菜单句柄。 */
class ZzSplitButtonPrivate final
{
public:
    /**
     * @brief 绑定公开按钮并启用稳定的鼠标移动跟踪。
     * @param q 非空、非拥有的公开分割按钮。
     */
    explicit ZzSplitButtonPrivate(ZzSplitButton *q);

    /** @brief 断开外部菜单连接，不改变菜单所有权。 */
    ~ZzSplitButtonPrivate();

    /** @brief 返回按当前方向映射后的主区和菜单区。 */
    [[nodiscard]] ZzSplitButtonRegions regions() const;

    /** @brief 按当前鼠标位置更新两个区域的悬停状态。 */
    void updateHover(const QPoint &position);

    /** @brief 设置视觉外观并只发一次变化信号。 */
    void setAppearance(ZzButtonAppearance value);

    /** @brief 更换非拥有菜单并观察其隐藏和销毁。 */
    void setMenu(QMenu *value);

    /** @brief 发出打开意图并非阻塞弹出当前菜单。 */
    void showMenu();

    /** @brief 使用当前主题和交互状态绘制整个按钮。 */
    void paint(QPainter *painter) const;

    /** @brief 重建非 Fluent 回退快照并刷新尺寸与绘制。 */
    void refreshTheme();

    ZzSplitButton *const q_ptr;
    ZzWidgetTheme theme;
    QPointer<QMenu> menu;
    QMetaObject::Connection menuDestroyedConnection;
    QMetaObject::Connection menuAboutToHideConnection;
    ZzButtonAppearance appearance = ZzButtonAppearance::Standard;
    bool mainHovered = false;
    bool menuHovered = false;
    bool menuPressed = false;
    bool menuArmed = false;
    bool menuOpen = false;

private:
    /** @brief 复制 QPushButton 状态并应用 appearance 调色板。 */
    void initStyleOption(QStyleOptionButton *option) const;
};

} // namespace ZzFluentUI
