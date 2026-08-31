#pragma once

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QVector>

#include <vector>

#include "ZzNavigationProjectionModel.h"

namespace ZzFluentUI {

class ZzNavigationTreeModel final : public QAbstractProxyModel
{
public:
    /** @brief 创建按主导航或页脚位置筛选的树模型。 */
    explicit ZzNavigationTreeModel(
        ZzNavigationProjection projection,
        QObject *parent = nullptr);

    /** @brief 断开源模型观察并释放树节点。 */
    ~ZzNavigationTreeModel() override;

    /** @brief 设置非拥有源模型并重建分组树。 */
    void setSourceModel(QAbstractItemModel *sourceModel) override;

    /** @brief 将树叶节点映射回源模型索引，分组节点返回无效索引。 */
    [[nodiscard]] QModelIndex mapToSource(
        const QModelIndex &proxyIndex) const override;

    /** @brief 将源模型顶层索引映射为树中的叶节点。 */
    [[nodiscard]] QModelIndex mapFromSource(
        const QModelIndex &sourceIndex) const override;

    /** @brief 返回指定父节点下的树行。 */
    [[nodiscard]] QModelIndex index(
        int row,
        int column,
        const QModelIndex &parent = QModelIndex()) const override;

    /** @brief 返回树节点的父分组索引。 */
    [[nodiscard]] QModelIndex parent(
        const QModelIndex &child) const override;

    /** @brief 返回根节点或指定分组下的子节点数量。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

    /** @brief 树模型固定为单列。 */
    [[nodiscard]] int columnCount(
        const QModelIndex &parent = QModelIndex()) const override;

    /** @brief 分组节点提供标题，叶节点转发源模型展示数据。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    /** @brief 分组节点可展开但不可选中，叶节点沿用源模型 flags。 */
    [[nodiscard]] Qt::ItemFlags flags(
        const QModelIndex &index) const override;

    /** @brief 返回源模型角色并公开树分组标识。 */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    struct TreeNode final
    {
        QPersistentModelIndex sourceIndex;
        QString title;
        TreeNode *parent = nullptr;
        std::vector<std::unique_ptr<TreeNode>> children;
        bool section = false;
        int row = 0;
    };

    /** @brief 在 reset 事务中按 Section 和 Placement 重建树。 */
    void rebuild();

    /** @brief 连接源模型结构与局部数据变化。 */
    void connectSourceModel();

    /** @brief 断开源模型的全部观察连接。 */
    void disconnectSourceModel();

    /** @brief 转发叶节点的局部数据变化。 */
    void forwardDataChanged(
        const QModelIndex &topLeft,
        const QModelIndex &bottomRight,
        const QList<int> &roles);

    /** @brief 返回索引对应的内部树节点。 */
    [[nodiscard]] TreeNode *nodeForIndex(
        const QModelIndex &index) const noexcept;

    ZzNavigationProjection projection_;
    std::unique_ptr<TreeNode> root_;
    QVector<QModelIndex> sourceToProxy_;
    QList<QMetaObject::Connection> sourceConnections_;
};

} // namespace ZzFluentUI
