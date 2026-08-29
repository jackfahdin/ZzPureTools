#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QPoint>
#include <QtCore/QPointer>

#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>

class QAbstractItemModel;
class QEvent;
class QListView;
class QMimeData;
class QModelIndex;
class QStyledItemDelegate;
class QTimer;

namespace ZzFluentUI {

class ZzActivityBar;
class ZzActivityProjectionModel;

/** @brief 管理固定双投影、源索引同步和进程内移动令牌。 */
class ZzActivityBarPrivate final
{
public:
    /** @brief 创建两个固定列表视图、一个共享 delegate 和无间距布局。 */
    explicit ZzActivityBarPrivate(
        ZzActivityBar *publicObject,
        ZzSidePaneEdge initialEdge);

    /** @brief 解除模型观察和进程内令牌记录。 */
    ~ZzActivityBarPrivate();

    /** @brief 切换物理侧并刷新两个区域投影。 */
    void setEdge(ZzSidePaneEdge newEdge);

    /** @brief 按 Secondary 投影行数将底部视图锚定到 Activity Bar 底边。 */
    void updateSecondaryViewGeometry();

    /** @brief 更换非拥有模型并清空当前源索引。 */
    void setModel(QAbstractItemModel *model);

    /** @brief 处理外部模型析构并清空固定投影。 */
    void handleModelDestroyed();

    /** @brief 按源索引同步两个视图的唯一选择。 */
    void setCurrentSourceIndex(const QModelIndex &index);

    /** @brief 设置按输入顺序去重的多个活动源索引。 */
    void setActiveSourceIndexes(const QList<QModelIndex> &indexes);

    /** @brief 设置是否绘制 ActivityBar 的选中背景和指示条。 */
    void setSelectionVisible(bool visible);

    /** @brief 清除当前模型或物理侧投影外的失效活动索引。 */
    void sanitizeActiveIndexes();

    /** @brief 判断源索引是否属于当前有效活动集合。 */
    [[nodiscard]] bool isSourceIndexActive(const QModelIndex &index) const;

    /** @brief 判断投影视图索引映射的源项是否处于活动集合。 */
    [[nodiscard]] bool isProjectionIndexActive(const QModelIndex &index) const;

    /** @brief 判断索引是否属于当前模型、顶层 column 0 和物理侧投影。 */
    [[nodiscard]] bool acceptsSourceIndex(const QModelIndex &index) const;

    /** @brief 处理点击或键盘激活，只发出公开意图。 */
    void activateSourceIndex(const QModelIndex &index);

    /** @brief 将鼠标点击激活推迟到释放事件结束后，并复核模型状态。 */
    void scheduleSourceActivation(const QModelIndex &index);

    /** @brief 在两个分组中以确定顺序处理键盘移动和激活。 */
    bool handleKey(QListView *view, int key);

    /** @brief 按需显示只包含三个其他目标区域的移动菜单。 */
    bool showMoveContextMenu(QListView *view, const QPoint &viewportPosition);

    /** @brief 返回源模型中指定区域当前包含的行数。 */
    [[nodiscard]] int rowCountForArea(ZzActivityArea area) const;

    /** @brief 创建一次性进程内拖放令牌并返回 MIME 数据。 */
    [[nodiscard]] QMimeData *createMimeData(const QModelIndex &sourceIndex);

    /** @brief 验证令牌后发出移动意图，绝不调用源模型修改接口。 */
    bool handleDrop(QListView *targetView, const QMimeData *mimeData, int y);

    /** @brief 清理取消拖放留下的进程内令牌。 */
    void discardDragTokens();

    /** @brief 返回指定固定视图对应的物理分组。 */
    [[nodiscard]] ZzActivityArea areaForView(const QListView *view) const;

    ZzActivityBar *const q_ptr;
    ZzActivityProjectionModel *primaryProjection = nullptr;
    ZzActivityProjectionModel *secondaryProjection = nullptr;
    QListView *primaryView = nullptr;
    QListView *secondaryView = nullptr;
    QStyledItemDelegate *delegate = nullptr;
    QPointer<QAbstractItemModel> sourceModel;
    QPersistentModelIndex currentSourceIndex;
    bool multiActiveEnabled = false;
    bool selectionVisible = true;
    QList<QPersistentModelIndex> activeSourceIndexes;
    QMetaObject::Connection modelDestroyedConnection;
    QList<QMetaObject::Connection> activeModelConnections;
    QHash<QString, QPersistentModelIndex> dragTokens;
    QTimer *dragTokenExpiryTimer = nullptr;
    ZzSidePaneEdge edge = ZzSidePaneEdge::Left;
};

} // namespace ZzFluentUI
