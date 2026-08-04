#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

class QHBoxLayout;
class QToolButton;

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

    ZzBreadcrumbBar *const q_ptr;
    QHBoxLayout *layout = nullptr;
    QList<QToolButton *> buttons;
    QStringList items;
    int currentIndex = -1;
};

} // namespace ZzFluentUI
