#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include <QtCore/QList>
#include <QtCore/Qt>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzTabGroupId.h>

namespace ZzFluentUI {

class ZzSplitWorkspacePrivate;
class ZzTabWidget;

/** @brief 指定新标签组位于来源组的物理前侧或后侧。 */
enum class ZzSplitPlacement : std::uint8_t
{
    Before,
    After
};

/**
 * @brief 使用有界递归分割树承载多个标签组。
 *
 * 工作区始终保留至少一个标签组，最多承载 64 组且树深不超过 16。
 * 同方向分割会合并为同一分支，删除空组后会自动收敛单子分支。
 */
class ZZ_FLUENT_UI_EXPORT ZzSplitWorkspace final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSplitWorkspace)

public:
    /**
     * @brief 创建包含一个自动生成标签组的工作区。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzSplitWorkspace(QWidget *parent = nullptr);

    /** @brief 销毁分割树及其拥有的全部标签组。 */
    ~ZzSplitWorkspace() override;

    /** @brief 按稳定树顺序返回全部标签组标识。 */
    [[nodiscard]] QList<ZzTabGroupId> groupIds() const;

    /** @brief 返回当前活动标签组标识。 */
    [[nodiscard]] ZzTabGroupId activeGroupId() const;

    /**
     * @brief 将已有标签组设为活动组并把焦点移入其标签容器。
     * @param id 已存在的标签组标识。
     * @return 组存在时返回 true；重复设置不会重复发出信号。
     */
    bool setActiveGroup(const ZzTabGroupId &id);

    /**
     * @brief 返回指定组唯一拥有的标签容器。
     * @param id 标签组标识。
     * @return 对应容器；组不存在时返回 nullptr。
     */
    [[nodiscard]] ZzTabWidget *tabWidget(
        const ZzTabGroupId &id) const noexcept;

    /**
     * @brief 返回标签容器对应的稳定组标识。
     * @param tabs 当前工作区拥有的标签容器。
     * @return 对应标识；容器不属于工作区时返回无效标识。
     */
    [[nodiscard]] ZzTabGroupId groupId(
        const ZzTabWidget *tabs) const;

    /**
     * @brief 在来源组旁创建一个空标签组并提交树结构变更。
     * @param source 已存在的来源组。
     * @param orientation 分割方向，只接受 Horizontal 或 Vertical。
     * @param placement 新组位于来源组的前侧或后侧。
     * @param requestedId 可选稳定标识；无效值表示自动生成。
     * @return 成功时返回新组标识；重复标识或超过边界时返回空值。
     *
     * 操作先完整校验组数、深度和标识，再一次性修改树；只有成功提交
     * 后才发出 groupAdded 与 layoutChanged。
     */
    [[nodiscard]] std::optional<ZzTabGroupId> splitGroup(
        const ZzTabGroupId &source,
        Qt::Orientation orientation,
        ZzSplitPlacement placement,
        const ZzTabGroupId &requestedId = {});

    /**
     * @brief 删除一个没有标签页的非末组并收敛分割树。
     * @param id 待删除标签组标识。
     * @return 成功提交删除时返回 true。
     *
     * 非空组、未知组和最后一个组不会改变结构或发出生命周期信号。
     */
    bool removeEmptyGroup(const ZzTabGroupId &id);

    /**
     * @brief 按实际全局几何将焦点移到指定物理方向的最近标签组。
     * @param direction LeftEdge、TopEdge、RightEdge 或 BottomEdge。
     * @return 找到并聚焦相邻组时返回 true；边界处返回 false。
     */
    bool focusAdjacentGroup(Qt::Edge direction);

Q_SIGNALS:
    /** @brief 活动标签组实际变化后发出。 */
    void activeGroupChanged(const ZzTabGroupId &id);

    /** @brief 新标签组成功加入树后发出。 */
    void groupAdded(const ZzTabGroupId &id);

    /** @brief 空标签组已从树结构提交移除后发出。 */
    void groupAboutToBeRemoved(const ZzTabGroupId &id);

    /** @brief 分割树结构成功提交变化后发出。 */
    void layoutChanged();

private:
    friend class ZzSplitWorkspacePrivate;
    std::unique_ptr<ZzSplitWorkspacePrivate> d_ptr;
};

} // namespace ZzFluentUI
