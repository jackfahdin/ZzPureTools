#pragma once

#include <QtCore/QStringList>

#include <ZzFluentUI/ZzActivityArea.h>

#include "ZzWorkspaceLayoutStatePrivate.h"

class QModelIndex;

namespace ZzPureTools {

class ZzWorkspaceShellPrivate;

/** @brief 以固定快照和目标投影执行可回滚的 Activity 面板迁移。 */
class ZzWorkspaceActivityMoveTransactionPrivate final
{
public:
    /** @brief 绑定待执行事务的 Shell 私有状态，不立即修改 QObject。 */
    explicit ZzWorkspaceActivityMoveTransactionPrivate(
        ZzWorkspaceShellPrivate &shell) noexcept;

    /**
     * @brief 捕获原始投影并执行一次 Activity 移动。
     * @param sourceIndex 移动开始前属于当前 Activity model 的源索引。
     * @param targetArea 固定的目标 Activity 分组。
     * @param targetRow 目标分组内从零开始的插入位置。
     * @return 完整提交固定目标时返回 true；回滚或清理时返回 false。
     */
    [[nodiscard]] bool execute(
        const QModelIndex &sourceIndex,
        ZzFluentUI::ZzActivityArea targetArea,
        int targetRow);

private:
    using ZzProjection =
        ZzWorkspaceLayoutStatePrivate::ZzWorkspaceProjection;

    /** @brief 用同一组有界步骤应用正向目标或原始回滚投影。 */
    [[nodiscard]] bool applyProjection(
        const ZzProjection &projection,
        const QStringList &modelOrder,
        bool strict);

    ZzWorkspaceShellPrivate &shell_;
    QString movedId_;
};

} // namespace ZzPureTools
