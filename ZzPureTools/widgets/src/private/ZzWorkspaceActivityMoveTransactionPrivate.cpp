#include "ZzWorkspaceActivityMoveTransactionPrivate.h"

#include <algorithm>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzSidePane.h>

#include "ZzWorkspaceShellPrivate.h"

namespace ZzPureTools {
namespace {

using ZzLayoutState = ZzWorkspaceLayoutStatePrivate;
using ZzProjection = ZzLayoutState::ZzWorkspaceProjection;
using ZzSnapshot = ZzLayoutState::ZzWorkspaceSnapshot;
using ZzSide = ZzLayoutState::ZzSideProjection;

[[nodiscard]] const QStringList *zzActivityRows(
    const ZzLayoutState::ZzActivityProjection &activity,
    ZzFluentUI::ZzActivityArea area) noexcept
{
    switch (area) {
    case ZzFluentUI::ZzActivityArea::LeftPrimary:
        return &activity.leftPrimary;
    case ZzFluentUI::ZzActivityArea::LeftSecondary:
        return &activity.leftSecondary;
    case ZzFluentUI::ZzActivityArea::RightPrimary:
        return &activity.rightPrimary;
    case ZzFluentUI::ZzActivityArea::RightSecondary:
        return &activity.rightSecondary;
    }
    return nullptr;
}

[[nodiscard]] bool zzAreaForId(
    const ZzLayoutState::ZzActivityProjection &activity,
    const QString &id,
    ZzFluentUI::ZzActivityArea *area) noexcept
{
    for (const auto candidate : {
             ZzFluentUI::ZzActivityArea::LeftPrimary,
             ZzFluentUI::ZzActivityArea::LeftSecondary,
             ZzFluentUI::ZzActivityArea::RightPrimary,
             ZzFluentUI::ZzActivityArea::RightSecondary}) {
        if (zzActivityRows(activity, candidate)->contains(id)) {
            *area = candidate;
            return true;
        }
    }
    return false;
}

[[nodiscard]] ZzWorkspaceShellPrivate::ZzPanelRecord *zzRecord(
    ZzWorkspaceShellPrivate &shell,
    const QString &id)
{
    const int index = shell.indexOf(ZzWorkspacePanelId(id));
    return index >= 0 ? &shell.panels[index] : nullptr;
}

[[nodiscard]] const ZzWorkspaceShellPrivate::ZzPanelRecord *zzRecord(
    const ZzWorkspaceShellPrivate &shell,
    const QString &id)
{
    const int index = shell.indexOf(ZzWorkspacePanelId(id));
    return index >= 0 ? &shell.panels.at(index) : nullptr;
}

[[nodiscard]] QString zzIdForWidget(
    const ZzWorkspaceShellPrivate &shell,
    QWidget *widget)
{
    for (const auto &record : shell.panels) {
        if (record.kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
            && record.content == widget) {
            return record.id.value();
        }
    }
    return {};
}

[[nodiscard]] ZzLayoutState::ZzSubsystemIdentity zzIdentity(
    QObject *object)
{
    return {object, object};
}

void zzCaptureSide(
    const ZzWorkspaceShellPrivate &shell,
    ZzFluentUI::ZzSidePane *pane,
    ZzSide *side)
{
    if (pane == nullptr || pane->panelStack() == nullptr) {
        return;
    }
    ZzFluentUI::ZzPanelStack *const stack = pane->panelStack();
    side->paneIdentity = zzIdentity(pane);
    side->stackIdentity = zzIdentity(stack);
    for (QWidget *const content : stack->panels()) {
        const QString id = zzIdForWidget(shell, content);
        if (!id.isEmpty()) {
            side->order.append(id);
            side->contents.append(
                {id, side->stackIdentity,
                    {side->paneIdentity, side->stackIdentity}});
        }
    }
    for (QWidget *const content : stack->visiblePanels()) {
        const QString id = zzIdForWidget(shell, content);
        if (!id.isEmpty()) {
            side->visible.append(id);
        }
    }
    side->sizes = stack->panelSizes();
    side->current = zzIdForWidget(shell, pane->currentWidget());
    side->collapsed = pane->isCollapsed();
    side->width = pane->paneWidth();
}

[[nodiscard]] ZzSnapshot zzCaptureSnapshot(
    const ZzWorkspaceShellPrivate &shell)
{
    ZzSnapshot snapshot;
    zzCaptureSide(shell, shell.leftSidePane, &snapshot.leftSide);
    zzCaptureSide(shell, shell.rightSidePane, &snapshot.rightSide);
    snapshot.activity.modelIdentity = zzIdentity(shell.activityModel);
    for (const auto &record : shell.panels) {
        if (record.kind != ZzWorkspaceShellPrivate::ZzPanelKind::Side) {
            continue;
        }
        snapshot.identities.append({
            record.id.value(), ZzLayoutState::ZzPanelKind::Side,
            record.content, record.contentIdentity,
            record.registrationGeneration, {}, nullptr});
    }
    for (const auto &row : shell.activityRows()) {
        QStringList *rows = nullptr;
        switch (row.area) {
        case ZzFluentUI::ZzActivityArea::LeftPrimary:
            rows = &snapshot.activity.leftPrimary;
            break;
        case ZzFluentUI::ZzActivityArea::LeftSecondary:
            rows = &snapshot.activity.leftSecondary;
            break;
        case ZzFluentUI::ZzActivityArea::RightPrimary:
            rows = &snapshot.activity.rightPrimary;
            break;
        case ZzFluentUI::ZzActivityArea::RightSecondary:
            rows = &snapshot.activity.rightSecondary;
            break;
        }
        rows->append(row.id.value());
    }
    snapshot.activity.leftCurrent = snapshot.leftSide.current;
    snapshot.activity.rightCurrent = snapshot.rightSide.current;
    snapshot.activity.leftActive = QSet<QString>(
        snapshot.leftSide.visible.cbegin(), snapshot.leftSide.visible.cend());
    snapshot.activity.rightActive = QSet<QString>(
        snapshot.rightSide.visible.cbegin(), snapshot.rightSide.visible.cend());
    return snapshot;
}

[[nodiscard]] QStringList zzModelOrder(
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> &rows)
{
    QStringList result;
    result.reserve(rows.size());
    for (const auto &row : rows) {
        result.append(row.id.value());
    }
    return result;
}

[[nodiscard]] QStringList zzBuildTargetModelOrder(
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> &snapshotRows,
    const ZzProjection &target,
    const QString &movedId,
    ZzFluentUI::ZzActivityArea targetArea)
{
    QStringList order = zzModelOrder(snapshotRows);
    const QStringList desired = *zzActivityRows(target.activity, targetArea);
    QStringList currentArea;
    for (const QString &id : std::as_const(order)) {
        ZzFluentUI::ZzActivityArea area = targetArea;
        if (zzAreaForId(target.activity, id, &area) && area == targetArea) {
            currentArea.append(id);
        }
    }
    if (currentArea == desired) {
        return order;
    }

    order.removeAll(movedId);
    const qsizetype desiredIndex = desired.indexOf(movedId);
    qsizetype insertionIndex = order.size();
    for (qsizetype index = desiredIndex + 1; index < desired.size(); ++index) {
        const qsizetype next = order.indexOf(desired.at(index));
        if (next >= 0) {
            insertionIndex = next;
            break;
        }
    }
    if (insertionIndex == order.size()) {
        for (qsizetype index = desiredIndex; index > 0; --index) {
            const qsizetype previous = order.indexOf(desired.at(index - 1));
            if (previous >= 0) {
                insertionIndex = previous + 1;
                break;
            }
        }
    }
    order.insert(insertionIndex, movedId);
    return order;
}

[[nodiscard]] bool zzSameIdentity(
    const ZzLayoutState::ZzSubsystemIdentity &expected,
    QObject *current) noexcept
{
    return expected.object != nullptr
        && expected.object.data() == expected.rawObject
        && current == expected.rawObject;
}

[[nodiscard]] bool zzSamePanel(
    const ZzWorkspaceShellPrivate &shell,
    const ZzLayoutState::ZzPanelIdentity &expected)
{
    const auto *const record = zzRecord(shell, expected.id);
    return record != nullptr
        && record->kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
        && record->registrationGeneration == expected.registrationGeneration
        && record->content != nullptr
        && record->content.data() == expected.rawWidget
        && record->contentIdentity == expected.rawWidget
        && expected.widget != nullptr
        && expected.widget.data() == expected.rawWidget;
}

[[nodiscard]] bool zzStableRuntime(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    QWidget *transient = nullptr)
{
    if (!zzSameIdentity(projection.activity.modelIdentity, shell.activityModel)
        || !zzSameIdentity(projection.leftSide.paneIdentity, shell.leftSidePane)
        || !zzSameIdentity(projection.rightSide.paneIdentity, shell.rightSidePane)) {
        return false;
    }
    auto *const leftPane = shell.leftSidePane.data();
    auto *const rightPane = shell.rightSidePane.data();
    if (!zzSameIdentity(
            projection.leftSide.stackIdentity, leftPane->panelStack())
        || !zzSameIdentity(
            projection.rightSide.stackIdentity, rightPane->panelStack())) {
        return false;
    }
    ZzFluentUI::ZzPanelStack *const leftStack = leftPane->panelStack();
    ZzFluentUI::ZzPanelStack *const rightStack = rightPane->panelStack();
    for (const auto &identity : projection.identities) {
        if (!zzSamePanel(shell, identity)) {
            return false;
        }
        QWidget *const content = identity.widget.data();
        const bool inLeft = leftStack->panels().contains(content);
        const bool inRight = rightStack->panels().contains(content);
        if (inLeft == inRight) {
            if (content != transient || content->parent() != nullptr) {
                return false;
            }
            continue;
        }
        ZzFluentUI::ZzPanelStack *const stack = inLeft ? leftStack : rightStack;
        ZzFluentUI::ZzSidePane *const pane = inLeft ? leftPane : rightPane;
        if (!stack->isAncestorOf(content) || !pane->isAncestorOf(content)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] QStringList zzIds(
    const ZzWorkspaceShellPrivate &shell,
    const QList<QWidget *> &widgets)
{
    QStringList result;
    result.reserve(widgets.size());
    for (QWidget *const widget : widgets) {
        result.append(zzIdForWidget(shell, widget));
    }
    return result;
}

[[nodiscard]] bool zzSideMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzSide &side,
    bool includeSizes)
{
    if (!zzSameIdentity(side.paneIdentity, side.paneIdentity.rawObject)) {
        return false;
    }
    auto *const pane = qobject_cast<ZzFluentUI::ZzSidePane *>(
        side.paneIdentity.object.data());
    auto *const stack = pane != nullptr ? pane->panelStack() : nullptr;
    if (pane == nullptr || !zzSameIdentity(side.stackIdentity, stack)
        || zzIds(shell, stack->panels()) != side.order
        || zzIds(shell, stack->visiblePanels()) != side.visible
        || zzIdForWidget(shell, pane->currentWidget()) != side.current
        || pane->isCollapsed() != side.collapsed
        || pane->paneWidth() != side.width
        || (includeSizes && stack->panelSizes() != side.sizes)) {
        return false;
    }
    for (const QString &id : side.order) {
        const auto *const record = zzRecord(shell, id);
        if (record == nullptr || record->content == nullptr
            || !stack->panels().contains(record->content)
            || !stack->isAncestorOf(record->content)
            || !pane->isAncestorOf(record->content)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ZzFluentUI::ZzSidePane *zzLivePane(
    ZzWorkspaceShellPrivate &shell,
    const ZzSide &side)
{
    auto *const pane = qobject_cast<ZzFluentUI::ZzSidePane *>(
        side.paneIdentity.object.data());
    if (pane == nullptr || pane != side.paneIdentity.rawObject
        || pane->panelStack() != side.stackIdentity.rawObject) {
        return nullptr;
    }
    if (pane != shell.leftSidePane && pane != shell.rightSidePane) {
        return nullptr;
    }
    return pane;
}

[[nodiscard]] QList<QModelIndex> zzIndexes(
    ZzWorkspaceShellPrivate &shell,
    const QStringList &ids)
{
    QList<QModelIndex> result;
    if (shell.activityModel == nullptr) {
        return result;
    }
    for (const QString &id : ids) {
        for (const auto &row : shell.activityRows()) {
            if (row.id.value() == id) {
                result.append(shell.activityModel->index(row.order, 0));
                break;
            }
        }
    }
    return result;
}

[[nodiscard]] QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry>
zzRowsForProjection(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const QStringList &modelOrder)
{
    QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> rows;
    for (const QString &id : modelOrder) {
        ZzFluentUI::ZzActivityArea area =
            ZzFluentUI::ZzActivityArea::LeftPrimary;
        if (zzRecord(shell, id) != nullptr
            && zzAreaForId(projection.activity, id, &area)) {
            rows.append({
                ZzWorkspacePanelId(id), area,
                static_cast<int>(rows.size())});
        }
    }
    return rows;
}

void zzCleanupInterruptedMove(
    ZzWorkspaceShellPrivate &shell,
    const ZzWorkspaceShellPrivate::ZzPanelRecord &expected)
{
    const int index = shell.stablePanelIndex(expected);
    if (index < 0 || shell.panels.at(index).content == nullptr) {
        return;
    }
    QWidget *const content = shell.panels.at(index).content.data();
    for (const QPointer<ZzFluentUI::ZzSidePane> &paneGuard : {
             shell.leftSidePane, shell.rightSidePane}) {
        if (paneGuard == nullptr || paneGuard->panelStack() == nullptr
            || !paneGuard->panelStack()->panels().contains(content)) {
            continue;
        }
        if (paneGuard->isAncestorOf(content)
            && paneGuard->panelStack()->isAncestorOf(content)) {
            static_cast<void>(paneGuard->takeWidget(content));
        } else {
            static_cast<void>(
                paneGuard->panelStack()->setPanelVisible(content, false));
        }
    }
    shell.cleanupInterruptedPanelRemoval(
        expected.id, expected.contentIdentity,
        expected.registrationGeneration);
}

[[nodiscard]] bool zzMovedRestored(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &snapshot,
    const ZzWorkspaceShellPrivate::ZzPanelRecord &expected)
{
    if (shell.stablePanelIndex(expected) < 0 || shell.activityModel == nullptr
        || expected.content == nullptr) {
        return false;
    }
    const ZzSide &side = snapshot.leftSide.order.contains(expected.id.value())
        ? snapshot.leftSide : snapshot.rightSide;
    auto *const pane = qobject_cast<ZzFluentUI::ZzSidePane *>(
        side.paneIdentity.object.data());
    if (pane == nullptr || pane->panelStack() == nullptr
        || !pane->panelStack()->panels().contains(expected.content)
        || !pane->panelStack()->isAncestorOf(expected.content)
        || !pane->isAncestorOf(expected.content)) {
        return false;
    }
    for (const auto &row : shell.activityRows()) {
        if (row.id == expected.id) {
            ZzFluentUI::ZzActivityArea expectedArea = row.area;
            return zzAreaForId(
                    snapshot.activity, expected.id.value(), &expectedArea)
                && row.area == expectedArea;
        }
    }
    return false;
}

class ZzActivityTransactionScope final
{
public:
    explicit ZzActivityTransactionScope(
        ZzWorkspaceShellPrivate &shell) noexcept
        : shell_(shell)
    {
        shell_.transactionKind =
            ZzWorkspaceShellPrivate::ZzTransactionKind::ActivityMove;
    }

    ~ZzActivityTransactionScope()
    {
        shell_.transactionKind =
            ZzWorkspaceShellPrivate::ZzTransactionKind::None;
    }

private:
    ZzWorkspaceShellPrivate &shell_;
};

} // namespace

ZzWorkspaceActivityMoveTransactionPrivate::
ZzWorkspaceActivityMoveTransactionPrivate(
    ZzWorkspaceShellPrivate &shell) noexcept
    : shell_(shell)
{
}

bool ZzWorkspaceActivityMoveTransactionPrivate::execute(
    const QModelIndex &sourceIndex,
    ZzFluentUI::ZzActivityArea targetArea,
    int targetRow)
{
    if (shell_.transactionKind
            != ZzWorkspaceShellPrivate::ZzTransactionKind::None
        || shell_.activityModel == nullptr
        || !sourceIndex.isValid()
        || sourceIndex.model() != shell_.activityModel
        || sourceIndex.row() < 0) {
        return false;
    }
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> snapshotRows =
        shell_.activityRows();
    if (sourceIndex.row() >= snapshotRows.size()) {
        return false;
    }
    const ZzWorkspaceShellPrivate::ZzPanelRecord *const sourceRecord =
        zzRecord(shell_, snapshotRows.at(sourceIndex.row()).id.value());
    if (sourceRecord == nullptr
        || sourceRecord->kind != ZzWorkspaceShellPrivate::ZzPanelKind::Side) {
        return false;
    }
    const ZzWorkspaceShellPrivate::ZzPanelRecord expected = *sourceRecord;
    const ZzSnapshot snapshot = zzCaptureSnapshot(shell_);
    const auto planned = ZzLayoutState::buildActivityMoveTarget(
        snapshot, expected.id.value(), targetArea, targetRow);
    if (!planned.has_value()) {
        return false;
    }
    const ZzProjection target = *planned;
    const QStringList snapshotOrder = zzModelOrder(snapshotRows);
    const QStringList targetOrder = zzBuildTargetModelOrder(
        snapshotRows, target, expected.id.value(), targetArea);
    if (!zzStableRuntime(shell_, snapshot)) {
        return false;
    }

    const ZzActivityTransactionScope transaction(shell_);
    if (applyProjection(target, targetOrder, true)) {
        return true;
    }
    static_cast<void>(applyProjection(snapshot, snapshotOrder, false));
    if (!zzMovedRestored(shell_, snapshot, expected)) {
        zzCleanupInterruptedMove(shell_, expected);
    }
    return false;
}

bool ZzWorkspaceActivityMoveTransactionPrivate::applyProjection(
    const ZzProjection &projection,
    const QStringList &modelOrder,
    bool strict)
{
    bool complete = true;
    if (strict && !zzStableRuntime(shell_, projection)) {
        return false;
    }
    const QPointer<ZzFluentUI::ZzSidePane> leftPane(
        zzLivePane(shell_, projection.leftSide));
    const QPointer<ZzFluentUI::ZzSidePane> rightPane(
        zzLivePane(shell_, projection.rightSide));
    const auto paneForContent = [&leftPane, &rightPane](QWidget *content) {
        if (leftPane != nullptr
            && leftPane->panelStack()->panels().contains(content)) {
            return leftPane.data();
        }
        if (rightPane != nullptr
            && rightPane->panelStack()->panels().contains(content)) {
            return rightPane.data();
        }
        return static_cast<ZzFluentUI::ZzSidePane *>(nullptr);
    };

    const auto placeSide = [&](const ZzSide &side,
                               ZzFluentUI::ZzSidePane *destination) {
        const QPointer<ZzFluentUI::ZzSidePane> destinationGuard(destination);
        for (const QString &id : side.order) {
            auto *const record = zzRecord(shell_, id);
            if (record == nullptr || record->content == nullptr
                || record->content.data() != record->contentIdentity) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
            const ZzWorkspaceShellPrivate::ZzPanelRecord expected = *record;
            QWidget *const content = record->content.data();
            ZzFluentUI::ZzSidePane *const current = paneForContent(content);
            if (current != nullptr
                && (!current->isAncestorOf(content)
                    || !current->panelStack()->isAncestorOf(content))) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
            if (current == nullptr && content->parent() != nullptr) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
            if (destinationGuard == nullptr) {
                complete = false;
                if (current != nullptr) {
                    static_cast<void>(current->takeWidget(content));
                }
                if (strict) {
                    return false;
                }
                continue;
            }
            if (current != destinationGuard) {
                if (current != nullptr) {
                    QWidget *const taken = current->takeWidget(content);
                    if (taken != content
                        || shell_.stablePanelIndex(expected) < 0
                        || content->parent() != nullptr
                        || (strict
                            && !zzStableRuntime(shell_, projection, content))) {
                        complete = false;
                        if (strict) {
                            return false;
                        }
                        continue;
                    }
                }
                if (destinationGuard == nullptr
                    || !destinationGuard->addWidget(content, record->title)
                    || destinationGuard == nullptr
                    || (strict && !zzStableRuntime(shell_, projection))) {
                    complete = false;
                    if (strict) {
                        return false;
                    }
                    continue;
                }
            }
        }
        if (destinationGuard == nullptr) {
            return !strict;
        }
        for (qsizetype index = 0; index < side.order.size(); ++index) {
            if (destinationGuard == nullptr) {
                complete = false;
                return false;
            }
            const auto *const record = zzRecord(shell_, side.order.at(index));
            if (record == nullptr || record->content == nullptr
                || !destinationGuard->panelStack()->panels().contains(
                    record->content)) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
            if (!destinationGuard->panelStack()->movePanel(
                    record->content, static_cast<int>(index))
                || destinationGuard == nullptr
                || (strict && !zzStableRuntime(shell_, projection))) {
                complete = false;
                if (strict) {
                    return false;
                }
            }
        }
        return true;
    };
    if (!placeSide(projection.leftSide, leftPane.data())
        || !placeSide(projection.rightSide, rightPane.data())) {
        return false;
    }

    const auto applySideState = [&](const ZzSide &side,
                                    ZzFluentUI::ZzSidePane *pane) {
        const QPointer<ZzFluentUI::ZzSidePane> paneGuard(pane);
        if (paneGuard == nullptr) {
            complete = false;
            return !strict;
        }
        for (const QString &id : side.order) {
            const auto *const record = zzRecord(shell_, id);
            if (record == nullptr || record->content == nullptr) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
            const bool visible = side.visible.contains(id);
            const bool visibilityApplied =
                paneGuard->setWidgetVisible(record->content, visible);
            if (paneGuard == nullptr) {
                complete = false;
                return false;
            }
            if (!visibilityApplied
                || (strict && !zzStableRuntime(shell_, projection))) {
                complete = false;
                if (strict) {
                    return false;
                }
            }
        }
        if (!side.current.isEmpty()) {
            const auto *const current = zzRecord(shell_, side.current);
            if (current == nullptr || current->content == nullptr) {
                complete = false;
                if (strict) {
                    return false;
                }
            } else {
                const bool currentApplied =
                    paneGuard->setCurrentWidget(current->content);
                if (paneGuard == nullptr) {
                    complete = false;
                    return false;
                }
                if (!currentApplied
                    || (strict && !zzStableRuntime(shell_, projection))) {
                    complete = false;
                    if (strict) {
                        return false;
                    }
                }
            }
        }
        paneGuard->setPaneWidth(side.width);
        if (paneGuard == nullptr
            || (strict && !zzStableRuntime(shell_, projection))) {
            return false;
        }
        paneGuard->setCollapsed(side.collapsed);
        if (paneGuard == nullptr
            || (strict && !zzStableRuntime(shell_, projection))) {
            return false;
        }
        return true;
    };
    if (!applySideState(projection.leftSide, leftPane.data())
        || !applySideState(projection.rightSide, rightPane.data())) {
        return false;
    }

    for (auto &record : shell_.panels) {
        if (record.kind != ZzWorkspaceShellPrivate::ZzPanelKind::Side) {
            continue;
        }
        ZzFluentUI::ZzActivityArea area = record.activityArea;
        if (zzAreaForId(projection.activity, record.id.value(), &area)) {
            record.activityArea = area;
        }
    }
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> rows =
        zzRowsForProjection(shell_, projection, modelOrder);
    if (shell_.activityModel == nullptr
        || rows.size() != shell_.activityRows().size()
        || !shell_.replaceActivityRows(rows)) {
        complete = false;
        if (strict) {
            return false;
        }
    }
    if (strict
        && (!zzStableRuntime(shell_, projection)
            || !zzSideMatches(shell_, projection.leftSide, false)
            || !zzSideMatches(shell_, projection.rightSide, false))) {
        return false;
    }

    const QPointer<ZzFluentUI::ZzActivityBar> leftBar(shell_.leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBar(shell_.rightActivityBar);
    const auto stableAfterActivity = [&] {
        return !strict || (leftBar != nullptr && rightBar != nullptr
            && shell_.leftActivityBar == leftBar
            && shell_.rightActivityBar == rightBar
            && zzStableRuntime(shell_, projection)
            && zzSideMatches(shell_, projection.leftSide, false)
            && zzSideMatches(shell_, projection.rightSide, false));
    };
    if (leftBar == nullptr || rightBar == nullptr
        || shell_.activityModel == nullptr) {
        complete = false;
        if (strict) {
            return false;
        }
    } else {
        leftBar->setCurrentSourceIndex(
            zzIndexes(shell_, {projection.leftSide.current}).value(0));
        if (!stableAfterActivity()) {
            return false;
        }
        rightBar->setCurrentSourceIndex(
            zzIndexes(shell_, {projection.rightSide.current}).value(0));
        if (!stableAfterActivity()) {
            return false;
        }
        leftBar->setActiveSourceIndexes(
            zzIndexes(shell_, projection.leftSide.visible));
        if (!stableAfterActivity()) {
            return false;
        }
        rightBar->setActiveSourceIndexes(
            zzIndexes(shell_, projection.rightSide.visible));
        if (!stableAfterActivity()) {
            return false;
        }
    }

    const auto applySizes = [&](const ZzSide &side,
                                ZzFluentUI::ZzSidePane *pane) {
        const QPointer<ZzFluentUI::ZzSidePane> paneGuard(pane);
        if (paneGuard == nullptr) {
            complete = false;
            return !strict;
        }
        const QPointer<ZzFluentUI::ZzPanelStack> stackGuard(
            paneGuard->panelStack());
        if (stackGuard == nullptr || !stackGuard->setPanelSizes(side.sizes)
            || paneGuard == nullptr || stackGuard == nullptr) {
            complete = false;
            return !strict;
        }
        if (strict && (!zzStableRuntime(shell_, projection)
                || !zzSideMatches(shell_, side, true))) {
            return false;
        }
        return true;
    };
    if (!applySizes(projection.leftSide, leftPane.data())
        || !applySizes(projection.rightSide, rightPane.data())) {
        return false;
    }
    if (strict) {
        return zzStableRuntime(shell_, projection)
            && zzSideMatches(shell_, projection.leftSide, true)
            && zzSideMatches(shell_, projection.rightSide, true);
    }
    return complete;
}

} // namespace ZzPureTools
