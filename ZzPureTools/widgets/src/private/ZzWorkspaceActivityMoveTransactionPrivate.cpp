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
        if (content != nullptr) {
            ++allowedVisibilityChanges_[content];
        }
    }

    void allowModelReset() noexcept
    {
        allowedModelResets_ = 1;
    }

    void finishMutation() noexcept
    {
        allowedParentContent_ = nullptr;
        allowedMovedContent_ = nullptr;
        allowedParentChanges_ = 0;
        allowedPanelMoves_ = 0;
        allowedVisibilityChanges_.clear();
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
                const auto allowed = allowedVisibilityChanges_.find(content);
                if (content == nullptr || allowed == allowedVisibilityChanges_.end()
                    || allowed.value() <= 0) {
                    valid_ = false;
                    return;
                }
                --allowed.value();
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
    QHash<QWidget *, int> allowedVisibilityChanges_;
    int allowedParentChanges_ = 0;
    int allowedPanelMoves_ = 0;
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
            && record.materialization
                == ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
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

/** @brief 按输入顺序移除指定面板 ID，不改变其余面板的相对顺序。 */
[[nodiscard]] QStringList zzWithoutId(
    const QStringList &ids,
    const QString &removedId)
{
    QStringList result;
    result.reserve(ids.size());
    for (const QString &id : ids) {
        if (id != removedId) {
            result.append(id);
        }
    }
    return result;
}

/** @brief 验证两个顺序仅允许指定面板发生位置变化。 */
[[nodiscard]] bool zzHasSingleMovedIdOrder(
    const QStringList &actual,
    const QStringList &target,
    const QString &movedId)
{
    if (movedId.isEmpty() || actual.size() != target.size()) {
        return false;
    }
    QSet<QString> actualIds;
    QSet<QString> targetIds;
    actualIds.reserve(actual.size());
    targetIds.reserve(target.size());
    for (const QString &id : actual) {
        if (id.isEmpty() || actualIds.contains(id)) {
            return false;
        }
        actualIds.insert(id);
    }
    for (const QString &id : target) {
        if (id.isEmpty() || targetIds.contains(id)) {
            return false;
        }
        targetIds.insert(id);
    }
    const bool actualHasMoved = actualIds.contains(movedId);
    if (actualHasMoved != targetIds.contains(movedId)) {
        return false;
    }
    return actualHasMoved
        ? zzWithoutId(actual, movedId) == zzWithoutId(target, movedId)
        : actual == target;
}

[[nodiscard]] QStringList zzBuildTargetModelOrder(
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> &snapshotRows,
    const ZzProjection &target,
    const QString &movedId,
    ZzFluentUI::ZzActivityArea targetArea)
{
    QStringList snapshotOrder = zzModelOrder(snapshotRows);
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
    for (const QString &id : std::as_const(snapshotOrder)) {
        const auto area = areas.constFind(id);
        if (area != areas.cend() && area.value() == targetArea) {
            currentArea.append(id);
        }
    }
    if (currentArea == desired) {
        return snapshotOrder;
    }

    QStringList order;
    order.reserve(snapshotOrder.size());
    for (const QString &id : snapshotOrder) {
        if (id != movedId) {
            order.append(id);
        }
    }
    QHash<QString, qsizetype> orderPositions;
    orderPositions.reserve(order.size());
    for (qsizetype index = 0; index < order.size(); ++index) {
        orderPositions.insert(order.at(index), index);
    }
    qsizetype desiredIndex = desired.size();
    for (qsizetype index = 0; index < desired.size(); ++index) {
        if (desired.at(index) == movedId) {
            desiredIndex = index;
            break;
        }
    }
    if (desiredIndex == desired.size()) {
        return order;
    }
    qsizetype insertionIndex = order.size();
    for (qsizetype index = desiredIndex + 1; index < desired.size(); ++index) {
        const auto next = orderPositions.constFind(desired.at(index));
        if (next != orderPositions.cend()) {
            insertionIndex = next.value();
            break;
        }
    }
    if (insertionIndex == order.size()) {
        for (qsizetype index = desiredIndex; index > 0; --index) {
            const auto previous = orderPositions.constFind(desired.at(index - 1));
            if (previous != orderPositions.cend()) {
                insertionIndex = previous.value() + 1;
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

[[nodiscard]] bool zzLogicalIdentitiesMatch(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const ZzAuditIndex &index)
{
    if (index.areas.size() != projection.identities.size()) {
        return false;
    }
    for (const auto &identity : projection.identities) {
        const auto *const record = zzRecord(shell, index, identity.id);
        if (record == nullptr
            || record->kind != ZzWorkspaceShellPrivate::ZzPanelKind::Side
            || record->registrationGeneration
                != identity.registrationGeneration) {
            return false;
        }
        if (record->materialization
            == ZzWorkspaceShellPrivate::ZzMaterializationState::Pending) {
            if (record->content != nullptr || record->contentIdentity != nullptr
                || identity.widget != nullptr
                || identity.rawWidget != nullptr) {
                return false;
            }
            continue;
        }
        if (record->materialization
                != ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
            || !zzSamePanel(shell, index, identity)) {
            return false;
        }
    }
    return true;
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
    const bool left = pane != nullptr && pane == shell.leftSidePane;
    const ZzWorkspacePanelId &shellCurrent = left
        ? shell.leftCurrentPanel : shell.rightCurrentPanel;
    const bool shellExpanded = left
        ? shell.leftPaneExpanded : shell.rightPaneExpanded;
    if (pane == nullptr || !zzSameIdentity(side.stackIdentity, stack)
        || zzIds(index, stack->panels()) != side.order
        || zzIds(index, stack->visiblePanels()) != side.visible
        || zzIdForWidget(index, pane->currentWidget()) != side.current
        || pane->isCollapsed() != side.collapsed
        || shellCurrent.value() != side.current
        || shellExpanded != (!side.current.isEmpty() && !side.collapsed)
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
        && zzLogicalIdentitiesMatch(shell, projection, index)
        && zzSideMatches(
            shell, projection, index, projection.leftSide, includeSizes)
        && zzSideMatches(
            shell, projection, index, projection.rightSide, includeSizes);
}

/**
 * @brief 将纯值移动规划中的逻辑 Side 顺序收窄为 Ready 物理投影。
 *
 * Activity 四区保留全部 Side ID；PanelStack 的顺序、可见项、尺寸和当前项
 * 只能引用已经创建的内容。Pending 迁移因此不会触碰 QWidget。
 */
void zzKeepReadyPhysicalProjection(
    const ZzWorkspaceShellPrivate &shell,
    const ZzSnapshot &snapshot,
    ZzProjection *target)
{
    QSet<QString> readyIds;
    readyIds.reserve(shell.panels.size());
    for (const auto &record : shell.panels) {
        if (record.kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
            && record.materialization
                == ZzWorkspaceShellPrivate::ZzMaterializationState::Ready) {
            readyIds.insert(record.id.value());
        }
    }
    const auto filterSide = [&readyIds](
                                ZzSide *side,
                                const ZzSide &before) {
        QHash<QString, int> sizesById;
        sizesById.reserve(side->visible.size());
        for (qsizetype index = 0; index < side->visible.size(); ++index) {
            sizesById.insert(
                side->visible.at(index),
                index < side->sizes.size() ? side->sizes.at(index) : 1);
        }
        QStringList order;
        for (const QString &id : std::as_const(side->order)) {
            if (readyIds.contains(id)) {
                order.append(id);
            }
        }
        QStringList visible;
        QList<int> sizes;
        for (const QString &id : std::as_const(side->visible)) {
            if (readyIds.contains(id)) {
                visible.append(id);
                sizes.append(sizesById.value(id, 1));
            }
        }
        side->order = std::move(order);
        side->visible = std::move(visible);
        side->sizes = std::move(sizes);
        if (!readyIds.contains(side->current)) {
            side->current = side->visible.contains(before.current)
                ? before.current : QString{};
        }
        QList<ZzLayoutState::ZzContentPlacement> contents;
        contents.reserve(side->contents.size());
        for (const auto &placement : std::as_const(side->contents)) {
            if (readyIds.contains(placement.panelId)) {
                contents.append(placement);
            }
        }
        side->contents = std::move(contents);
    };
    filterSide(&target->leftSide, snapshot.leftSide);
    filterSide(&target->rightSide, snapshot.rightSide);
    target->activity.leftCurrent = target->leftSide.current;
    target->activity.rightCurrent = target->rightSide.current;
    target->activity.leftActive = QSet<QString>(
        target->leftSide.visible.cbegin(), target->leftSide.visible.cend());
    target->activity.rightActive = QSet<QString>(
        target->rightSide.visible.cbegin(), target->rightSide.visible.cend());
}

[[nodiscard]] int zzVisibleSizeForId(
    const ZzSide &side,
    const QString &id) noexcept
{
    const qsizetype visibleIndex = side.visible.indexOf(id);
    return visibleIndex >= 0 && visibleIndex < side.sizes.size()
        ? std::max(side.sizes.at(visibleIndex), 1) : 1;
}

/** @brief 将通用多可见移动投影收敛为每侧唯一 current 与展开状态。 */
void zzApplySingleActivityMoveProjection(
    const ZzSnapshot &snapshot,
    const QString &movedId,
    ZzFluentUI::ZzActivityArea targetArea,
    ZzProjection *target)
{
    const bool sourceLeft = snapshot.leftSide.order.contains(movedId);
    const bool targetLeft = targetArea
            == ZzFluentUI::ZzActivityArea::LeftPrimary
        || targetArea == ZzFluentUI::ZzActivityArea::LeftSecondary;
    const ZzSide &sourceBefore = sourceLeft
        ? snapshot.leftSide : snapshot.rightSide;
    const ZzSide &destinationBefore = targetLeft
        ? snapshot.leftSide : snapshot.rightSide;
    ZzSide &sourceAfter = sourceLeft
        ? target->leftSide : target->rightSide;
    ZzSide &destinationAfter = targetLeft
        ? target->leftSide : target->rightSide;
    const bool movedWasCurrent = sourceBefore.current == movedId;

    const auto restoreSideState = [](ZzSide *side, const ZzSide &before) {
        side->visible.clear();
        side->sizes.clear();
        if (!before.current.isEmpty()
            && side->order.contains(before.current)) {
            side->current = before.current;
            side->visible.append(before.current);
            side->sizes.append(zzVisibleSizeForId(before, before.current));
            side->collapsed = before.collapsed;
        } else {
            side->current.clear();
            side->collapsed = true;
        }
    };
    const auto selectCurrent = [](ZzSide *side, const QString &id,
                                  bool expanded, int size) {
        side->current = id;
        side->visible = id.isEmpty() ? QStringList{} : QStringList{id};
        side->sizes = id.isEmpty() ? QList<int>{} : QList<int>{std::max(size, 1)};
        side->collapsed = id.isEmpty() || !expanded;
    };

    if (sourceLeft == targetLeft) {
        restoreSideState(&sourceAfter, sourceBefore);
    } else if (!movedWasCurrent) {
        restoreSideState(&sourceAfter, sourceBefore);
        restoreSideState(&destinationAfter, destinationBefore);
    } else {
        const QString fallback = sourceAfter.order.value(0);
        selectCurrent(
            &sourceAfter, fallback, !sourceBefore.collapsed,
            zzVisibleSizeForId(sourceBefore, fallback));
        selectCurrent(
            &destinationAfter, movedId, true,
            zzVisibleSizeForId(sourceBefore, movedId));
    }

    target->activity.leftCurrent = target->leftSide.current;
    target->activity.rightCurrent = target->rightSide.current;
    target->activity.leftActive = target->leftSide.current.isEmpty()
        ? QSet<QString>{} : QSet<QString>{target->leftSide.current};
    target->activity.rightActive = target->rightSide.current.isEmpty()
        ? QSet<QString>{} : QSet<QString>{target->rightSide.current};
}

[[nodiscard]] bool zzProjectedRecordStateMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const ZzAuditIndex &index,
    const QString &id)
{
    const auto *const record = zzRecord(shell, index, id);
    if (record == nullptr) {
        return false;
    }
    if (record->materialization
        == ZzWorkspaceShellPrivate::ZzMaterializationState::Pending) {
        return record->content == nullptr
            && record->contentIdentity == nullptr
            && index.areas.contains(id)
            && !index.idsByWidget.values().contains(id);
    }
    return record->materialization
            == ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
        && zzProjectedPanelStateMatches(shell, projection, index, id);
}

[[nodiscard]] bool zzActivityProjectionMatches(
    const ZzWorkspaceShellPrivate &shell,
    const ZzProjection &projection,
    const QStringList &modelOrder)
{
    if (shell.activityModel == nullptr
        || shell.leftActivityBar == nullptr
        || shell.rightActivityBar == nullptr) {
        return false;
    }
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> rows =
        shell.activityRows();
    if (rows.size() != modelOrder.size()) {
        return false;
    }
    QStringList actualOrder;
    actualOrder.reserve(rows.size());
    QHash<QString, ZzFluentUI::ZzActivityArea> actualAreas;
    for (const auto &row : rows) {
        const QString id = row.id.value();
        if (id.isEmpty() || actualAreas.contains(id)) {
            return false;
        }
        actualOrder.append(id);
        actualAreas.insert(id, row.area);
    }
    if (actualOrder != modelOrder) {
        return false;
    }
    const auto rowsForArea = [&actualAreas, &actualOrder](
                                  ZzFluentUI::ZzActivityArea area) {
        QStringList result;
        for (const QString &id : actualOrder) {
            if (actualAreas.value(id) == area) {
                result.append(id);
            }
        }
        return result;
    };
    if (rowsForArea(ZzFluentUI::ZzActivityArea::LeftPrimary)
            != projection.activity.leftPrimary
        || rowsForArea(ZzFluentUI::ZzActivityArea::LeftSecondary)
            != projection.activity.leftSecondary
        || rowsForArea(ZzFluentUI::ZzActivityArea::RightPrimary)
            != projection.activity.rightPrimary
        || rowsForArea(ZzFluentUI::ZzActivityArea::RightSecondary)
            != projection.activity.rightSecondary) {
        return false;
    }
    const auto idAt = [&rows, model = shell.activityModel](
                          const QModelIndex &index) {
        return index.isValid() && index.model() == model && index.row() >= 0
                && index.row() < rows.size()
            ? rows.at(index.row()).id.value() : QString{};
    };
    const auto activeIds = [&idAt](
                               const QList<QModelIndex> &indexes,
                               QSet<QString> *ids) {
        ids->clear();
        for (const QModelIndex &index : indexes) {
            const QString id = idAt(index);
            if (id.isEmpty()) {
                return false;
            }
            ids->insert(id);
        }
        return true;
    };
    QSet<QString> leftActive;
    QSet<QString> rightActive;
    return idAt(shell.leftActivityBar->currentSourceIndex())
            == projection.activity.leftCurrent
        && idAt(shell.rightActivityBar->currentSourceIndex())
            == projection.activity.rightCurrent
        && activeIds(shell.leftActivityBar->activeSourceIndexes(), &leftActive)
        && activeIds(shell.rightActivityBar->activeSourceIndexes(), &rightActive)
        && leftActive == projection.activity.leftActive
        && rightActive == projection.activity.rightActive;
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
        || shell.activityModel == nullptr) {
        return false;
    }
    if (expected.materialization
        == ZzWorkspaceShellPrivate::ZzMaterializationState::Pending) {
        if (record->materialization
                != ZzWorkspaceShellPrivate::ZzMaterializationState::Pending
            || record->content != nullptr
            || record->contentIdentity != nullptr) {
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
    if (expected.materialization
            != ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
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
    const int sidePanelTargetRow = shell_.sidePanelTargetRow(
        targetArea, targetRow);
    if (sidePanelTargetRow < 0) {
        return false;
    }
    const auto planned = ZzLayoutState::buildActivityMoveTarget(
        snapshot, expected.id.value(), targetArea, sidePanelTargetRow);
    if (!planned.has_value()) {
        return false;
    }
    ZzProjection target = *planned;
    zzKeepReadyPhysicalProjection(shell_, snapshot, &target);
    zzApplySingleActivityMoveProjection(
        snapshot, expected.id.value(), targetArea, &target);
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
            if (current != destinationGuard && id != movedId_) {
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
                    if (current->mode() == ZzFluentUI::ZzSidePaneMode::Single
                        && current->currentWidget() == content) {
                        for (QWidget *const candidate
                             : current->panelStack()->panels()) {
                            if (candidate != content) {
                                mutationObserver.allowVisibilityChange(candidate);
                                break;
                            }
                        }
                    }
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
                if (destinationGuard->mode()
                        == ZzFluentUI::ZzSidePaneMode::Single
                    && destinationGuard->currentWidget() != nullptr
                    && destinationGuard->currentWidget() != content) {
                    mutationObserver.allowVisibilityChange(
                        destinationGuard->currentWidget());
                }
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
        const QList<QWidget *> panels = destinationGuard->panelStack()->panels();
        const QStringList actualOrder = zzIds(audit, panels);
        const bool targetHasMovedId = side.order.contains(movedId_);
        const bool actualHasMovedId = actualOrder.contains(movedId_);
        if (actualOrder.size() != panels.size()
            || ((targetHasMovedId || !actualHasMovedId)
                && !zzHasSingleMovedIdOrder(
                    actualOrder, side.order, movedId_))) {
            complete = false;
            return !strict;
        }
        for (const QString &id : side.order) {
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
        }
        if (!targetHasMovedId && actualHasMovedId) {
            return true;
        }
        const qsizetype actualIndex = actualOrder.indexOf(movedId_);
        const qsizetype targetIndex = side.order.indexOf(movedId_);
        if (actualIndex < 0 && targetIndex < 0) {
            return true;
        }
        if (actualIndex < 0 || targetIndex < 0) {
            complete = false;
            return !strict;
        }
        const auto *const moved = zzRecord(shell_, audit, movedId_);
        if (moved == nullptr || moved->content == nullptr) {
            complete = false;
            return !strict;
        }
        if (actualIndex != targetIndex) {
            mutationObserver.allowPanelMove(moved->content);
            const bool movedPanel = destinationGuard->panelStack()->movePanel(
                moved->content, static_cast<int>(targetIndex));
            mutationObserver.finishMutation();
            if (!movedPanel || !mutationObserver.isValid()
                || destinationGuard == nullptr
                || !zzBoundaryMatches(
                    shell_, projection, audit, movedId_, destinationGuard,
                    false, strict)) {
                complete = false;
                return !strict;
            }
        }
        if (zzIds(audit, destinationGuard->panelStack()->panels()) != side.order) {
            complete = false;
            return !strict;
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
            if (visible && paneGuard->mode()
                    == ZzFluentUI::ZzSidePaneMode::Single) {
                for (QWidget *const candidate
                     : paneGuard->panelStack()->panels()) {
                    if (candidate != record->content
                        && paneGuard->panelStack()->isPanelVisible(candidate)) {
                        mutationObserver.allowVisibilityChange(candidate);
                    }
                }
            }
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
            || !zzProjectedRecordStateMatches(
                shell_, projection, audit, movedId_))) {
        return false;
    }

    shell_.leftCurrentPanel = ZzWorkspacePanelId(
        projection.leftSide.current);
    shell_.rightCurrentPanel = ZzWorkspacePanelId(
        projection.rightSide.current);
    shell_.leftPaneExpanded = !projection.leftSide.current.isEmpty()
        && !projection.leftSide.collapsed;
    shell_.rightPaneExpanded = !projection.rightSide.current.isEmpty()
        && !projection.rightSide.collapsed;

    const QPointer<ZzFluentUI::ZzActivityBar> leftBar(shell_.leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBar(shell_.rightActivityBar);
    const auto stableAfterActivity = [&] {
        return !strict || (leftBar != nullptr && rightBar != nullptr
            && mutationObserver.isValid()
            && shell_.leftActivityBar == leftBar
            && shell_.rightActivityBar == rightBar
            && zzStableSubsystems(shell_, projection)
            && zzProjectedRecordStateMatches(
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
                || !zzProjectedRecordStateMatches(
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
    shell_.syncSideEdgeVisibility();
    if (strict) {
        return mutationObserver.isValid()
            && zzProjectionMatches(shell_, projection, audit, true)
            && zzActivityProjectionMatches(shell_, projection, modelOrder)
            && ((shell_.leftActivityBar != nullptr
                    && !shell_.leftActivityBar->isHidden())
                == (!projection.activity.leftPrimary.isEmpty()
                    || !projection.activity.leftSecondary.isEmpty()))
            && ((shell_.rightActivityBar != nullptr
                    && !shell_.rightActivityBar->isHidden())
                == (!projection.activity.rightPrimary.isEmpty()
                    || !projection.activity.rightSecondary.isEmpty()))
            && ((shell_.leftSidePane != nullptr
                    && !shell_.leftSidePane->isHidden())
                == shell_.leftPaneExpanded)
            && ((shell_.rightSidePane != nullptr
                    && !shell_.rightSidePane->isHidden())
                == shell_.rightPaneExpanded);
    }
    return complete;
}

} // namespace ZzPureTools
