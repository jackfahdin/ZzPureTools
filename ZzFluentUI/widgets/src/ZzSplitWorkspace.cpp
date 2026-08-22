#include <ZzFluentUI/ZzSplitWorkspace.h>

#include <QtCore/QPointer>
#include <QtCore/QEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>

#include <ZzFluentUI/ZzTabWidget.h>

#include "private/ZzSplitWorkspacePrivate.h"

namespace ZzFluentUI {

ZzSplitWorkspace::ZzSplitWorkspace(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzSplitWorkspacePrivate>(this))
{
}

ZzSplitWorkspace::~ZzSplitWorkspace() = default;

QList<ZzTabGroupId> ZzSplitWorkspace::groupIds() const
{
    return d_ptr->groupIds();
}

ZzTabGroupId ZzSplitWorkspace::activeGroupId() const
{
    return d_ptr->activeId;
}

bool ZzSplitWorkspace::setActiveGroup(const ZzTabGroupId &id)
{
    ZzNode *const node = d_ptr->findLeaf(id);
    if (node == nullptr) {
        return false;
    }

    const bool changed = d_ptr->activeId != id;
    d_ptr->activeId = id;
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    QPointer<ZzTabWidget> guardedTabs = std::get<ZzLeaf>(node->value).tabs;
    if (!guardedTabs.isNull()) {
        guardedTabs->setFocus(Qt::OtherFocusReason);
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (changed) {
        Q_EMIT activeGroupChanged(id);
    }
    return true;
}

ZzTabWidget *ZzSplitWorkspace::tabWidget(
    const ZzTabGroupId &id) const noexcept
{
    ZzNode *const node = d_ptr->findLeaf(id);
    return node != nullptr
        ? std::get<ZzLeaf>(node->value).tabs.data()
        : nullptr;
}

ZzTabGroupId ZzSplitWorkspace::groupId(const ZzTabWidget *tabs) const
{
    ZzNode *const node = d_ptr->findLeaf(tabs);
    return node != nullptr ? std::get<ZzLeaf>(node->value).id
                           : ZzTabGroupId {};
}

std::optional<ZzTabGroupId> ZzSplitWorkspace::splitGroup(
    const ZzTabGroupId &source,
    Qt::Orientation orientation,
    ZzSplitPlacement placement,
    const ZzTabGroupId &requestedId)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    const auto result = d_ptr->splitGroup(
        source, orientation, placement, requestedId);
    if (!result.has_value() || guardedWorkspace.isNull()) {
        return result;
    }

    Q_EMIT groupAdded(result.value());
    if (guardedWorkspace.isNull()) {
        return result;
    }
    Q_EMIT layoutChanged();
    return result;
}

bool ZzSplitWorkspace::removeEmptyGroup(const ZzTabGroupId &id)
{
    const bool removedActive = d_ptr->activeId == id;
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    if (!d_ptr->removeEmptyGroup(id)) {
        return false;
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }

    Q_EMIT groupAboutToBeRemoved(id);
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (removedActive) {
        Q_EMIT activeGroupChanged(d_ptr->activeId);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    Q_EMIT layoutChanged();
    return true;
}

bool ZzSplitWorkspace::focusAdjacentGroup(Qt::Edge direction)
{
    const ZzTabGroupId adjacent = d_ptr->adjacentGroup(direction);
    return adjacent.isValid() && setActiveGroup(adjacent);
}

bool ZzSplitWorkspace::transferTab(
    const ZzTabGroupId &source,
    int sourceIndex,
    const ZzTabGroupId &target,
    int targetIndex)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    QPointer<ZzTabWidget> guardedSource = tabWidget(source);
    QPointer<ZzTabWidget> guardedTarget = tabWidget(target);
    if (guardedSource.isNull() || guardedTarget.isNull()
        || sourceIndex < 0 || sourceIndex >= guardedSource->count()) {
        return false;
    }
    QPointer<QWidget> guardedPage = guardedSource->widget(sourceIndex);
    if (guardedPage.isNull()) {
        return false;
    }

    const bool transferred = guardedSource->transferTabTo(
        guardedTarget, sourceIndex, targetIndex);
    if (guardedWorkspace.isNull()) {
        return transferred;
    }
    if (!transferred || guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull()) {
        return false;
    }

    ZzTabWidget *const resolvedTarget = tabWidget(target);
    return resolvedTarget == guardedTarget
        && resolvedTarget->indexOf(guardedPage) >= 0;
}

bool ZzSplitWorkspace::moveTabToDropZone(
    const ZzTabGroupId &source,
    int sourceIndex,
    const ZzTabGroupId &target,
    ZzWorkspaceDropZone zone)
{
    switch (zone) {
    case ZzWorkspaceDropZone::Center:
    case ZzWorkspaceDropZone::Left:
    case ZzWorkspaceDropZone::Top:
    case ZzWorkspaceDropZone::Right:
    case ZzWorkspaceDropZone::Bottom:
        break;
    default:
        return false;
    }

    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    QPointer<ZzTabWidget> guardedSource = tabWidget(source);
    QPointer<ZzTabWidget> guardedTarget = tabWidget(target);
    if (guardedSource.isNull() || guardedTarget.isNull()
        || sourceIndex < 0 || sourceIndex >= guardedSource->count()) {
        return false;
    }
    QPointer<QWidget> guardedPage = guardedSource->widget(sourceIndex);
    if (guardedPage.isNull()) {
        return false;
    }

    if (zone == ZzWorkspaceDropZone::Center) {
        const bool transferred = guardedSource->transferTabTo(
            guardedTarget, sourceIndex);
        if (guardedWorkspace.isNull()) {
            return transferred;
        }
        if (!transferred) {
            return false;
        }
        ZzTabWidget *const resolvedTarget = tabWidget(target);
        if (guardedPage.isNull() || guardedTarget.isNull()
            || resolvedTarget != guardedTarget
            || resolvedTarget->indexOf(guardedPage) < 0) {
            return false;
        }
        Q_EMIT tabDropCommitted(source, target, zone, guardedPage);
        return true;
    }

    const Qt::Orientation orientation =
        zone == ZzWorkspaceDropZone::Left
            || zone == ZzWorkspaceDropZone::Right
        ? Qt::Horizontal
        : Qt::Vertical;
    ZzSplitPlacement placement =
        zone == ZzWorkspaceDropZone::Left
            || zone == ZzWorkspaceDropZone::Top
        ? ZzSplitPlacement::Before
        : ZzSplitPlacement::After;
    if (orientation == Qt::Horizontal
        && layoutDirection() == Qt::RightToLeft) {
        placement = placement == ZzSplitPlacement::Before
            ? ZzSplitPlacement::After
            : ZzSplitPlacement::Before;
    }

    const ZzTreeSnapshot snapshot = d_ptr->captureTreeSnapshot();
    const auto temporaryId = d_ptr->splitGroup(
        target, orientation, placement, {}, false);
    if (!temporaryId.has_value() || guardedWorkspace.isNull()) {
        return false;
    }

    guardedSource = tabWidget(source);
    QPointer<ZzTabWidget> temporaryTabs = tabWidget(temporaryId.value());
    const auto rollbackTemporary = [&]() {
        if (guardedWorkspace.isNull()) {
            return;
        }
        if (d_ptr->removeEmptyGroup(temporaryId.value(), false)) {
            delete temporaryTabs.data();
            return;
        }
        d_ptr->restoreTreeSnapshot(snapshot);
    };
    if (guardedSource.isNull() || temporaryTabs.isNull()
        || guardedPage.isNull()
        || guardedSource->indexOf(guardedPage) != sourceIndex
        || !guardedSource->transferTabTo(temporaryTabs, sourceIndex)) {
        rollbackTemporary();
        return false;
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }

    ZzTabWidget *const resolvedTemporary = tabWidget(temporaryId.value());
    if (guardedPage.isNull() || temporaryTabs.isNull()
        || resolvedTemporary != temporaryTabs
        || resolvedTemporary->indexOf(guardedPage) < 0) {
        rollbackTemporary();
        return false;
    }

    bool removedSource = false;
    if (ZzTabWidget *const resolvedSource = tabWidget(source);
        resolvedSource != nullptr && resolvedSource->count() == 0
        && source != temporaryId.value()) {
        removedSource = d_ptr->removeEmptyGroup(source);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    if (!removedSource) {
        d_ptr->rebuildView();
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }

    if (!setActiveGroup(temporaryId.value())) {
        return false;
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }

    Q_EMIT groupAdded(temporaryId.value());
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (removedSource) {
        Q_EMIT groupAboutToBeRemoved(source);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    Q_EMIT tabDropCommitted(
        source, temporaryId.value(), zone, guardedPage.data());
    if (guardedWorkspace.isNull()) {
        return true;
    }
    Q_EMIT layoutChanged();
    return true;
}

bool ZzSplitWorkspace::eventFilter(QObject *watched, QEvent *event)
{
    if (d_ptr->eventFilter(watched, event)) {
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ZzSplitWorkspace::dragEnterEvent(QDragEnterEvent *event)
{
    if (!d_ptr->handleDragEnter(this, event)) {
        QWidget::dragEnterEvent(event);
    }
}

void ZzSplitWorkspace::dragMoveEvent(QDragMoveEvent *event)
{
    if (!d_ptr->handleDragMove(this, event)) {
        QWidget::dragMoveEvent(event);
    }
}

void ZzSplitWorkspace::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (!d_ptr->handleDragLeave(event)) {
        QWidget::dragLeaveEvent(event);
    }
}

void ZzSplitWorkspace::dropEvent(QDropEvent *event)
{
    if (!d_ptr->handleDrop(this, event)) {
        QWidget::dropEvent(event);
    }
}

} // namespace ZzFluentUI
