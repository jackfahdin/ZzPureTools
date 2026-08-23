#include "ZzWorkspaceActivityMoveTransactionPrivate.h"

#include <algorithm>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QSet>
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

struct ZzAuditIndex final
{
    QHash<QString, int> recordRows;
    QHash<QString, qsizetype> identityRows;
    QHash<QWidget *, QString> idsByWidget;
    QHash<QString, ZzFluentUI::ZzActivityArea> areas;
    QHash<QString, int> modelRows;
    QSet<QString> visibleIds;
    QHash<QString, QPointer<QWidget>> frames;
    QHash<QString, QWidget *> rawFrames;
    bool valid = true;
};

class ZzMutationObserver final : public QObject
{
public:
    explicit ZzMutationObserver(
        ZzWorkspaceShellPrivate &shell,
        const ZzProjection &projection)
    {
        watchedContents_.reserve(projection.identities.size());
        for (const auto &identity : projection.identities) {
            if (identity.widget == nullptr) {
                continue;
            }
            watchedContents_.append(identity.widget);
            identity.widget->installEventFilter(this);
            QObject::connect(
                identity.widget, &QObject::destroyed,
                this, [this] { valid_ = false; });
        }
        watchStack(shell.leftSidePane != nullptr
                ? shell.leftSidePane->panelStack() : nullptr);
        watchStack(shell.rightSidePane != nullptr
                ? shell.rightSidePane->panelStack() : nullptr);
        if (shell.activityModel != nullptr) {
            QObject::connect(
                shell.activityModel, &QAbstractItemModel::modelReset,
                this, [this] {
                    if (allowedModelResets_ > 0) {
                        --allowedModelResets_;
                    } else {
                        valid_ = false;
                    }
                });
            const auto invalidate = [this] { valid_ = false; };
            QObject::connect(
                shell.activityModel, &QAbstractItemModel::rowsInserted,
                this, invalidate);
            QObject::connect(
                shell.activityModel, &QAbstractItemModel::rowsRemoved,
                this, invalidate);
            QObject::connect(
                shell.activityModel, &QAbstractItemModel::rowsMoved,
                this, invalidate);
            QObject::connect(
                shell.activityModel, &QAbstractItemModel::layoutChanged,
                this, invalidate);
            QObject::connect(
                shell.activityModel, &QAbstractItemModel::dataChanged,
                this, invalidate);
        }
    }

    ~ZzMutationObserver() override
    {
        for (const QPointer<QWidget> &content : std::as_const(watchedContents_)) {
            if (content != nullptr) {
                content->removeEventFilter(this);
            }
        }
    }

    void allowParentChange(QWidget *content) noexcept
    {
        allowedParentContent_ = content;
        allowedParentChanges_ = 1;
    }

    void allowPanelMove(QWidget *content) noexcept
    {
        allowedMovedContent_ = content;
        allowedPanelMoves_ = 1;
    }

    void allowVisibilityChange(QWidget *content) noexcept
    {
        allowedVisibilityContent_ = content;
        allowedVisibilityChanges_ = 1;
    }

    void allowModelReset() noexcept
    {
        allowedModelResets_ = 1;
    }

