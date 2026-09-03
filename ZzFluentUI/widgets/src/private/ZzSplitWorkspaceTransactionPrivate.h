#pragma once

#include <memory>
#include <QtCore/QList>
#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzTabGroupId.h>

class QWidget;

namespace ZzFluentUI {

class ZzSplitWorkspace;

/** @brief 仅在组件内部使用的工作区事务与状态快照桥接。 */
class ZZ_FLUENT_UI_EXPORT ZzSplitWorkspaceTransactionPrivate final
{
public:
    static void begin(ZzSplitWorkspace *workspace);
    static void end(ZzSplitWorkspace *workspace);
    [[nodiscard]] static std::shared_ptr<void> capture(
        const ZzSplitWorkspace *workspace);
    [[nodiscard]] static bool matches(
        const ZzSplitWorkspace *workspace,
        const std::shared_ptr<void> &snapshot);
    [[nodiscard]] static bool restore(
        ZzSplitWorkspace *workspace,
        const std::shared_ptr<void> &snapshot);
    [[nodiscard]] static ZzCore::ZzResult<void> transferSilently(
        ZzSplitWorkspace *source,
        const ZzTabGroupId &sourceGroup,
        int sourceIndex,
        ZzSplitWorkspace *target,
        const ZzTabGroupId &targetGroup,
        int targetIndex = -1);
    [[nodiscard]] static bool restoreGroupOrder(
        ZzSplitWorkspace *workspace,
        const ZzTabGroupId &group,
        const QList<QWidget *> &pages);
};

} // namespace ZzFluentUI
