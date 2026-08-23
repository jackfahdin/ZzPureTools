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
    zzUniqueNonEmpty(&side->visible);
    side->visible.erase(std::remove_if(side->visible.begin(), side->visible.end(),
        [side](const QString &id) { return !side->order.contains(id); }),
        side->visible.end());
    if (side->visible.isEmpty()) {
        side->collapsed = true;
        side->current.clear();
        return;
    }
    if (!side->visible.contains(side->current)) {
        side->current = side->visible.front();
    }
}

void zzNormalizeSplit(ZzLayoutState::ZzSplitProjection *split)
{
    zzUniqueNonEmpty(&split->visible);
    while (split->sizes.size() < split->visible.size()) {
        split->sizes.append(1);
    }
    split->sizes.resize(split->visible.size());
    for (int &size : split->sizes) {
        if (size <= 0) {
            size = 1;
        }
    }
    if (split->currentIndex < 0
        || split->currentIndex >= split->visible.size()) {
        split->currentIndex = split->visible.isEmpty() ? -1 : 0;
    }
}

void zzDeriveActivityCurrent(ZzLayoutState::ZzWorkspaceProjection *target)
{
    target->activity.leftCurrent = target->leftSide.current;
    target->activity.rightCurrent = target->rightSide.current;
}

void zzNormalizeTarget(ZzLayoutState::ZzWorkspaceProjection *target)
{
    zzNormalizeSide(&target->leftSide);
    zzNormalizeSide(&target->rightSide);
    zzNormalizeSplit(&target->split);
    zzUniqueNonEmpty(&target->activity.leftPrimary);
    zzUniqueNonEmpty(&target->activity.leftSecondary);
    zzUniqueNonEmpty(&target->activity.rightPrimary);
    zzUniqueNonEmpty(&target->activity.rightSecondary);
    zzDeriveActivityCurrent(target);
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
        zzRemoveId(&side->visible, panelId);
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
    zzNormalizeTarget(&target);
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
    zzRemoveFromSides(&target, panelId);
    zzRemoveFromActivity(&target.activity, panelId);

    ZzSideProjection &side = zzIsLeftArea(targetArea)
        ? target.leftSide : target.rightSide;
    const qsizetype insertionIndex = std::min<qsizetype>(
        targetRow, side.order.size());
    side.order.insert(insertionIndex, panelId);
    side.visible.insert(std::min<qsizetype>(targetRow, side.visible.size()),
        panelId);
    side.current = panelId;

    QStringList *const activity = zzActivityList(&target.activity, targetArea);
    if (activity == nullptr) {
        return std::nullopt;
    }
    activity->insert(std::min<qsizetype>(targetRow, activity->size()), panelId);
    zzNormalizeTarget(&target);
    return target;
}

bool ZzWorkspaceLayoutStatePrivate::equals(
    const ZzWorkspaceProjection &left,
    const ZzWorkspaceProjection &right) noexcept
{
    return left == right;
}

} // namespace ZzPureTools
