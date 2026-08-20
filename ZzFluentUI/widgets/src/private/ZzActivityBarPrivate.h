#pragma once

#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QPersistentModelIndex>
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

    /** @brief 更换非拥有模型并清空当前源索引。 */
    void setModel(QAbstractItemModel *model);

    /** @brief 处理外部模型析构并清空固定投影。 */
    void handleModelDestroyed();

    /** @brief 按源索引同步两个视图的唯一选择。 */
    void setCurrentSourceIndex(const QModelIndex &index);

    /** @brief 处理点击或键盘激活，只发出公开意图。 */
    void activateSourceIndex(const QModelIndex &index);

    /** @brief 在两个分组中以确定顺序处理键盘移动和激活。 */
    bool handleKey(QListView *view, int key);

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
    QMetaObject::Connection modelDestroyedConnection;
    QHash<QString, QPersistentModelIndex> dragTokens;
    QTimer *dragTokenExpiryTimer = nullptr;
    ZzSidePaneEdge edge = ZzSidePaneEdge::Left;
};

} // namespace ZzFluentUI