    void finishMutation() noexcept
    {
        allowedParentContent_ = nullptr;
        allowedMovedContent_ = nullptr;
        allowedVisibilityContent_ = nullptr;
        allowedParentChanges_ = 0;
        allowedPanelMoves_ = 0;
        allowedVisibilityChanges_ = 0;
        allowedModelResets_ = 0;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return valid_;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event != nullptr && event->type() == QEvent::ParentChange) {
            consume(
                qobject_cast<QWidget *>(watched),
                allowedParentContent_, allowedParentChanges_);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void watchStack(ZzFluentUI::ZzPanelStack *stack)
    {
        if (stack == nullptr) {
            return;
        }
        QObject::connect(
            stack, &ZzFluentUI::ZzPanelStack::panelMoved,
            this, [this](QWidget *content, int) {
                consume(
                    content, allowedMovedContent_, allowedPanelMoves_);
            });
        QObject::connect(
            stack, &ZzFluentUI::ZzPanelStack::panelVisibilityChanged,
            this, [this](QWidget *content, bool) {
                consume(
                    content, allowedVisibilityContent_,
                    allowedVisibilityChanges_);
            });
    }

    void consume(
        QWidget *actual,
        const QPointer<QWidget> &expected,
        int &remaining) noexcept
    {
        if (actual == nullptr || expected == nullptr
            || actual != expected.data() || remaining <= 0) {
            valid_ = false;
            return;
        }
        --remaining;
    }

    QList<QPointer<QWidget>> watchedContents_;
    QPointer<QWidget> allowedParentContent_;
    QPointer<QWidget> allowedMovedContent_;
    QPointer<QWidget> allowedVisibilityContent_;
    int allowedParentChanges_ = 0;
    int allowedPanelMoves_ = 0;
    int allowedVisibilityChanges_ = 0;
    int allowedModelResets_ = 0;
    bool valid_ = true;
};

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

void zzInsertActivityRows(
    ZzAuditIndex *index,
    const QStringList &ids,
    ZzFluentUI::ZzActivityArea area)
{
    for (const QString &id : ids) {
        if (index->areas.contains(id)) {
            index->valid = false;
            continue;
        }
        index->areas.insert(id, area);
    }
}

[[nodiscard]] ZzAuditIndex zzBuildAuditIndex(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection)
{
    ZzAuditIndex result;
    result.recordRows.reserve(shell.panels.size());
    result.idsByWidget.reserve(shell.panels.size());
    for (qsizetype row = 0; row < shell.panels.size(); ++row) {
        const auto &record = shell.panels.at(row);
        const QString id = record.id.value();
        if (result.recordRows.contains(id)) {
            result.valid = false;
            continue;
        }
        result.recordRows.insert(id, static_cast<int>(row));
        if (record.kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
            && record.content != nullptr
            && record.content.data() == record.contentIdentity) {
            result.idsByWidget.insert(record.contentIdentity, id);
            result.frames.insert(id, record.content->parentWidget());
            result.rawFrames.insert(
                id, record.content->parentWidget());
        }
    }
    result.identityRows.reserve(projection.identities.size());
    for (qsizetype row = 0; row < projection.identities.size(); ++row) {
        const QString &id = projection.identities.at(row).id;
        if (result.identityRows.contains(id)) {
            result.valid = false;
            continue;
        }
        result.identityRows.insert(id, row);
    }
    zzInsertActivityRows(
        &result, projection.activity.leftPrimary,
        ZzFluentUI::ZzActivityArea::LeftPrimary);
    zzInsertActivityRows(
        &result, projection.activity.leftSecondary,
        ZzFluentUI::ZzActivityArea::LeftSecondary);
    zzInsertActivityRows(
        &result, projection.activity.rightPrimary,
        ZzFluentUI::ZzActivityArea::RightPrimary);
    zzInsertActivityRows(
        &result, projection.activity.rightSecondary,
        ZzFluentUI::ZzActivityArea::RightSecondary);
    result.visibleIds.reserve(
        projection.leftSide.visible.size()
        + projection.rightSide.visible.size());
    for (const QString &id : projection.leftSide.visible) {
        result.visibleIds.insert(id);
    }
    for (const QString &id : projection.rightSide.visible) {
        result.visibleIds.insert(id);
    }
    const auto modelRows = shell.activityRows();
    result.modelRows.reserve(modelRows.size());
    for (const auto &row : modelRows) {
        const QString id = row.id.value();
        if (result.modelRows.contains(id)) {
            result.valid = false;
            continue;
        }
        result.modelRows.insert(id, row.order);
    }
    return result;
}

[[nodiscard]] const ZzWorkspaceShellPrivate::ZzPanelRecord *zzRecord(
    const ZzWorkspaceShellPrivate &shell,
    const ZzAuditIndex &index,
    const QString &id)
{
    const auto row = index.recordRows.constFind(id);
    if (row == index.recordRows.cend()
        || row.value() < 0 || row.value() >= shell.panels.size()
        || shell.panels.at(row.value()).id.value() != id) {
        return nullptr;
    }
    return &shell.panels.at(row.value());
}

[[nodiscard]] ZzWorkspaceShellPrivate::ZzPanelRecord *zzRecord(
    ZzWorkspaceShellPrivate &shell,
    const ZzAuditIndex &index,
    const QString &id)
{
    return const_cast<ZzWorkspaceShellPrivate::ZzPanelRecord *>(
        zzRecord(std::as_const(shell), index, id));
}

[[nodiscard]] const ZzLayoutState::ZzPanelIdentity *zzPanelIdentity(
    const ZzProjection &projection,
    const ZzAuditIndex &index,
    const QString &id)
{
    const auto row = index.identityRows.constFind(id);
    return row != index.identityRows.cend()
            && row.value() >= 0
            && row.value() < projection.identities.size()
        ? &projection.identities.at(row.value()) : nullptr;
}

[[nodiscard]] QString zzIdForWidget(
    const ZzAuditIndex &index,
    QWidget *widget)
{
    return index.idsByWidget.value(widget);
}

[[nodiscard]] ZzLayoutState::ZzSubsystemIdentity zzIdentity(
    QObject *object)
{
    return {object, object};
}

void zzCaptureSide(
    const QHash<QWidget *, QString> &idsByWidget,
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
        const QString id = idsByWidget.value(content);
        if (!id.isEmpty()) {
            side->order.append(id);
            side->contents.append(
                {id, side->stackIdentity,
                    {side->paneIdentity, side->stackIdentity}});
        }
    }
    for (QWidget *const content : stack->visiblePanels()) {
        const QString id = idsByWidget.value(content);
        if (!id.isEmpty()) {
            side->visible.append(id);
        }
    }
    side->sizes = stack->panelSizes();
    side->current = idsByWidget.value(pane->currentWidget());
    side->collapsed = pane->isCollapsed();
    side->width = pane->paneWidth();
}

