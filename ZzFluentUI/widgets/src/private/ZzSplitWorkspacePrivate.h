#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/Qt>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabGroupId.h>

class QSplitter;
class QVBoxLayout;
class QWidget;

namespace ZzFluentUI {

class ZzTabWidget;

struct ZzNode;

/** @brief 保存一个分割方向一致且至少包含两个子节点的分支。 */
struct ZzBranch final
{
    Qt::Orientation orientation = Qt::Horizontal;
    QSplitter *splitter = nullptr;
    std::vector<std::unique_ptr<ZzNode>> children;
};

/** @brief 保存一个标签组稳定标识及其唯一标签容器。 */
struct ZzLeaf final
{
    ZzTabGroupId id;
    ZzTabWidget *tabs = nullptr;
};

/** @brief 表示递归分割树中的 Branch 或 Leaf 节点。 */
struct ZzNode final
{
    explicit ZzNode(ZzBranch branch);
    explicit ZzNode(ZzLeaf leaf);

    ZzNode *parent = nullptr;
    std::variant<ZzBranch, ZzLeaf> value;
};

/** @brief 管理有界分割树、固定根宿主和标签组焦点状态。 */
class ZzSplitWorkspacePrivate final
{
public:
    static constexpr int maximumGroupCount = 64;
    static constexpr int maximumTreeDepth = 16;

    /** @brief 创建固定根宿主和初始叶子。 */
    explicit ZzSplitWorkspacePrivate(ZzSplitWorkspace *publicObject);

    /** @brief 销毁树模型；QObject 子对象由公开工作区销毁。 */
    ~ZzSplitWorkspacePrivate();

    /** @brief 按树顺序返回全部标签组标识。 */
    [[nodiscard]] QList<ZzTabGroupId> groupIds() const;

    /** @brief 按稳定标识查找叶子。 */
    [[nodiscard]] ZzNode *findLeaf(const ZzTabGroupId &id) const noexcept;

    /** @brief 按标签容器身份查找叶子。 */
    [[nodiscard]] ZzNode *findLeaf(
        const ZzTabWidget *tabs) const noexcept;

    /** @brief 提交一次经过组数、深度和标识校验的分割。 */
    [[nodiscard]] std::optional<ZzTabGroupId> splitGroup(
        const ZzTabGroupId &source,
        Qt::Orientation orientation,
        ZzSplitPlacement placement,
        const ZzTabGroupId &requestedId);

    /** @brief 提交空叶子删除并收敛分支。 */
    bool removeEmptyGroup(const ZzTabGroupId &id);

    /** @brief 返回指定物理方向最近叶子的标识。 */
    [[nodiscard]] ZzTabGroupId adjacentGroup(Qt::Edge direction) const;

    /** @brief 在应用焦点进入任意叶子时同步活动标识。 */
    void handleFocusChanged(QWidget *focused);

    /** @brief 返回节点在根节点为第一级时的深度。 */
    [[nodiscard]] static int nodeDepth(const ZzNode *node) noexcept;

    /** @brief 收集子树中的叶子指针。 */
    static void collectLeaves(
        ZzNode *node,
        std::vector<ZzNode *> &leaves);

    /** @brief 收集只读子树中的叶子指针。 */
    static void collectLeaves(
        const ZzNode *node,
        std::vector<const ZzNode *> &leaves);

    /** @brief 递归合并同向分支并提升单子分支。 */
    [[nodiscard]] static std::unique_ptr<ZzNode> normalize(
        std::unique_ptr<ZzNode> node,
        ZzNode *parent);

    /** @brief 将当前模型重新挂接到固定根宿主。 */
    void rebuildView();

    /** @brief 为节点递归创建分割器视图并复用叶子标签容器。 */
    [[nodiscard]] QWidget *buildNodeWidget(
        ZzNode *node,
        QWidget *parent);

    /** @brief 清空模型中即将失效的分割器缓存指针。 */
    static void clearSplitterPointers(ZzNode *node);

    /** @brief 返回自动生成的无花括号 UUID 标识。 */
    [[nodiscard]] static ZzTabGroupId createGroupId();

    ZzSplitWorkspace *const q_ptr;
    QWidget *rootHost = nullptr;
    QVBoxLayout *rootLayout = nullptr;
    std::unique_ptr<ZzNode> root;
    ZzTabGroupId activeId;
};

} // namespace ZzFluentUI
