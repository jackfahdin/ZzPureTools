#include "ZzNavigationModelPrivate.h"

#include <limits>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QSet>
#include <QtCore/QString>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzNavigationModel.h>

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzNavigationModelFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), std::move(context)));
}

[[nodiscard]] QString zzTranslateNode(const ZzNavigationNode &node)
{
    const QByteArray context = node.titleTranslationContext.toUtf8();
    const QByteArray source = node.titleSourceText.toUtf8();
    return QCoreApplication::translate(
        context.constData(), source.constData());
}

} // namespace

ZzNavigationModelPrivate::ZzNavigationModelPrivate(
    ZzNavigationModel *model)
    : q_ptr(model)
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }
}

ZzCore::ZzResult<void> ZzNavigationModelPrivate::setNodes(
    QList<ZzNavigationNode> newNodes)
{
    if (newNodes.size() > std::numeric_limits<int>::max()) {
        return zzNavigationModelFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("navigation model exceeds QModelIndex row capacity"));
    }

    QSet<ZzRouteId> routeIds;
    routeIds.reserve(newNodes.size());
    for (qsizetype row = 0; row < newNodes.size(); ++row) {
        const auto &node = newNodes.at(row);
        if (!node.routeId.isValid()) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("navigation route must not be empty"),
                QStringLiteral("row=%1").arg(row));
        }
        if (node.titleTranslationContext.trimmed().isEmpty()) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("navigation title context must not be empty"),
                QStringLiteral("routeId=%1")
                    .arg(node.routeId.value()));
        }
        if (node.titleSourceText.trimmed().isEmpty()) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("navigation title source must not be empty"),
                QStringLiteral("routeId=%1")
                    .arg(node.routeId.value()));
        }
        if (routeIds.contains(node.routeId)) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("navigation route must be unique"),
                QStringLiteral("routeId=%1")
                    .arg(node.routeId.value()));
        }
        routeIds.insert(node.routeId);
    }

    QStringList newTitles;
    newTitles.reserve(newNodes.size());
    for (const auto &node : newNodes) {
        newTitles.append(zzTranslateNode(node));
    }

    q_ptr->beginResetModel();
    nodes = std::move(newNodes);
    translatedTitles = std::move(newTitles);
    q_ptr->endResetModel();
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<ZzNavigationNode> ZzNavigationModelPrivate::nodeAt(
    qsizetype row) const
{
    if (row < 0 || row >= nodes.size()) {
        return zzNavigationModelFailure<ZzNavigationNode>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("navigation row is out of range"),
            QStringLiteral("row=%1; rowCount=%2")
                .arg(row)
                .arg(nodes.size()));
    }
    return ZzCore::ZzResult<ZzNavigationNode>::success(nodes.at(row));
}

void ZzNavigationModelPrivate::refreshTranslations()
{
    for (qsizetype row = 0; row < nodes.size(); ++row) {
        translatedTitles[row] = zzTranslateNode(nodes.at(row));
    }
    if (!nodes.isEmpty()) {
        Q_EMIT q_ptr->dataChanged(
            q_ptr->index(0, 0),
            q_ptr->index(static_cast<int>(nodes.size() - 1), 0),
            {Qt::DisplayRole});
    }
}

} // namespace ZzPureTools