[[nodiscard]] ZzSnapshot zzCaptureSnapshot(
    const ZzWorkspaceShellPrivate &shell)
{
    ZzSnapshot snapshot;
    QHash<QWidget *, QString> idsByWidget;
    idsByWidget.reserve(shell.panels.size());
    for (const auto &record : shell.panels) {
        if (record.kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
            && record.contentIdentity != nullptr) {
            idsByWidget.insert(record.contentIdentity, record.id.value());
        }
    }
    zzCaptureSide(idsByWidget, shell.leftSidePane, &snapshot.leftSide);
    zzCaptureSide(idsByWidget, shell.rightSidePane, &snapshot.rightSide);
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
    QHash<QString, ZzFluentUI::ZzActivityArea> areas;
    ZzAuditIndex areaIndex;
    zzInsertActivityRows(
        &areaIndex, target.activity.leftPrimary,
        ZzFluentUI::ZzActivityArea::LeftPrimary);
    zzInsertActivityRows(
        &areaIndex, target.activity.leftSecondary,
        ZzFluentUI::ZzActivityArea::LeftSecondary);
    zzInsertActivityRows(
        &areaIndex, target.activity.rightPrimary,
        ZzFluentUI::ZzActivityArea::RightPrimary);
    zzInsertActivityRows(
        &areaIndex, target.activity.rightSecondary,
        ZzFluentUI::ZzActivityArea::RightSecondary);
    areas = std::move(areaIndex.areas);
    QStringList currentArea;
    for (const QString &id : std::as_const(order)) {
        const auto area = areas.constFind(id);
        if (area != areas.cend() && area.value() == targetArea) {
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
    const ZzAuditIndex &index,
    const ZzLayoutState::ZzPanelIdentity &expected)
{
    const auto *const record = zzRecord(shell, index, expected.id);
    return record != nullptr
        && record->kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
        && record->registrationGeneration == expected.registrationGeneration
        && record->content != nullptr
        && record->content.data() == expected.rawWidget
        && record->contentIdentity == expected.rawWidget
        && expected.widget != nullptr
        && expected.widget.data() == expected.rawWidget;
}

[[nodiscard]] bool zzStableSubsystems(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection)
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
    return true;
}

[[nodiscard]] bool zzBoundaryMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const ZzAuditIndex &index,
    const QString &id,
    ZzFluentUI::ZzSidePane *pane,
    bool transient = false,
    bool requireAllSubsystems = true)
{
    if (!index.valid
        || (requireAllSubsystems
            && !zzStableSubsystems(shell, projection))) {
        return false;
    }
    const auto *const identity = zzPanelIdentity(projection, index, id);
    if (identity == nullptr || !zzSamePanel(shell, index, *identity)) {
        return false;
    }
    QWidget *const content = identity->widget.data();
    if (transient) {
        return content != nullptr && content->parent() == nullptr;
    }
    const QPointer<QWidget> frame = index.frames.value(id);
    return pane != nullptr && pane->panelStack() != nullptr
        && frame != nullptr && frame.data() == index.rawFrames.value(id)
        && content->parentWidget() == frame
        && pane->isAncestorOf(content)
        && pane->panelStack()->isAncestorOf(content);
}

