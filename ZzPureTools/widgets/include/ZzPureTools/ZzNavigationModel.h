#pragma once

#include <memory>

#include <QtCore/QAbstractListModel>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QVariant>

#include <ZzCore/ZzResult.h>

#include <ZzFluentUI/ZzNavigationItemRole.h>

#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPureToolsExport.h>

namespace ZzPureTools {

class ZzNavigationModelPrivate;

/** @brief 定义导航模型在 DisplayRole 之外公开的稳定数据角色。 */
enum class ZzNavigationRole : int
{
    /** @brief 返回 ZzRouteId 值。 */
    Route = Qt::UserRole + 1,
    /** @brief 返回 ZzFluentUI::ZzIconDescriptor 值。 */
    Icon = static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Icon),
    /** @brief 返回当前节点前可选的已翻译分区标题。 */
    Section = static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Section),
    /** @brief 返回 ZzFluentUI::ZzNavigationPlacement 值。 */
    Placement = static_cast<int>(
        ZzFluentUI::ZzNavigationItemRole::Placement),
    /** @brief 返回可选短徽标文本。 */
    Badge = static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge)
};

/** @brief 提供只读、可重新翻译且不持有页面实例的导航列表模型。 */
class ZZ_PURE_TOOLS_EXPORT ZzNavigationModel final
    : public QAbstractListModel
{
public:
    /**
     * @brief 创建空导航模型。
     * @param parent 可选 QObject 父对象。
     */
    explicit ZzNavigationModel(QObject *parent = nullptr);

    /** @brief 销毁导航节点和翻译缓存。 */
    ~ZzNavigationModel() override;

    /** @brief 禁止复制 QObject 模型。 */
    ZzNavigationModel(const ZzNavigationModel &) = delete;

    /** @brief 禁止复制赋值 QObject 模型。 */
    ZzNavigationModel &operator=(const ZzNavigationModel &) = delete;

    /** @brief 禁止移动 QObject 模型。 */
    ZzNavigationModel(ZzNavigationModel &&) = delete;

    /** @brief 禁止移动赋值 QObject 模型。 */
    ZzNavigationModel &operator=(ZzNavigationModel &&) = delete;

    /**
     * @brief 返回根列表的节点数量。
     * @param parent 有效父索引返回 0。
     * @return 根列表节点数量。
     */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 返回指定节点的翻译标题、路由或图标。
     * @param index 有效根列表索引。
     * @param role Qt DisplayRole 或 ZzNavigationRole。
     * @return 对应角色值；索引或角色无效时返回空 QVariant。
     */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    /** @brief 返回 display、route 和全部导航展示角色的稳定名称。 */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 原子替换经过完整校验的导航节点。
     * @param nodes 拥有值的导航节点列表。
     * @return 替换成功，或无效、重复路由及空翻译键错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> setNodes(
        QList<ZzNavigationNode> nodes);

    /**
     * @brief 按当前行读取拥有值的导航节点。
     * @param row 从零开始的模型行号。
     * @return 节点副本，或越界错误。
     */
    [[nodiscard]] ZzCore::ZzResult<ZzNavigationNode> nodeAt(
        qsizetype row) const;

    /**
     * @brief 按强类型路由返回当前顶层模型索引。
     * @param routeId 必须有效且存在于当前节点集合的路由。
     * @return column 0 顶层索引，或参数无效及路由不存在错误。
     */
    [[nodiscard]] ZzCore::ZzResult<QModelIndex> indexForRoute(
        const ZzRouteId &routeId) const;

    /**
     * @brief 只更新一个路由的短徽标并发出局部数据变化。
     * @param routeId 必须有效且存在于当前节点集合的路由。
     * @param badgeText 已去除首尾空白且不超过八个 UTF-16 code unit 的文本。
     * @return 更新成功，或参数无效及路由不存在错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> setBadge(
        const ZzRouteId &routeId,
        QString badgeText);

    /** @brief 使用当前安装的 translators 刷新标题缓存并通知视图。 */
    void refreshTranslations();

private:
    friend class ZzNavigationModelPrivate;
    std::unique_ptr<ZzNavigationModelPrivate> d_ptr;
};

} // namespace ZzPureTools
