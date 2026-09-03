#include <ZzFluentUI/ZzSplitWorkspace.h>

#include <QtCore/QPointer>
#include <QtCore/QEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>

#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTabBar.h>

#include "private/ZzSplitWorkspacePrivate.h"
#include "private/ZzWorkspaceCrossTransferTransactionPrivate.h"

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
    return d_ptr->transferTab(source, sourceIndex, target, targetIndex);
}

bool ZzSplitWorkspace::setPageLayoutKey(
    QWidget *page,
    const QString &key)
{
    return d_ptr->setPageLayoutKey(page, key);
}

QString ZzSplitWorkspace::pageLayoutKey(const QWidget *page) const
{
    return d_ptr->pageLayoutKey(page);
}

ZzWorkspacePageId ZzSplitWorkspace::pageId(const QWidget *page) const
{
    return d_ptr->pageId(page);
}

QWidget *ZzSplitWorkspace::pageForId(const ZzWorkspacePageId &id) const
{
    return d_ptr->pageForId(id);
}

ZzCore::ZzResult<void> ZzSplitWorkspace::transferTabToWorkspace(
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *targetWorkspace,
    const ZzTabGroupId &targetGroup,
    int targetIndex,
    ZzWorkspaceDropZone zone)
{
    return ZzWorkspaceCrossTransferTransactionPrivate::run(
        this,
        sourceGroup,
        sourceIndex,
        targetWorkspace,
        targetGroup,
        targetIndex,
        zone);
}

ZzCore::ZzResult<void> ZzSplitWorkspace::transferTabToWorkspaceSilently(
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *targetWorkspace,
    const ZzTabGroupId &targetGroup,
    int targetIndex)
{
    return ZzWorkspaceCrossTransferTransactionPrivate::run(
        this,
        sourceGroup,
        sourceIndex,
        targetWorkspace,
        targetGroup,
        targetIndex,
        ZzWorkspaceDropZone::Center,
        false);
}

bool ZzSplitWorkspace::restoreGroupOrderSilently(
    const ZzTabGroupId &group,
    const QList<QWidget *> &pages)
{
    QPointer<ZzTabWidget> tabs = tabWidget(group);
    if (tabs.isNull() || tabs->count() != pages.size()) {
        return false;
    }
    QPointer<ZzTabBar> bar = tabs->fluentTabBar();
    const bool previouslyBlocked = tabs->blockSignals(true);
    const auto restoreSignals = [&tabs, previouslyBlocked] {
        if (!tabs.isNull()) tabs->blockSignals(previouslyBlocked);
    };
    for (int desired = 0; desired < pages.size(); ++desired) {
        QPointer<QWidget> page = pages.at(desired);
        if (tabs.isNull() || bar.isNull() || page.isNull()) {
            restoreSignals();
            return false;
        }
        const int actual = tabs->indexOf(page);
        if (actual < 0) {
            restoreSignals();
            return false;
        }
        if (actual != desired) {
            bar->moveTab(actual, desired);
            if (tabs.isNull() || bar.isNull() || page.isNull()) {
                restoreSignals();
                return false;
            }
        }
    }
    for (int index = 0; index < pages.size(); ++index) {
        if (tabs.isNull() || tabs->widget(index) != pages.at(index)) {
            restoreSignals();
            return false;
        }
    }
    restoreSignals();
    return true;
}

QByteArray ZzSplitWorkspace::saveLayout() const
{
    return d_ptr->saveLayout();
}

bool ZzSplitWorkspace::restoreLayout(const QByteArray &state)
{
    const ZzTabGroupId previousActive = d_ptr->activeId;
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    if (!d_ptr->restoreLayout(state)) {
        return false;
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (previousActive != d_ptr->activeId) {
        Q_EMIT activeGroupChanged(d_ptr->activeId);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    Q_EMIT layoutChanged();
    return true;
}

ZzTabGroupId ZzSplitWorkspace::savedGroupForPageKey(
    const QString &key) const
{
    return d_ptr->savedGroupForPageKey(key);
}

bool ZzSplitWorkspace::moveTabToDropZone(
    const ZzTabGroupId &source,
    int sourceIndex,
    const ZzTabGroupId &target,
    ZzWorkspaceDropZone zone)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = this;
    const ZzWorkspaceTransferResult result = d_ptr->moveTabToDropZone(
        source, sourceIndex, target, zone);
    if (!result.committed || guardedWorkspace.isNull()) {
        return result.committed;
    }
    if (result.groupAdded) {
        Q_EMIT groupAdded(result.destinationId);
    }
    if (guardedWorkspace.isNull()) {
        return true;
    }
    if (result.sourceRemoved) {
        Q_EMIT groupAboutToBeRemoved(result.sourceId);
        if (guardedWorkspace.isNull()) {
            return true;
        }
    }
    if (result.layoutChanged) {
        Q_EMIT layoutChanged();
    }
    if (result.activeChanged) {
        Q_EMIT activeGroupChanged(result.destinationId);
        if (guardedWorkspace.isNull()) return true;
    }
    Q_EMIT tabDropCommitted(
        result.sourceId,
        result.destinationId,
        result.zone,
        result.page.data());
    if (guardedWorkspace.isNull()) {
        return true;
    }
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