[[nodiscard]] bool zzProjectedVisibilityMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const ZzAuditIndex &index,
    const QString &id,
    bool requireAllSubsystems = true)
{
    const auto area = index.areas.constFind(id);
    const auto *const identity = zzPanelIdentity(projection, index, id);
    if (area == index.areas.cend() || identity == nullptr) {
        return false;
    }
    const bool left = area.value()
            == ZzFluentUI::ZzActivityArea::LeftPrimary
        || area.value() == ZzFluentUI::ZzActivityArea::LeftSecondary;
    ZzFluentUI::ZzSidePane *const pane = left
        ? shell.leftSidePane.data() : shell.rightSidePane.data();
    const QPointer<QWidget> frame = index.frames.value(id);
    return zzBoundaryMatches(
               shell, projection, index, id, pane, false,
               requireAllSubsystems)
        && frame != nullptr
        && (!frame->isHidden()) == index.visibleIds.contains(id);
}

[[nodiscard]] bool zzProjectedPanelStateMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const ZzAuditIndex &index,
    const QString &id,
    bool requireAllSubsystems = true)
{
    const auto area = index.areas.constFind(id);
    if (area == index.areas.cend()
        || !zzProjectedVisibilityMatches(
            shell, projection, index, id, requireAllSubsystems)) {
        return false;
    }
    const bool left = area.value()
            == ZzFluentUI::ZzActivityArea::LeftPrimary
        || area.value() == ZzFluentUI::ZzActivityArea::LeftSecondary;
    ZzFluentUI::ZzSidePane *const pane = left
        ? shell.leftSidePane.data() : shell.rightSidePane.data();
    const QString &current = left
        ? projection.leftSide.current : projection.rightSide.current;
    const auto *const identity = zzPanelIdentity(projection, index, id);
    return identity != nullptr
        && (current != id || pane->currentWidget() == identity->widget);
}

[[nodiscard]] QStringList zzIds(
    const ZzAuditIndex &index,
    const QList<QWidget *> &widgets)
{
    QStringList result;
    result.reserve(widgets.size());
    for (QWidget *const widget : widgets) {
        result.append(zzIdForWidget(index, widget));
    }
    return result;
}

