#pragma once

#include <cstdint>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace ZzFluentUI {

/** @brief 标识导航投影生成主区还是固定页脚区。 */
enum class ZzNavigationProjection : std::uint8_t
{
    Primary,
    Footer,
    All
};

/** @brief 保存一个合成分区行或一个 source model 目标行。 */
struct ZzNavigationProjectionEntry final
{
    QPersistentModelIndex sourceIndex;
    QString sectionTitle;
    bool sectionHeader = false;
};

/** @brief 以冷路径线性重建换取热路径常数时间映射的导航私有投影。 */
class ZzNavigationProjectionModel final : public QAbstractProxyModel
{
public:
    /** @brief 创建固定用途的空投影模型。 */
    explicit ZzNavigationProjectionModel(
        ZzNavigationProjection projection,
        QObject *parent = nullptr);

    /** @brief 断开 source model 的私有结构观察连接。 */
    ~ZzNavigationProjectionModel() override;

    /** @brief 更换非拥有 source model 并原子重建投影。 */
    void setSourceModel(QAbstractItemModel *sourceModel) override;

    /** @brief 将目标投影索引映射回 source；分区行返回无效索引。 */
    [[nodiscard]] QModelIndex mapToSource(
        const QModelIndex &proxyIndex) const override;

    /** @brief 以常数时间把 source 顶层 column 0 索引映射到目标行。 */
    [[nodiscard]] QModelIndex mapFromSource(
        const QModelIndex &sourceIndex) const override;

    /** @brief 返回无父级的单列投影索引。 */
    [[nodiscard]] QModelIndex index(
        int row,
        int column,
        const QModelIndex &parent = QModelIndex()) const override;

    /** @brief 平面投影始终返回无效父级。 */
    [[nodiscard]] QModelIndex parent(
        const QModelIndex &child) const override;

    /** @brief 返回当前投影 entry 数量。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

    /** @brief 有 source model 时根级固定返回一列。 */
    [[nodiscard]] int columnCount(
        const QModelIndex &parent = QModelIndex()) const override;

    /** @brief 分区行返回标题数据，目标行直接转发 source 数据。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    /** @brief 分区行不可操作，目标行转发 source flags。 */
    [[nodiscard]] Qt::ItemFlags flags(
        const QModelIndex &index) const override;

    /** @brief 返回 source role names 并增加私有分区标识。 */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    /** @brief 在 reset 事务中按当前 source 数据重建连续映射。 */
    void rebuild();

    /** @brief 连接 source 的结构和局部数据变化。 */
    void connectSourceModel();

    /** @brief 断开全部私有 source 观察连接。 */
    void disconnectSourceModel();

    /** @brief 转发不改变投影结构的 source 数据变化。 */
    void forwardDataChanged(
        const QModelIndex &topLeft,
        const QModelIndex &bottomRight,
        const QList<int> &roles);

    ZzNavigationProjection projection_;
    QVector<ZzNavigationProjectionEntry> entries_;
    QVector<int> sourceToProxyRow_;
    QList<QMetaObject::Connection> sourceConnections_;
};

} // namespace ZzFluentUI
