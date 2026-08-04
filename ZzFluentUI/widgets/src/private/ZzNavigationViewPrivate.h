#pragma once

#include <QtCore/QModelIndex>

namespace ZzFluentUI {

class ZzNavigationView;

/** @brief 保存紧凑状态并统一校验模型激活意图。 */
class ZzNavigationViewPrivate final
{
public:
    /** @brief 绑定非空导航视图。 */
    explicit ZzNavigationViewPrivate(
        ZzNavigationView *publicObject) noexcept;

    /** @brief 仅对有效且启用的索引发出导航意图。 */
    void activateIndex(const QModelIndex &index);

    ZzNavigationView *const q_ptr;
    bool compact = false;
};

} // namespace ZzFluentUI
