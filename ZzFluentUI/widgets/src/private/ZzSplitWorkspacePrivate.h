#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/Qt>

#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabGroupId.h>

class QSplitter;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QEvent;
class QMimeData;
class QObject;
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
    QPointer<ZzTabWidget> tabs;
};

/** @brief 保存可重建的单个树节点及对应分割器尺寸。 */
struct ZzTreeNodeSnapshot final
{
    bool leaf = true;
    ZzTabGroupId id;
    QPointer<ZzTabWidget> tabs;
    Qt::Orientation orientation = Qt::Horizontal;
    QList<int> sizes;
    std::vector<ZzTreeNodeSnapshot> children;
};

/** @brief 保存失败回滚所需的完整分割树和活动组。 */
struct ZzTreeSnapshot final
{
    ZzTreeNodeSnapshot root;
    ZzTabGroupId activeId;
};

/** @brief 保存一次实例内拖放令牌绑定的稳定来源。 */
struct ZzWorkspaceDragRecord final
{
    ZzTabGroupId sourceId;
    int sourceIndex = -1;
    QPointer<QWidget> page;
    std::chrono::steady_clock::time_point deadline;
};

/** @brief 返回统一标签事务提交后需要发布的公开状态。 */
struct ZzWorkspaceTransferResult final
{
    bool committed = false;
    bool layoutChanged = false;
    bool groupAdded = false;
    bool sourceRemoved = false;
    bool activeChanged = false;
    ZzTabGroupId sourceId;
    ZzTabGroupId destinationId;
    ZzWorkspaceDropZone zone = ZzWorkspaceDropZone::Center;
    QPointer<QWidget> page;
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
        const ZzTabGroupId &requestedId,
        bool rebuildViewAfterCommit = true);

    /** @brief 提交空叶子删除并收敛分支。 */
    bool removeEmptyGroup(
        const ZzTabGroupId &id,
        bool rebuildViewAfterCommit = true);

    /** @brief 返回指定物理方向最近叶子的标识。 */
    [[nodiscard]] ZzTabGroupId adjacentGroup(Qt::Edge direction) const;

    /** @brief 复用标签容器原子能力并验证最终页面所有权。 */
    bool transferTab(
        const ZzTabGroupId &source,
        int sourceIndex,
        const ZzTabGroupId &target,
        int targetIndex);

    /** @brief 执行 Center 或四边统一事务并返回公开信号事实。 */
    [[nodiscard]] ZzWorkspaceTransferResult moveTabToDropZone(
        const ZzTabGroupId &source,
        int sourceIndex,
        const ZzTabGroupId &target,
        ZzWorkspaceDropZone zone);

    /** @brief 捕获当前分割树、组引用、活动组和 splitter sizes。 */
    [[nodiscard]] ZzTreeSnapshot captureTreeSnapshot() const;

    /** @brief 原子替换模型并恢复捕获的 splitter sizes。 */
    bool restoreTreeSnapshot(const ZzTreeSnapshot &snapshot);

    /** @brief 将公开工作区或标签栏拖放事件交给统一处理器。 */
    bool eventFilter(QObject *watched, QEvent *event);

    /** @brief 验证载荷并显示当前物理 DropZone。 */
    bool handleDragEnter(QWidget *watched, QDragEnterEvent *event);

    /** @brief 更新当前物理 DropZone 和共享覆盖层。 */
    bool handleDragMove(QWidget *watched, QDragMoveEvent *event);

    /** @brief 清理共享覆盖层和未消费令牌。 */
    bool handleDragLeave(QDragLeaveEvent *event);

    /** @brief 消费实例内令牌并提交标签拖放事务。 */
    bool handleDrop(QWidget *watched, QDropEvent *event);

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

    /** @brief 递归捕获树节点及当前分割尺寸。 */
    [[nodiscard]] static ZzTreeNodeSnapshot captureNodeSnapshot(
        const ZzNode *node);

    /** @brief 使用原标签容器或等价空容器重建树节点。 */
    [[nodiscard]] std::unique_ptr<ZzNode> buildSnapshotNode(
        const ZzTreeNodeSnapshot &snapshot,
        ZzNode *parent);

    /** @brief 递归恢复快照中的分割尺寸。 */
    static void restoreNodeSizes(
        const ZzTreeNodeSnapshot &snapshot,
        ZzNode *node);

    /** @brief 为标签容器及其标签栏安装工作区拖放过滤。 */
    void prepareTabs(ZzTabWidget *tabs);

    /** @brief 为真实标签拖拽创建并写入一次性实例令牌。 */
    bool ensureDragToken(const QMimeData *mimeData);

    /** @brief 严格解析并验证当前实例内拖放载荷。 */
    [[nodiscard]] std::optional<ZzWorkspaceDragRecord> dragRecord(
        const QMimeData *mimeData);

    /** @brief 返回工作区坐标命中的标签组。 */
    [[nodiscard]] ZzTabGroupId groupAt(const QPoint &position) const;

    /** @brief 返回目标组内工作区坐标对应的五区。 */
    [[nodiscard]] ZzWorkspaceDropZone dropZoneAt(
        const ZzTabGroupId &target,
        const QPoint &position) const;

    /** @brief 返回目标五区覆盖层在工作区中的几何。 */
    [[nodiscard]] QRect dropZoneRect(
        const ZzTabGroupId &target,
        ZzWorkspaceDropZone zone) const;

    /** @brief 按需创建唯一共享覆盖层并显示指定区域。 */
    void showDropOverlay(const QRect &geometry);

    /** @brief 隐藏共享覆盖层。 */
    void hideDropOverlay();

    /** @brief 清空当前实例内的拖放令牌集合。 */
    void discardDragTokens();

    /** @brief 返回自动生成的无花括号 UUID 标识。 */
    [[nodiscard]] static ZzTabGroupId createGroupId();

    ZzSplitWorkspace *const q_ptr;
    QWidget *rootHost = nullptr;
    QVBoxLayout *rootLayout = nullptr;
    std::unique_ptr<ZzNode> root;
    ZzTabGroupId activeId;
    QPointer<QWidget> dropOverlay;
    QHash<QString, ZzWorkspaceDragRecord> dragTokens;
};

} // namespace ZzFluentUI
