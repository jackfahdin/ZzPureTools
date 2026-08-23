#include "ZzWorkspaceLayoutStatePrivate.h"

#include <algorithm>

namespace ZzPureTools {
namespace {

using ZzLayoutState = ZzWorkspaceLayoutStatePrivate;

void zzUniqueNonEmpty(QStringList *ids)
{
    QStringList unique;
    unique.reserve(ids->size());
    for (const QString &id : std::as_const(*ids)) {
        if (!id.isEmpty() && !unique.contains(id)) {
            unique.append(id);
        }
    }
    *ids = std::move(unique);
}

void zzNormalizeSide(ZzLayoutState::ZzSideProjection *side)
{
    zzUniqueNonEmpty(&side->order);
    QStringList visible;
    QList<int> sizes;
    visible.reserve(side->visible.size());
    sizes.reserve(side->visible.size());
    for (qsizetype index = 0; index < side->visible.size(); ++index) {
        const QString &id = side->visible.at(index);
        if (id.isEmpty() || !side->order.contains(id) || visible.contains(id)) {
            continue;
        }
        visible.append(id);
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
    if (!side->visible.contains(side->current)) {
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
    bottom->visible.erase(
        std::remove_if(bottom->visible.begin(), bottom->visible.end(),
            [bottom](const QString &id) {
                return !bottom->order.contains(id);
            }),
        bottom->visible.end());
    if (!bottom->order.contains(bottom->current)) {
        bottom->current = bottom->order.contains(snapshotBottom.current)
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
    QStringList *groups)
{
    if (node.leaf) {
        if (!node.groupId.isEmpty() && !groups->contains(node.groupId)) {
            groups->append(node.groupId);
        }
        return;
    }
    for (const ZzLayoutState::ZzSplitNode &child : node.children) {
        zzCollectSplitGroupOrder(child, groups);
    }
}

void zzNormalizeSplit(ZzLayoutState::ZzSplitProjection *split)
{
    zzNormalizeSplitNode(&split->root);
    zzUniqueNonEmpty(&split->groupOrder);
    QStringList treeGroups;
    zzCollectSplitGroupOrder(split->root, &treeGroups);
    split->groupOrder.erase(
        std::remove_if(split->groupOrder.begin(), split->groupOrder.end(),
            [&treeGroups](const QString &groupId) {
                return !treeGroups.contains(groupId);
            }),
        split->groupOrder.end());
    for (const QString &groupId : std::as_const(treeGroups)) {
        if (!split->groupOrder.contains(groupId)) {
            split->groupOrder.append(groupId);
        }
    }
    if (!split->groupOrder.contains(split->activeGroup)) {
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
    for (ZzLayoutState::ZzDockPlacement &dock : target->dock.docks) {
        const auto matchingDock = std::find_if(
            snapshot.dock.docks.cbegin(), snapshot.dock.docks.cend(),
            [&dock](const ZzLayoutState::ZzDockPlacement &candidate) {
                return !dock.panel.id.isEmpty()
                    && candidate.panel.id == dock.panel.id;
            });
        if (matchingDock != snapshot.dock.docks.cend()) {
            zzCopyPanelRuntimeIdentity(&dock.panel, matchingDock->panel);
            dock.actualOwnerIdentity = matchingDock->actualOwnerIdentity;
            continue;
        }
        const auto matchingPanel = std::find_if(
            snapshot.identities.cbegin(), snapshot.identities.cend(),
            [&dock](const ZzLayoutState::ZzPanelIdentity &candidate) {
                return !dock.panel.id.isEmpty()
                    && candidate.id == dock.panel.id;
            });
        if (matchingPanel != snapshot.identities.cend()) {
            zzCopyPanelRuntimeIdentity(&dock.panel, *matchingPanel);
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
    for (const ZzLayoutState::ZzPanelIdentity &identity : snapshot.identities) {
        if (identity.id == panelId) {
            return identity.kind == ZzLayoutState::ZzPanelKind::Side;
        }
    }
    return snapshot.leftSide.order.contains(panelId)
        || snapshot.rightSide.order.contains(panelId);
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
    const auto selectSideCurrent = [](ZzSideProjection *side,
                                       const QString &requestedCurrent,
                                       const QString &snapshotCurrent) {
        if (!requestedCurrent.isEmpty()
            && side->visible.contains(requestedCurrent)) {
            side->current = requestedCurrent;
        } else if (side->visible.contains(snapshotCurrent)) {
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