[[nodiscard]] bool zzSideMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const ZzAuditIndex &index,
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
        || zzIds(index, stack->panels()) != side.order
        || zzIds(index, stack->visiblePanels()) != side.visible
        || zzIdForWidget(index, pane->currentWidget()) != side.current
        || pane->isCollapsed() != side.collapsed
        || pane->paneWidth() != side.width
        || (includeSizes && stack->panelSizes() != side.sizes)) {
        return false;
    }
    for (const QString &id : side.order) {
        const auto *const record = zzRecord(shell, index, id);
        const auto *const identity = zzPanelIdentity(projection, index, id);
        if (record == nullptr || identity == nullptr
            || !zzSamePanel(shell, index, *identity)
            || record->content == nullptr
            || !stack->isAncestorOf(record->content)
            || !pane->isAncestorOf(record->content)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool zzProjectionMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const ZzAuditIndex &index,
    bool includeSizes)
{
    return index.valid && zzStableSubsystems(shell, projection)
        && projection.leftSide.order.size()
                + projection.rightSide.order.size()
            == projection.identities.size()
        && zzSideMatches(
            shell, projection, index, projection.leftSide, includeSizes)
        && zzSideMatches(
            shell, projection, index, projection.rightSide, includeSizes);
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
    const ZzAuditIndex &index,
    const QStringList &ids)
{
    QList<QModelIndex> result;
    if (shell.activityModel == nullptr) {
        return result;
    }
    for (const QString &id : ids) {
        const auto row = index.modelRows.constFind(id);
        if (row != index.modelRows.cend()) {
            result.append(shell.activityModel->index(row.value(), 0));
        }
    }
    return result;
}

[[nodiscard]] QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry>
zzRowsForProjection(
    const ZzWorkspaceShellPrivate &shell,
    const ZzAuditIndex &index,
    const QStringList &modelOrder)
{
    QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> rows;
    for (const QString &id : modelOrder) {
        const auto area = index.areas.constFind(id);
        if (zzRecord(shell, index, id) != nullptr
            && area != index.areas.cend()) {
            rows.append({
                ZzWorkspacePanelId(id), area.value(),
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
    const ZzAuditIndex &index,
    const ZzWorkspaceShellPrivate::ZzPanelRecord &expected)
{
    const auto *const record = zzRecord(shell, index, expected.id.value());
    if (record == nullptr || record->registrationGeneration
            != expected.registrationGeneration
        || record->contentIdentity != expected.contentIdentity
        || record->content != expected.content
        || shell.activityModel == nullptr || expected.content == nullptr) {
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
    const int sourcePanelRow = shell_.indexOf(
        snapshotRows.at(sourceIndex.row()).id);
    const ZzWorkspaceShellPrivate::ZzPanelRecord *const sourceRecord =
        sourcePanelRow >= 0 ? &shell_.panels.at(sourcePanelRow) : nullptr;
    if (sourceRecord == nullptr
        || sourceRecord->kind != ZzWorkspaceShellPrivate::ZzPanelKind::Side) {
        return false;
    }
    const ZzWorkspaceShellPrivate::ZzPanelRecord expected = *sourceRecord;
    movedId_ = expected.id.value();
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
    const ZzAuditIndex snapshotAudit = zzBuildAuditIndex(shell_, snapshot);
    if (!zzProjectionMatches(shell_, snapshot, snapshotAudit, true)) {
        return false;
    }

    const ZzActivityTransactionScope transaction(shell_);
    if (applyProjection(target, targetOrder, true)) {
        return true;
    }
    static_cast<void>(applyProjection(snapshot, snapshotOrder, false));
    const ZzAuditIndex rollbackAudit = zzBuildAuditIndex(shell_, snapshot);
    if (!zzMovedRestored(shell_, snapshot, rollbackAudit, expected)) {
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
    ZzAuditIndex audit = zzBuildAuditIndex(shell_, projection);
    ZzMutationObserver mutationObserver(shell_, projection);
    if (strict
        && (!audit.valid || !zzStableSubsystems(shell_, projection))) {
        return false;
    }
    const QPointer<ZzFluentUI::ZzSidePane> leftPane(
        zzLivePane(shell_, projection.leftSide));
    const QPointer<ZzFluentUI::ZzSidePane> rightPane(
        zzLivePane(shell_, projection.rightSide));
    const auto paneForContent = [&leftPane, &rightPane](QWidget *content) {
        if (leftPane != nullptr
            && leftPane->isAncestorOf(content)
            && leftPane->panelStack()->isAncestorOf(content)) {
            return leftPane.data();
        }
        if (rightPane != nullptr
            && rightPane->isAncestorOf(content)
            && rightPane->panelStack()->isAncestorOf(content)) {
            return rightPane.data();
        }
        return static_cast<ZzFluentUI::ZzSidePane *>(nullptr);
    };

    const auto placeSide = [&](const ZzSide &side,
                               ZzFluentUI::ZzSidePane *destination) {
        const QPointer<ZzFluentUI::ZzSidePane> destinationGuard(destination);
        for (const QString &id : side.order) {
            auto *const record = zzRecord(shell_, audit, id);
            if (record == nullptr || record->content == nullptr
                || record->content.data() != record->contentIdentity) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
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
                    mutationObserver.allowParentChange(content);
                    static_cast<void>(current->takeWidget(content));
                    mutationObserver.finishMutation();
                }
                if (strict) {
                    return false;
                }
                continue;
            }
            if (current != destinationGuard) {
                if (current != nullptr) {
                    mutationObserver.allowParentChange(content);
                    QWidget *const taken = current->takeWidget(content);
                    mutationObserver.finishMutation();
                    if (taken != content
                        || !mutationObserver.isValid()
                        || !zzBoundaryMatches(
                            shell_, projection, audit, id, nullptr, true,
                            strict)) {
                        complete = false;
                        if (strict) {
                            return false;
                        }
                        continue;
                    }
                }
                mutationObserver.allowParentChange(content);
                const bool added = destinationGuard != nullptr
                    && destinationGuard->addWidget(content, record->title);
                mutationObserver.finishMutation();
                if (added && destinationGuard != nullptr
                    && destinationGuard->isAncestorOf(content)
                    && destinationGuard->panelStack()->isAncestorOf(content)) {
                    audit.frames.insert(id, content->parentWidget());
                    audit.rawFrames.insert(id, content->parentWidget());
                }
                if (!added || !mutationObserver.isValid()
                    || destinationGuard == nullptr
                    || !zzBoundaryMatches(
                        shell_, projection, audit, id, destinationGuard,
                        false, strict)) {
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
            const QString &id = side.order.at(index);
            const auto *const record = zzRecord(shell_, audit, id);
            if (record == nullptr || record->content == nullptr
                || !zzBoundaryMatches(
                    shell_, projection, audit, id, destinationGuard,
                    false, strict)) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
            mutationObserver.allowPanelMove(record->content);
            const bool moved = destinationGuard->panelStack()->movePanel(
                record->content, static_cast<int>(index));
            mutationObserver.finishMutation();
            if (!moved || !mutationObserver.isValid()
                || destinationGuard == nullptr
                || !zzBoundaryMatches(
                    shell_, projection, audit, id, destinationGuard,
                    false, strict)) {
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
            const auto *const record = zzRecord(shell_, audit, id);
            if (record == nullptr || record->content == nullptr) {
                complete = false;
                if (strict) {
                    return false;
                }
                continue;
            }
            const bool visible = audit.visibleIds.contains(id);
            const QPointer<QWidget> frame = audit.frames.value(id);
            if (frame != nullptr && (!frame->isHidden()) == visible) {
                if (!mutationObserver.isValid()
                    || !zzProjectedVisibilityMatches(
                        shell_, projection, audit, id, strict)) {
                    complete = false;
                    if (strict) {
                        return false;
                    }
                }
                continue;
            }
            mutationObserver.allowVisibilityChange(record->content);
            const bool visibilityApplied =
                paneGuard->setWidgetVisible(record->content, visible);
            mutationObserver.finishMutation();
            if (paneGuard == nullptr) {
                complete = false;
                return false;
            }
            if (!visibilityApplied || !mutationObserver.isValid()
                || !zzProjectedVisibilityMatches(
                    shell_, projection, audit, id, strict)) {
                complete = false;
                if (strict) {
                    return false;
                }
            }
        }
        if (!side.current.isEmpty()) {
            const auto *const current = zzRecord(shell_, audit, side.current);
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
                if (!currentApplied || !mutationObserver.isValid()
                    || !zzProjectedPanelStateMatches(
                        shell_, projection, audit, side.current, strict)) {
                    complete = false;
                    if (strict) {
                        return false;
                    }
                }
            }
        }
        paneGuard->setPaneWidth(side.width);
        if (paneGuard == nullptr
            || !mutationObserver.isValid()
            || (strict && !zzStableSubsystems(shell_, projection))) {
            return false;
        }
        paneGuard->setCollapsed(side.collapsed);
        if (paneGuard == nullptr
            || !mutationObserver.isValid()
            || (strict && !zzStableSubsystems(shell_, projection))) {
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
        const auto area = audit.areas.constFind(record.id.value());
        if (area != audit.areas.cend()) {
            record.activityArea = area.value();
        }
    }
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> rows =
        zzRowsForProjection(shell_, audit, modelOrder);
    mutationObserver.allowModelReset();
    const bool rowsReplaced = shell_.activityModel != nullptr
        && rows.size() == shell_.activityRows().size()
        && shell_.replaceActivityRows(rows);
    mutationObserver.finishMutation();
    if (!rowsReplaced || !mutationObserver.isValid()) {
        complete = false;
        if (strict) {
            return false;
        }
    } else {
        audit.modelRows.clear();
        audit.modelRows.reserve(rows.size());
        for (const auto &row : rows) {
            audit.modelRows.insert(row.id.value(), row.order);
        }
    }
    if (strict
        && (!zzStableSubsystems(shell_, projection)
            || !zzProjectedPanelStateMatches(
                shell_, projection, audit, movedId_))) {
        return false;
    }

    const QPointer<ZzFluentUI::ZzActivityBar> leftBar(shell_.leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBar(shell_.rightActivityBar);
    const auto stableAfterActivity = [&] {
        return !strict || (leftBar != nullptr && rightBar != nullptr
            && mutationObserver.isValid()
            && shell_.leftActivityBar == leftBar
            && shell_.rightActivityBar == rightBar
            && zzStableSubsystems(shell_, projection)
            && zzProjectedPanelStateMatches(
                shell_, projection, audit, movedId_));
    };
    if (leftBar == nullptr || rightBar == nullptr
        || shell_.activityModel == nullptr) {
        complete = false;
        if (strict) {
            return false;
        }
    } else {
        leftBar->setCurrentSourceIndex(
            zzIndexes(
                shell_, audit, {projection.leftSide.current}).value(0));
        if (!stableAfterActivity()) {
            return false;
        }
        rightBar->setCurrentSourceIndex(
            zzIndexes(
                shell_, audit, {projection.rightSide.current}).value(0));
        if (!stableAfterActivity()) {
            return false;
        }
        leftBar->setActiveSourceIndexes(
            zzIndexes(shell_, audit, projection.leftSide.visible));
        if (!stableAfterActivity()) {
            return false;
        }
        rightBar->setActiveSourceIndexes(
            zzIndexes(shell_, audit, projection.rightSide.visible));
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
        if (!mutationObserver.isValid()
            || (strict && (!zzStableSubsystems(shell_, projection)
                || !zzProjectedPanelStateMatches(
                    shell_, projection, audit, movedId_)
                || stackGuard->panelSizes() != side.sizes))) {
            return false;
        }
        return true;
    };
    if (!applySizes(projection.leftSide, leftPane.data())
        || !applySizes(projection.rightSide, rightPane.data())) {
        return false;
    }
    if (strict) {
        return mutationObserver.isValid()
            && zzProjectionMatches(shell_, projection, audit, true);
    }
    return complete;
}

} // namespace ZzPureTools
