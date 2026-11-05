#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

class QHBoxLayout;
class QToolButton;
class QLabel;

namespace ZzFluentUI {

class ZzBreadcrumbBar;

/** @brief 保存逻辑路径并重建具有稳定逻辑索引的少量展示按钮。 */
class ZzBreadcrumbBarPrivate final
{
public:
    /** @brief 创建空的无边距横向布局。 */
    explicit ZzBreadcrumbBarPrivate(ZzBreadcrumbBar *q);

    /** @brief 按当前布局方向重建按钮和非交互分隔符。 */
    void rebuild();

    /** @brief 同步所有按钮的 current checked 状态。 */
    void updateCurrentState();

    /** @brief 按可用宽度折叠前缀，并将隐藏项的焦点转移到菜单。 */
    void updateOverflow();

    /** @brief 使用原始 Segoe 字体更新方向分隔符和主题图标。 */
    void updateAppearance();

    ZzBreadcrumbBar *const q_ptr;
    QHBoxLayout *layout = nullptr;
    QList<QToolButton *> buttons;
    QList<QLabel *> separators;
    QToolButton *overflow = nullptr;
    QLabel *overflowSeparator = nullptr;
    int hiddenCount = -1;
    bool updating = false;
    QStringList items;
    int currentIndex = -1;
};

} // namespace ZzFluentUI
