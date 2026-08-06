#pragma once

#include <QtCore/QModelIndex>

namespace ZzFluentUI {

class ZzNavigationView;
class ZzNavigationItemDelegate;

/** @brief 保存紧凑状态并统一校验模型激活意图。 */
class ZzNavigationViewPrivate final
{
public:
    /** @brief 绑定非空导航视图。 */
    explicit ZzNavigationViewPrivate(
        ZzNavigationView *publicObject) noexcept;

    /** @brief 仅对有效且启用的索引发出导航意图。 */
    void activateIndex(const QModelIndex &index);

    /** @brief 同步 private delegate 的固定行高和紧凑绘制。 */
    void setCompactPresentation(bool compact);

    ZzNavigationView *const q_ptr;
    ZzNavigationItemDelegate *delegate = nullptr;
    bool compact = false;
};

} // namespace ZzFluentUI
