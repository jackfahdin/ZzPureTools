#include <ZzPureTools/ZzNavigationModel.h>

#include <utility>

#include "private/ZzNavigationModelPrivate.h"

namespace ZzPureTools {

ZzNavigationModel::ZzNavigationModel(QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(std::make_unique<ZzNavigationModelPrivate>(this))
{
}

ZzNavigationModel::~ZzNavigationModel() = default;

int ZzNavigationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(d_ptr->nodes.size());
}

QVariant ZzNavigationModel::data(
    const QModelIndex &index,
    int role) const
{
    if (!index.isValid() || index.parent().isValid()
        || index.column() != 0 || index.row() < 0
        || index.row() >= d_ptr->nodes.size()) {
        return {};
    }

    const auto row = static_cast<qsizetype>(index.row());
    const auto &node = d_ptr->nodes.at(row);
    if (role == Qt::DisplayRole) {
        return d_ptr->translatedTitles.at(row);
    }
    if (role == Qt::ToolTipRole) {
        const QString &title = d_ptr->translatedTitles.at(row);
        return node.badgeText.isEmpty()
            ? title
            : QStringLiteral("%1 (%2)").arg(title, node.badgeText);
    }
    if (role == Qt::AccessibleDescriptionRole) {
        return node.badgeText;
    }
    if (role == static_cast<int>(ZzNavigationRole::Route)) {
        return QVariant::fromValue(node.routeId);
    }
    if (role == static_cast<int>(ZzNavigationRole::Icon)) {
        return QVariant::fromValue(node.icon);
    }
    if (role == static_cast<int>(ZzNavigationRole::Section)) {
        return d_ptr->translatedSections.at(row);
    }
    if (role == static_cast<int>(ZzNavigationRole::Placement)) {
        return QVariant::fromValue(node.placement);
    }
    if (role == static_cast<int>(ZzNavigationRole::Badge)) {
        return node.badgeText;
    }
    return {};
}

QHash<int, QByteArray> ZzNavigationModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {static_cast<int>(ZzNavigationRole::Route),
         QByteArrayLiteral("route")},
        {static_cast<int>(ZzNavigationRole::Icon),
         QByteArrayLiteral("icon")},
        {static_cast<int>(ZzNavigationRole::Section),
         QByteArrayLiteral("section")},
        {static_cast<int>(ZzNavigationRole::Placement),
         QByteArrayLiteral("placement")},
        {static_cast<int>(ZzNavigationRole::Badge),
         QByteArrayLiteral("badge")}};
}

ZzCore::ZzResult<void> ZzNavigationModel::setNodes(
    QList<ZzNavigationNode> nodes)
{
    return d_ptr->setNodes(std::move(nodes));
}

ZzCore::ZzResult<ZzNavigationNode> ZzNavigationModel::nodeAt(
    qsizetype row) const
{
    return d_ptr->nodeAt(row);
}

ZzCore::ZzResult<QModelIndex> ZzNavigationModel::indexForRoute(
    const ZzRouteId &routeId) const
{
    return d_ptr->indexForRoute(routeId);
}

ZzCore::ZzResult<void> ZzNavigationModel::setBadge(
    const ZzRouteId &routeId,
    QString badgeText)
{
    return d_ptr->setBadge(routeId, std::move(badgeText));
}

void ZzNavigationModel::refreshTranslations()
{
    d_ptr->refreshTranslations();
}

} // namespace ZzPureTools
