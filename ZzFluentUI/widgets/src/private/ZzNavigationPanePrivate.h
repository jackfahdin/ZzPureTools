#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QPointer>

#include <ZzFluentUI/ZzNavigationDisplayMode.h>

class QAbstractItemModel;
class QModelIndex;
class QTreeView;
class QWidget;

namespace ZzFluentUI {

class ZzNavigationPane;
class ZzNavigationProjectionModel;
class ZzNavigationTreeModel;
class ZzNavigationView;

/** @brief 管理固定双投影、选择映射和自适应顶层窗口观察。 */
class ZzNavigationPanePrivate final
{
public:
    /** @brief 创建两个固定投影、两个固定视图和无间距布局。 */
    explicit ZzNavigationPanePrivate(ZzNavigationPane *publicObject);

    /** @brief 解除 source 和顶层窗口连接。 */
    ~ZzNavigationPanePrivate();

    /** @brief 更换非拥有 source model 并清除旧选择。 */
    void setModel(QAbstractItemModel *model);

    /** @brief 处理 source model 销毁并清空两个投影。 */
    void handleModelDestroyed();

    /** @brief 从任一投影激活源目标并转发用户意图。 */
    void activateProjectedIndex(
        ZzNavigationProjectionModel *projection,
        const QModelIndex &proxyIndex);

    /** @brief 同步两个投影中的唯一 source current index。 */
    void setCurrentSourceIndex(const QModelIndex &index);

    /** @brief 投影 reset 后尝试恢复仍有效的 source current index。 */
    void restoreCurrentSelection();

    /** @brief 刷新 footer 可见性和最多六行的固定高度。 */
    void updateFooterGeometry();

    /** @brief 重新观察当前父级所属的顶层 QWidget。 */
    void rebindAdaptiveWindow();

    /** @brief 按策略和顶层窗口宽度刷新实际 compact 状态。 */
    void syncDisplayMode();

    /** @brief 应用实际 compact 状态并同步两个 view 和 pane 宽度。 */
    void applyCompact(bool compact);

    /** @brief 切换统一侧栏使用的树导航展示。 */
    void setTreeMode(bool enabled);

    /** @brief 从 Tree View 叶节点发出源模型导航意图。 */
    void activateTreeIndex(const QModelIndex &index);

    ZzNavigationPane *const q_ptr;
    ZzNavigationProjectionModel *primaryProjection = nullptr;
    ZzNavigationProjectionModel *footerProjection = nullptr;
    ZzNavigationTreeModel *treeProjection = nullptr;
    ZzNavigationView *primaryView = nullptr;
    ZzNavigationView *footerView = nullptr;
    QTreeView *treeView = nullptr;
    QPointer<QAbstractItemModel> sourceModel;
    QPersistentModelIndex currentSourceIndex;
    QPointer<QWidget> adaptiveWindow;
    QMetaObject::Connection modelDestroyedConnection;
    ZzNavigationDisplayMode displayMode =
        ZzNavigationDisplayMode::Adaptive;
    int adaptiveThreshold = 900;
    bool compact = false;
    bool treeMode = false;
};

} // namespace ZzFluentUI
