#include "ZzWorkspaceLayoutStatePrivate.h"

#include <algorithm>

#include <QtCore/QHash>
#include <QtCore/QSet>

namespace ZzPureTools {
namespace {

using ZzLayoutState = ZzWorkspaceLayoutStatePrivate;

struct ZzSnapshotIndex final
{
    QHash<QString, const ZzLayoutState::ZzPanelIdentity *> identitiesById;
    QSet<QString> registeredSideIds;
    QHash<QString, ZzFluentUI::ZzActivityArea> activityAreas;
};

struct ZzSideIndex final
{
    QSet<QString> orderIds;
    QSet<QString> visibleIds;
    QHash<QString, int> sizesById;
};

/** @brief 单次建立快照身份、侧栏注册和 Activity 区域索引。 */
[[nodiscard]] ZzSnapshotIndex zzBuildSnapshotIndex(
    const ZzLayoutState::ZzWorkspaceSnapshot &snapshot)
{
    ZzSnapshotIndex index;
    index.identitiesById.reserve(snapshot.identities.size());
    index.registeredSideIds.reserve(
        snapshot.identities.size() + snapshot.leftSide.order.size()
        + snapshot.rightSide.order.size());
    for (const ZzLayoutState::ZzPanelIdentity &identity : snapshot.identities) {
        if (identity.id.isEmpty() || index.identitiesById.contains(identity.id)) {
            continue;
        }
        index.identitiesById.insert(identity.id, &identity);
        if (identity.kind == ZzLayoutState::ZzPanelKind::Side) {
            index.registeredSideIds.insert(identity.id);
        }
    }
    for (const QStringList *order : {
             &snapshot.leftSide.order, &snapshot.rightSide.order}) {
        for (const QString &id : *order) {
            if (!id.isEmpty() && !index.identitiesById.contains(id)) {
                index.registeredSideIds.insert(id);
            }
        }
    }
    const auto addActivityArea = [&index](const QStringList &rows,
                                       ZzFluentUI::ZzActivityArea area) {
        for (const QString &id : rows) {
            if (!id.isEmpty() && !index.activityAreas.contains(id)) {
                index.activityAreas.insert(id, area);
            }
        }
    };
    addActivityArea(snapshot.activity.leftPrimary,
        ZzFluentUI::ZzActivityArea::LeftPrimary);
    addActivityArea(snapshot.activity.leftSecondary,
        ZzFluentUI::ZzActivityArea::LeftSecondary);
    addActivityArea(snapshot.activity.rightPrimary,
        ZzFluentUI::ZzActivityArea::RightPrimary);
    addActivityArea(snapshot.activity.rightSecondary,
        ZzFluentUI::ZzActivityArea::RightSecondary);
    return index;
}

/** @brief 单次建立侧栏顺序、可见性和尺寸的按 ID 查询索引。 */
[[nodiscard]] ZzSideIndex zzBuildSideIndex(
    const ZzLayoutState::ZzSideProjection &side)
{
    ZzSideIndex index;
    index.orderIds.reserve(side.order.size());
    index.visibleIds.reserve(side.visible.size());
    index.sizesById.reserve(side.visible.size());
    for (const QString &id : side.order) {
        if (!id.isEmpty()) {
            index.orderIds.insert(id);
        }
    }
    for (qsizetype position = 0; position < side.visible.size(); ++position) {
        const QString &id = side.visible.at(position);
        if (id.isEmpty() || index.visibleIds.contains(id)) {
            continue;
        }
        index.visibleIds.insert(id);
        const int size = position < side.sizes.size()
            ? side.sizes.at(position) : 1;
        index.sizesById.insert(id, std::max(size, 1));
    }
    return index;
}

void zzUniqueNonEmpty(QStringList *ids)
{
    QStringList unique;
    QSet<QString> seen;
    unique.reserve(ids->size());
    seen.reserve(ids->size());
    for (const QString &id : std::as_const(*ids)) {
        if (!id.isEmpty() && !seen.contains(id)) {
            unique.append(id);
            seen.insert(id);
        }
    }
    *ids = std::move(unique);
}

void zzFilterRequestedSideOrder(
    QStringList *order,
    const QSet<QString> &registeredIds,
    QSet<QString> *claimedIds)
{
    QStringList filtered;
    filtered.reserve(order->size());
    for (const QString &id : std::as_const(*order)) {
        if (registeredIds.contains(id) && !claimedIds->contains(id)) {
            filtered.append(id);
            claimedIds->insert(id);
        }
    }
    *order = std::move(filtered);
}

void zzFilterRequestedSideVisibility(
    ZzLayoutState::ZzSideProjection *side)
{
    QStringList visible;
    QList<int> sizes;
    const ZzSideIndex sideIndex = zzBuildSideIndex(*side);
    QSet<QString> seen;
    visible.reserve(side->visible.size());
    sizes.reserve(side->visible.size());
    for (qsizetype index = 0; index < side->visible.size(); ++index) {
        const QString &id = side->visible.at(index);
        if (!sideIndex.orderIds.contains(id) || seen.contains(id)) {
            continue;
        }
        visible.append(id);
        seen.insert(id);
        sizes.append(index < side->sizes.size()
                ? std::max(side->sizes.at(index), 1) : 1);
    }
    side->visible = std::move(visible);
    side->sizes = std::move(sizes);
}

struct ZzStableMergeResult final
{
    QStringList order;
    QList<int> sizes;
};

[[nodiscard]] ZzStableMergeResult zzMergeOmittedBySnapshotAnchors(
    const QStringList &requestedOrder,
    const QList<int> &requestedSizes,
    const QStringList &snapshotOrder,
    const QList<int> &snapshotSizes,
    const QSet<QString> &omittedIds)
{
    const qsizetype targetCount = requestedOrder.size();
    const qsizetype noTarget = targetCount;
    QHash<QString, qsizetype> targetPositions;
    targetPositions.reserve(targetCount);
    for (qsizetype position = 0; position < targetCount; ++position) {
        targetPositions.insert(requestedOrder.at(position), position);
    }
    QVector<qsizetype> successorPositions(snapshotOrder.size(), noTarget);
    qsizetype nextTarget = noTarget;
    for (qsizetype position = snapshotOrder.size(); position > 0; --position) {
        const QString &id = snapshotOrder.at(position - 1);
        successorPositions[position - 1] = nextTarget;
        const auto targetPosition = targetPositions.constFind(id);
        if (targetPosition != targetPositions.cend()) {
            nextTarget = targetPosition.value();
        }
    }
    QVector<QStringList> before(targetCount);
    QVector<QList<int>> beforeSizes(targetCount);
    QVector<QStringList> after(targetCount);
    QVector<QList<int>> afterSizes(targetCount);
    QStringList tail;
    QList<int> tailSizes;
    qsizetype previousTarget = noTarget;
    for (qsizetype position = 0; position < snapshotOrder.size(); ++position) {
        const QString &id = snapshotOrder.at(position);
        const auto targetPosition = targetPositions.constFind(id);
        if (targetPosition != targetPositions.cend()) {
            previousTarget = targetPosition.value();
            continue;
        }
        if (!omittedIds.contains(id)) {
            continue;
        }
        const int size = position < snapshotSizes.size()
            ? std::max(snapshotSizes.at(position), 1) : 1;
        if (successorPositions.at(position) != noTarget) {
            const qsizetype anchor = successorPositions.at(position);
            before[anchor].append(id);
            beforeSizes[anchor].append(size);
        } else if (previousTarget != noTarget) {
            after[previousTarget].append(id);
            afterSizes[previousTarget].append(size);
        } else {
            tail.append(id);
            tailSizes.append(size);
        }
    }
    ZzStableMergeResult result;
    result.order.reserve(targetCount + omittedIds.size());
    result.sizes.reserve(targetCount + omittedIds.size());
    for (qsizetype position = 0; position < targetCount; ++position) {
        result.order.append(before.at(position));
        result.sizes.append(beforeSizes.at(position));
        result.order.append(requestedOrder.at(position));
        const int size = position < requestedSizes.size()
            ? std::max(requestedSizes.at(position), 1) : 1;
        result.sizes.append(size);
        result.order.append(after.at(position));
        result.sizes.append(afterSizes.at(position));
    }
    result.order.append(tail);
    result.sizes.append(tailSizes);
    return result;
}

void zzFilterActivityRows(
    QStringList *rows,
    const QSet<QString> &sideOrder,
    QSet<QString> *claimedIds)
{
    QStringList filtered;
    filtered.reserve(rows->size());
    for (const QString &id : std::as_const(*rows)) {
        if (sideOrder.contains(id) && !claimedIds->contains(id)) {
            filtered.append(id);
            claimedIds->insert(id);
        }
    }
    *rows = std::move(filtered);
}

void zzReconcileSerializedSideProjection(
    ZzLayoutState::ZzWorkspaceProjection *target,
    const ZzLayoutState::ZzWorkspaceSnapshot &snapshot)
{
    const ZzSnapshotIndex snapshotIndex = zzBuildSnapshotIndex(snapshot);
    QSet<QString> claimedIds;
    zzFilterRequestedSideOrder(
        &target->leftSide.order, snapshotIndex.registeredSideIds, &claimedIds);
    zzFilterRequestedSideOrder(
        &target->rightSide.order, snapshotIndex.registeredSideIds, &claimedIds);
    zzFilterRequestedSideVisibility(&target->leftSide);
    zzFilterRequestedSideVisibility(&target->rightSide);

    const auto mergeSide = [&claimedIds, &snapshotIndex](
                               ZzLayoutState::ZzSideProjection *targetSide,
                               const ZzLayoutState::ZzSideProjection &snapshotSide) {
        QSet<QString> omittedIds;
        omittedIds.reserve(snapshotSide.order.size());
        for (const QString &id : snapshotSide.order) {
            if (snapshotIndex.registeredSideIds.contains(id)
                && !claimedIds.contains(id)) {
                omittedIds.insert(id);
                claimedIds.insert(id);
            }
        }
        const ZzStableMergeResult order = zzMergeOmittedBySnapshotAnchors(
            targetSide->order, {}, snapshotSide.order, {}, omittedIds);
        targetSide->order = order.order;
        const ZzStableMergeResult visible = zzMergeOmittedBySnapshotAnchors(
            targetSide->visible, targetSide->sizes, snapshotSide.visible,
            snapshotSide.sizes, omittedIds);
        targetSide->visible = visible.order;
        targetSide->sizes = visible.sizes;
        return omittedIds;
    };
    const QSet<QString> leftOmitted = mergeSide(
        &target->leftSide, snapshot.leftSide);
    const QSet<QString> rightOmitted = mergeSide(
        &target->rightSide, snapshot.rightSide);

    QSet<QString> claimedActivityIds;
    const ZzSideIndex leftSideIndex = zzBuildSideIndex(target->leftSide);
    const ZzSideIndex rightSideIndex = zzBuildSideIndex(target->rightSide);
    zzFilterActivityRows(&target->activity.leftPrimary,
        leftSideIndex.orderIds, &claimedActivityIds);
    zzFilterActivityRows(&target->activity.leftSecondary,
        leftSideIndex.orderIds, &claimedActivityIds);
    zzFilterActivityRows(&target->activity.rightPrimary,
        rightSideIndex.orderIds, &claimedActivityIds);
    zzFilterActivityRows(&target->activity.rightSecondary,
        rightSideIndex.orderIds, &claimedActivityIds);
    const auto mergeActivity = [](QStringList *targetRows,
                                   const QStringList &snapshotRows,
                                   const QSet<QString> &omittedIds) {
        const ZzStableMergeResult rows = zzMergeOmittedBySnapshotAnchors(
            *targetRows, {}, snapshotRows, {}, omittedIds);
        *targetRows = rows.order;
    };
    mergeActivity(&target->activity.leftPrimary,
        snapshot.activity.leftPrimary, leftOmitted);
    mergeActivity(&target->activity.leftSecondary,
        snapshot.activity.leftSecondary, leftOmitted);
    mergeActivity(&target->activity.rightPrimary,
        snapshot.activity.rightPrimary, rightOmitted);
    mergeActivity(&target->activity.rightSecondary,
        snapshot.activity.rightSecondary, rightOmitted);
}

void zzNormalizeSide(ZzLayoutState::ZzSideProjection *side)
{
    zzUniqueNonEmpty(&side->order);
    const ZzSideIndex sideIndex = zzBuildSideIndex(*side);
    QStringList visible;
    QList<int> sizes;
    QSet<QString> seen;
    visible.reserve(side->visible.size());
    sizes.reserve(side->visible.size());
    for (qsizetype index = 0; index < side->visible.size(); ++index) {
        const QString &id = side->visible.at(index);
        if (!sideIndex.orderIds.contains(id) || seen.contains(id)) {
            continue;
        }
        visible.append(id);
        seen.insert(id);
        const int size = index < side->sizes.size()
            ? side->sizes.at(index) : 1;
        sizes.append(std::max(size, 1));
    }
    side->visible = std::move(visible);
    side->sizes = std::move(sizes);
    if (side->visible.isEmpty()) {
        side->collapsed = true;
        side->current.clear();
        return;
    }
    if (!seen.contains(side->current)) {
        side->current = side->visible.front();
    }
}

void zzAlignContents(
    const QStringList &order,
    const ZzLayoutState::ZzSubsystemIdentity &paneIdentity,
    const ZzLayoutState::ZzSubsystemIdentity &stackIdentity,
    QList<ZzLayoutState::ZzContentPlacement> *contents)
{
    contents->clear();
    contents->reserve(order.size());
    for (const QString &panelId : order) {
        contents->append({panelId, stackIdentity,
            {paneIdentity, stackIdentity}});
    }
}

void zzNormalizeBottom(
    ZzLayoutState::ZzBottomProjection *bottom,
    const ZzLayoutState::ZzBottomProjection &snapshotBottom)
{
    zzUniqueNonEmpty(&bottom->order);
    zzUniqueNonEmpty(&bottom->visible);
    QSet<QString> orderIds;
    orderIds.reserve(bottom->order.size());
    for (const QString &id : bottom->order) {
        orderIds.insert(id);
    }
    QStringList visible;
    visible.reserve(bottom->visible.size());
    for (const QString &id : std::as_const(bottom->visible)) {
        if (orderIds.contains(id)) {
            visible.append(id);
        }
    }
    bottom->visible = std::move(visible);
    if (!orderIds.contains(bottom->current)) {
        bottom->current = orderIds.contains(snapshotBottom.current)
            ? snapshotBottom.current
            : bottom->order.value(0);
    }
    if (bottom->order.isEmpty()) {
        bottom->current.clear();
        bottom->collapsed = true;
    }
    zzAlignContents(bottom->order, bottom->paneIdentity,
        bottom->stackIdentity, &bottom->contents);
}

void zzNormalizeSplitNode(ZzLayoutState::ZzSplitNode *node)
{
    if (node->leaf) {
        node->children.clear();
        node->sizes.clear();
        return;
    }
    for (ZzLayoutState::ZzSplitNode &child : node->children) {
        zzNormalizeSplitNode(&child);
    }
    while (node->sizes.size() < node->children.size()) {
        node->sizes.append(1);
    }
    node->sizes.resize(node->children.size());
    for (int &size : node->sizes) {
        if (size <= 0) {
            size = 1;
        }
    }
}

void zzCollectSplitGroupOrder(
    const ZzLayoutState::ZzSplitNode &node,
    QStringList *groups,
    QSet<QString> *seen)
{
    if (node.leaf) {
        if (!node.groupId.isEmpty() && !seen->contains(node.groupId)) {
            groups->append(node.groupId);
            seen->insert(node.groupId);
        }
        return;
    }
    for (const ZzLayoutState::ZzSplitNode &child : node.children) {
        zzCollectSplitGroupOrder(child, groups, seen);
    }
}

void zzNormalizeSplit(ZzLayoutState::ZzSplitProjection *split)
{
    zzNormalizeSplitNode(&split->root);
    zzUniqueNonEmpty(&split->groupOrder);
    QStringList treeGroups;
    QSet<QString> treeGroupIds;
    zzCollectSplitGroupOrder(split->root, &treeGroups, &treeGroupIds);
    QStringList requestedGroups;
    requestedGroups.reserve(split->groupOrder.size());
    QSet<QString> requestedGroupIds;
    requestedGroupIds.reserve(split->groupOrder.size());
    for (const QString &groupId : std::as_const(split->groupOrder)) {
        if (treeGroupIds.contains(groupId)
            && !requestedGroupIds.contains(groupId)) {
            requestedGroups.append(groupId);
            requestedGroupIds.insert(groupId);
        }
    }
    split->groupOrder = std::move(requestedGroups);
    for (const QString &groupId : std::as_const(treeGroups)) {
        if (!requestedGroupIds.contains(groupId)) {
            split->groupOrder.append(groupId);
            requestedGroupIds.insert(groupId);
        }
    }
    if (!requestedGroupIds.contains(split->activeGroup)) {
        split->activeGroup = split->groupOrder.value(0);
    }
}

void zzDeriveActivityState(ZzLayoutState::ZzWorkspaceProjection *target)
{
    target->activity.leftCurrent = target->leftSide.current;
    target->activity.rightCurrent = target->rightSide.current;
    target->activity.leftActive.clear();
    target->activity.rightActive.clear();
    for (const QString &id : std::as_const(target->leftSide.visible)) {
        target->activity.leftActive.insert(id);
    }
    for (const QString &id : std::as_const(target->rightSide.visible)) {
        target->activity.rightActive.insert(id);
    }
}

void zzNormalizeTarget(
    ZzLayoutState::ZzWorkspaceProjection *target,
    const ZzLayoutState::ZzWorkspaceSnapshot &snapshot)
{
    zzNormalizeSide(&target->leftSide);
    zzNormalizeSide(&target->rightSide);
    zzAlignContents(target->leftSide.order, target->leftSide.paneIdentity,
        target->leftSide.stackIdentity, &target->leftSide.contents);
    zzAlignContents(target->rightSide.order, target->rightSide.paneIdentity,
        target->rightSide.stackIdentity, &target->rightSide.contents);
    zzNormalizeBottom(&target->bottom, snapshot.bottom);
    zzNormalizeSplit(&target->split);
    zzUniqueNonEmpty(&target->activity.leftPrimary);
    zzUniqueNonEmpty(&target->activity.leftSecondary);
    zzUniqueNonEmpty(&target->activity.rightPrimary);
    zzUniqueNonEmpty(&target->activity.rightSecondary);
    zzDeriveActivityState(target);
}

void zzCopyPanelRuntimeIdentity(
    ZzLayoutState::ZzPanelIdentity *target,
    const ZzLayoutState::ZzPanelIdentity &source)
{
    target->widget = source.widget;
    target->rawWidget = source.rawWidget;
    target->registrationGeneration = source.registrationGeneration;
    target->dock = source.dock;
    target->rawDock = source.rawDock;
}

void zzRestoreRuntimeIdentities(
    ZzLayoutState::ZzWorkspaceProjection *target,
    const ZzLayoutState::ZzWorkspaceSnapshot &snapshot)
{
    target->identities = snapshot.identities;
    target->leftSide.paneIdentity = snapshot.leftSide.paneIdentity;
    target->leftSide.stackIdentity = snapshot.leftSide.stackIdentity;
    target->leftSide.contents = snapshot.leftSide.contents;
    target->rightSide.paneIdentity = snapshot.rightSide.paneIdentity;
    target->rightSide.stackIdentity = snapshot.rightSide.stackIdentity;
    target->rightSide.contents = snapshot.rightSide.contents;
    target->bottom.paneIdentity = snapshot.bottom.paneIdentity;
    target->bottom.stackIdentity = snapshot.bottom.stackIdentity;
    target->bottom.contents = snapshot.bottom.contents;
    target->activity.modelIdentity = snapshot.activity.modelIdentity;

    if (target->dock.docks.isEmpty()) {
        target->dock.docks = snapshot.dock.docks;
        return;
    }
    QHash<QString, const ZzLayoutState::ZzDockPlacement *> docksById;
    docksById.reserve(snapshot.dock.docks.size());
    for (const ZzLayoutState::ZzDockPlacement &dock : snapshot.dock.docks) {
        if (!dock.panel.id.isEmpty() && !docksById.contains(dock.panel.id)) {
            docksById.insert(dock.panel.id, &dock);
        }
    }
    const ZzSnapshotIndex snapshotIndex = zzBuildSnapshotIndex(snapshot);
    for (ZzLayoutState::ZzDockPlacement &dock : target->dock.docks) {
        const auto matchingDock = docksById.constFind(dock.panel.id);
        if (matchingDock != docksById.cend()) {
            zzCopyPanelRuntimeIdentity(&dock.panel, matchingDock.value()->panel);
            dock.actualOwnerIdentity = matchingDock.value()->actualOwnerIdentity;
            continue;
        }
        const auto matchingPanel = snapshotIndex.identitiesById.constFind(
            dock.panel.id);
        if (matchingPanel != snapshotIndex.identitiesById.cend()) {
            zzCopyPanelRuntimeIdentity(&dock.panel, *matchingPanel.value());
        }
    }
}

bool zzIsLeftArea(ZzFluentUI::ZzActivityArea area) noexcept
{
    return area == ZzFluentUI::ZzActivityArea::LeftPrimary
        || area == ZzFluentUI::ZzActivityArea::LeftSecondary;
}

QStringList *zzActivityList(
    ZzLayoutState::ZzActivityProjection *activity,
    ZzFluentUI::ZzActivityArea area) noexcept
{
    switch (area) {
    case ZzFluentUI::ZzActivityArea::LeftPrimary:
        return &activity->leftPrimary;
    case ZzFluentUI::ZzActivityArea::LeftSecondary:
        return &activity->leftSecondary;
    case ZzFluentUI::ZzActivityArea::RightPrimary:
        return &activity->rightPrimary;
    case ZzFluentUI::ZzActivityArea::RightSecondary:
        return &activity->rightSecondary;
    }
    return nullptr;
}

void zzRemoveId(QStringList *ids, const QString &id)
{
    ids->removeAll(id);
}

int zzVisiblePanelSize(
    const ZzLayoutState::ZzWorkspaceProjection &target,
    const QString &panelId)
{
    for (const ZzLayoutState::ZzSideProjection *side : {
             &target.leftSide, &target.rightSide}) {
        const qsizetype index = side->visible.indexOf(panelId);
        if (index >= 0 && index < side->sizes.size()) {
            return side->sizes.at(index);
        }
    }
    return 1;
}

bool zzIsKnownSidePanel(
    const ZzLayoutState::ZzWorkspaceSnapshot &snapshot,
    const QString &panelId)
{
    if (panelId.isEmpty()) {
        return false;
    }
    return zzBuildSnapshotIndex(snapshot).registeredSideIds.contains(panelId);
}

void zzRemoveFromSides(
    ZzLayoutState::ZzWorkspaceProjection *target,
    const QString &panelId)
{
    for (ZzLayoutState::ZzSideProjection *side : {
             &target->leftSide, &target->rightSide}) {
        zzRemoveId(&side->order, panelId);
        qsizetype visibleIndex = side->visible.indexOf(panelId);
        while (visibleIndex >= 0) {
            side->visible.removeAt(visibleIndex);
            if (visibleIndex < side->sizes.size()) {
                side->sizes.removeAt(visibleIndex);
            }
            visibleIndex = side->visible.indexOf(panelId);
        }
        if (side->current == panelId) {
            side->current.clear();
        }
    }
}

void zzRemoveFromActivity(
    ZzLayoutState::ZzActivityProjection *activity,
    const QString &panelId)
{
    for (QStringList *ids : {
             &activity->leftPrimary, &activity->leftSecondary,
             &activity->rightPrimary, &activity->rightSecondary}) {
        zzRemoveId(ids, panelId);
    }
}

} // namespace

std::optional<ZzWorkspaceLayoutStatePrivate::ZzWorkspaceProjection>
ZzWorkspaceLayoutStatePrivate::buildRestoreTarget(
    const ZzWorkspaceSnapshot &snapshot,
    const ZzLayoutRequest &request)
{
    ZzWorkspaceProjection target = request.projection.value_or(
        static_cast<const ZzWorkspaceProjection &>(snapshot));
    if (request.projection.has_value()) {
        zzReconcileSerializedSideProjection(&target, snapshot);
    }
    const auto selectSideCurrent = [](ZzSideProjection *side,
                                      const QString &requestedCurrent,
                                      const QString &snapshotCurrent) {
        QSet<QString> visibleIds;
        visibleIds.reserve(side->visible.size());
        for (const QString &id : side->visible) {
            visibleIds.insert(id);
        }
        if (!requestedCurrent.isEmpty()
            && visibleIds.contains(requestedCurrent)) {
            side->current = requestedCurrent;
        } else if (visibleIds.contains(snapshotCurrent)) {
            side->current = snapshotCurrent;
        } else {
            side->current.clear();
        }
    };
    selectSideCurrent(
        &target.leftSide, request.leftCurrent, snapshot.leftSide.current);
    selectSideCurrent(
        &target.rightSide, request.rightCurrent, snapshot.rightSide.current);
    zzRestoreRuntimeIdentities(&target, snapshot);
    zzNormalizeTarget(&target, snapshot);
    return target;
}

std::optional<ZzWorkspaceLayoutStatePrivate::ZzWorkspaceProjection>
ZzWorkspaceLayoutStatePrivate::buildActivityMoveTarget(
    const ZzWorkspaceSnapshot &snapshot,
    const QString &panelId,
    ZzFluentUI::ZzActivityArea targetArea,
    int targetRow)
{
    if (targetRow < 0 || !zzIsKnownSidePanel(snapshot, panelId)) {
        return std::nullopt;
    }
    const auto restored = buildRestoreTarget(snapshot, {});
    if (!restored.has_value()) {
        return std::nullopt;
    }
    ZzWorkspaceProjection target = *restored;
    const int panelSize = zzVisiblePanelSize(target, panelId);
    zzRemoveFromSides(&target, panelId);
    zzRemoveFromActivity(&target.activity, panelId);

    ZzSideProjection &side = zzIsLeftArea(targetArea)
        ? target.leftSide : target.rightSide;
    const qsizetype insertionIndex = std::min<qsizetype>(
        targetRow, side.order.size());
    side.order.insert(insertionIndex, panelId);
    const qsizetype visibleIndex = std::min<qsizetype>(
        targetRow, side.visible.size());
    side.visible.insert(visibleIndex, panelId);
    side.sizes.insert(visibleIndex, panelSize);
    side.current = panelId;

    QStringList *const activity = zzActivityList(&target.activity, targetArea);
    if (activity == nullptr) {
        return std::nullopt;
    }
    activity->insert(std::min<qsizetype>(targetRow, activity->size()), panelId);
    zzNormalizeTarget(&target, snapshot);
    return target;
}

bool ZzWorkspaceLayoutStatePrivate::equals(
    const ZzWorkspaceProjection &left,
    const ZzWorkspaceProjection &right) noexcept
{
    return left == right;
}

} // namespace ZzPureTools
