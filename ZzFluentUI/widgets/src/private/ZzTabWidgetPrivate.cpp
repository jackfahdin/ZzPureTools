#include "ZzTabWidgetPrivate.h"

#include <algorithm>

#include <QtCore/QPointer>

#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzFluentUI {

namespace {

/** @brief 在目标提交失败后恢复来源标签及完整公开元数据。 */
bool zzRollbackTransfer(
    ZzTabWidget *source,
    const ZzTabTransferSnapshot &transfer)
{
    if (source == nullptr || transfer.page.isNull()) {
        return false;
    }
    const int rollbackIndex = source->insertTab(
        std::clamp(transfer.sourceIndex, 0, source->count()),
        transfer.page,
        transfer.icon,
        transfer.text);
    if (rollbackIndex < 0) {
        return false;
    }
    ZzTabWidgetPrivate::restoreMetadata(source, rollbackIndex, transfer);
    source->setCurrentIndex(rollbackIndex);
    return true;
}

} // namespace

ZzTabWidgetPrivate::ZzTabWidgetPrivate(ZzTabWidget *q) noexcept
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

ZzTabWidgetPrivate::Metadata ZzTabWidgetPrivate::metadata(QWidget *page) const
{ return metadataByPage.value(page); }

ZzTabWidgetPrivate::Metadata &ZzTabWidgetPrivate::ensureMetadata(QWidget *page)
{
    auto &state = metadataByPage[page];
    if (page != nullptr && !state.destroyedConnection) {
        state.destroyedConnection = QObject::connect(page, &QObject::destroyed, q_ptr,
            [this](QObject *object) {
                metadataByPage.remove(static_cast<QWidget *>(object));
            });
    }
    return state;
}

void ZzTabWidgetPrivate::removeMetadata(QObject *object)
{ metadataByPage.remove(static_cast<QWidget *>(object)); }

void ZzTabWidgetPrivate::disconnectMetadataObservers() noexcept
{
    for (auto it = metadataByPage.begin(); it != metadataByPage.end(); ++it) {
        QObject::disconnect(it->destroyedConnection);
    }
    metadataByPage.clear();
}

void ZzTabWidgetPrivate::normalizePinnedOrder()
{
    if (normalizing) return;
    normalizing = true;
    int pinnedEnd = 0;
    for (int i = 0; i < q_ptr->count(); ++i) {
        if (metadata(q_ptr->widget(i)).pinned) {
            if (i != pinnedEnd) tabBar->moveTab(i, pinnedEnd);
            ++pinnedEnd;
        }
    }
    normalizing = false;
}

ZzTabTransferSnapshot ZzTabWidgetPrivate::snapshot(int index) const
{
    ZzTabTransferSnapshot result;
    if (index < 0 || index >= q_ptr->count()) {
        return result;
    }

    result.page = q_ptr->widget(index);
    result.text = q_ptr->tabText(index);
    result.icon = q_ptr->tabIcon(index);
    result.toolTip = q_ptr->tabToolTip(index);
    result.whatsThis = q_ptr->tabWhatsThis(index);
    result.enabled = q_ptr->isTabEnabled(index);
    result.data = tabBar->tabData(index);
    result.textColor = tabBar->tabTextColor(index);
    result.sourceIndex = index;
    const Metadata state = metadata(result.page);
    result.pinned = state.pinned;
    result.modified = state.modified;
    result.attention = state.attention;
    result.closeEnabled = state.closeEnabled;
    return result;
}

void ZzTabWidgetPrivate::restoreMetadata(
    ZzTabWidget *target,
    int index,
    const ZzTabTransferSnapshot &snapshotValue)
{
    if (target == nullptr || index < 0 || index >= target->count()) {
        return;
    }
    target->setTabToolTip(index, snapshotValue.toolTip);
    target->setTabWhatsThis(index, snapshotValue.whatsThis);
    target->setTabEnabled(index, snapshotValue.enabled);
    target->fluentTabBar()->setTabData(index, snapshotValue.data);
    target->fluentTabBar()->setTabTextColor(index, snapshotValue.textColor);
    auto &state = target->d_ptr->ensureMetadata(target->widget(index));
    state.pinned = snapshotValue.pinned;
    state.modified = snapshotValue.modified;
    state.attention = snapshotValue.attention;
    state.closeEnabled = snapshotValue.closeEnabled;
}

bool ZzTabWidgetPrivate::transferTo(
    ZzTabWidget *target,
    int sourceIndex,
    int targetIndex)
{
    if (target == nullptr || sourceIndex < 0
        || sourceIndex >= q_ptr->count()
        || !target->fluentTabBar()->isTabTransferEnabled()) {
        return false;
    }

    const ZzTabTransferSnapshot transfer = snapshot(sourceIndex);
    if (transfer.page.isNull()) {
        return false;
    }

    int requestedSlot = targetIndex < 0
        ? target->count()
        : std::clamp(targetIndex, 0, target->count());
    const int pinnedCount = [&] { int n=0; for(int i=0;i<target->count();++i) if(target->isTabPinned(i)) ++n; return n; }();
    requestedSlot = transfer.pinned ? std::clamp(requestedSlot, 0, pinnedCount) : std::clamp(requestedSlot, pinnedCount, target->count());
    if (target == q_ptr) {
        int finalIndex = requestedSlot;
        if (finalIndex > sourceIndex) {
            --finalIndex;
        }
        finalIndex = std::clamp(finalIndex, 0, q_ptr->count() - 1);
        if (finalIndex != sourceIndex) {
            tabBar->moveTab(sourceIndex, finalIndex);
        }
        normalizePinnedOrder();
        return true;
    }

    if (target->indexOf(transfer.page) >= 0) {
        return false;
    }

    QPointer<ZzTabWidget> guardedSource = q_ptr;
    QPointer<ZzTabWidget> guardedTarget = target;
    QPointer<QWidget> guardedPage = transfer.page;
    q_ptr->removeTab(sourceIndex);
    if (auto it = metadataByPage.find(transfer.page); it != metadataByPage.end()) {
        QObject::disconnect(it->destroyedConnection);
        metadataByPage.erase(it);
    }
    if (guardedTarget.isNull() || guardedPage.isNull()) {
        zzRollbackTransfer(guardedSource, transfer);
        return false;
    }

    const int insertedIndex = guardedTarget->insertTab(
        requestedSlot,
        guardedPage,
        transfer.icon,
        transfer.text);
    if (insertedIndex < 0 || guardedTarget.isNull()
        || guardedPage.isNull()) {
        zzRollbackTransfer(guardedSource, transfer);
        return false;
    }

    restoreMetadata(guardedTarget, insertedIndex, transfer);
    guardedTarget->d_ptr->normalizePinnedOrder();
    guardedTarget->setCurrentIndex(insertedIndex);
    Q_EMIT guardedTarget->tabTransferred(
        guardedSource,
        transfer.sourceIndex,
        insertedIndex,
        guardedPage);
    return true;
}

} // namespace ZzFluentUI
