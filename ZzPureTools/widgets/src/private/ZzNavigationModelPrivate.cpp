#include "ZzNavigationModelPrivate.h"

#include <limits>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
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

[[nodiscard]] QString zzTranslateSection(const ZzNavigationNode &node)
{
    if (node.sectionTranslationContext.isEmpty()) {
        return {};
    }
    const QByteArray context = node.sectionTranslationContext.toUtf8();
    const QByteArray source = node.sectionSourceText.toUtf8();
    return QCoreApplication::translate(
        context.constData(), source.constData());
}

[[nodiscard]] bool zzIsValidPlacement(
    ZzFluentUI::ZzNavigationPlacement placement) noexcept
{
    switch (placement) {
    case ZzFluentUI::ZzNavigationPlacement::Primary:
    case ZzFluentUI::ZzNavigationPlacement::Footer:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzContainsLineBreak(const QString &text) noexcept
{
    for (const QChar character : text) {
        if (character == QLatin1Char('\n')
            || character == QLatin1Char('\r')
            || character == QChar::LineSeparator
            || character == QChar::ParagraphSeparator) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool zzIsValidBadge(const QString &badgeText) noexcept
{
    return badgeText == badgeText.trimmed()
        && badgeText.size() <= 8
        && !zzContainsLineBreak(badgeText);
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
    qsizetype footerCount = 0;
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
        const bool hasSectionContext =
            !node.sectionTranslationContext.isEmpty();
        const bool hasSectionSource = !node.sectionSourceText.isEmpty();
        if (hasSectionContext != hasSectionSource
            || (hasSectionContext
                && (node.sectionTranslationContext.trimmed().isEmpty()
                    || node.sectionSourceText.trimmed().isEmpty()))) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("navigation section translation keys must be paired and non-empty"),
                QStringLiteral("routeId=%1")
                    .arg(node.routeId.value()));
        }
        if (!zzIsValidPlacement(node.placement)) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("navigation placement is invalid"),
                QStringLiteral("routeId=%1")
                    .arg(node.routeId.value()));
        }
        if (node.placement
                == ZzFluentUI::ZzNavigationPlacement::Footer
            && hasSectionContext) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("footer navigation node cannot start a section"),
                QStringLiteral("routeId=%1")
                    .arg(node.routeId.value()));
        }
        if (!zzIsValidBadge(node.badgeText)) {
            return zzNavigationModelFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("navigation badge must be trimmed, single-line, and at most eight UTF-16 code units"),
                QStringLiteral("routeId=%1")
                    .arg(node.routeId.value()));
        }
        if (node.placement
            == ZzFluentUI::ZzNavigationPlacement::Footer) {
            ++footerCount;
            if (footerCount > 6) {
                return zzNavigationModelFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("navigation footer cannot contain more than six nodes"));
            }
        }
        routeIds.insert(node.routeId);
    }

    QStringList newTitles;
    QStringList newSections;
    QHash<ZzRouteId, int> newRouteRows;
    newTitles.reserve(newNodes.size());
    newSections.reserve(newNodes.size());
    newRouteRows.reserve(newNodes.size());
    for (qsizetype row = 0; row < newNodes.size(); ++row) {
        const auto &node = newNodes.at(row);
        newTitles.append(zzTranslateNode(node));
        newSections.append(zzTranslateSection(node));
        newRouteRows.insert(node.routeId, static_cast<int>(row));
    }

    q_ptr->beginResetModel();
    nodes = std::move(newNodes);
    translatedTitles = std::move(newTitles);
    translatedSections = std::move(newSections);
    routeRows = std::move(newRouteRows);
    q_ptr->endResetModel();
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<QModelIndex> ZzNavigationModelPrivate::indexForRoute(
    const ZzRouteId &routeId) const
{
    if (!routeId.isValid()) {
        return zzNavigationModelFailure<QModelIndex>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("navigation route must not be empty"));
    }
    const auto iterator = routeRows.constFind(routeId);
    if (iterator == routeRows.cend()) {
        return zzNavigationModelFailure<QModelIndex>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("navigation route does not exist"),
            QStringLiteral("routeId=%1").arg(routeId.value()));
    }
    return ZzCore::ZzResult<QModelIndex>::success(
        q_ptr->index(iterator.value(), 0));
}

ZzCore::ZzResult<void> ZzNavigationModelPrivate::setBadge(
    const ZzRouteId &routeId,
    QString badgeText)
{
    if (!routeId.isValid()) {
        return zzNavigationModelFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("navigation route must not be empty"));
    }
    if (!zzIsValidBadge(badgeText)) {
        return zzNavigationModelFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("navigation badge must be trimmed, single-line, and at most eight UTF-16 code units"),
            QStringLiteral("routeId=%1").arg(routeId.value()));
    }
    const auto iterator = routeRows.constFind(routeId);
    if (iterator == routeRows.cend()) {
        return zzNavigationModelFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("navigation route does not exist"),
            QStringLiteral("routeId=%1").arg(routeId.value()));
    }

    const qsizetype row = static_cast<qsizetype>(iterator.value());
    if (nodes.at(row).badgeText == badgeText) {
        return ZzCore::ZzResult<void>::success();
    }
    nodes[row].badgeText = std::move(badgeText);
    const QModelIndex changedIndex = q_ptr->index(iterator.value(), 0);
    Q_EMIT q_ptr->dataChanged(
        changedIndex,
        changedIndex,
        {static_cast<int>(ZzNavigationRole::Badge),
         Qt::ToolTipRole,
         Qt::AccessibleDescriptionRole});
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
        translatedSections[row] = zzTranslateSection(nodes.at(row));
    }
    if (!nodes.isEmpty()) {
        Q_EMIT q_ptr->dataChanged(
            q_ptr->index(0, 0),
            q_ptr->index(static_cast<int>(nodes.size() - 1), 0),
            {Qt::DisplayRole,
             Qt::ToolTipRole,
             static_cast<int>(ZzNavigationRole::Section)});
    }
}

} // namespace ZzPureTools
