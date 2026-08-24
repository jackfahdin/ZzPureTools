#pragma once

#include <QtCore/QByteArray>

#include <ZzCore/ZzResult.h>

namespace ZzPureTools {

class ZzWorkspaceShellPrivate;

/** @brief 使用 prepare 阶段固定的目标同步提交或回滚完整工作区布局。 */
class ZzWorkspaceLayoutTransactionPrivate final
{
public:
    /** @brief 绑定 Shell 私有状态，不立即读取或修改 QObject。 */
    explicit ZzWorkspaceLayoutTransactionPrivate(
        ZzWorkspaceShellPrivate &shell) noexcept;

    /** @brief 捕获并按 schema 2 编码当前完整工作区布局。 */
    [[nodiscard]] ZzCore::ZzResult<QByteArray> save() const;

    /** @brief 解码、预计算固定目标并同步执行五阶段恢复事务。 */
    [[nodiscard]] ZzCore::ZzResult<void> restore(
        const QByteArray &encoded);

private:
    ZzWorkspaceShellPrivate &shell_;
};

} // namespace ZzPureTools
