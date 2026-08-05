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

    const int requestedSlot = targetIndex < 0
        ? target->count()
        : std::clamp(targetIndex, 0, target->count());
    if (target == q_ptr) {
        int finalIndex = requestedSlot;
        if (finalIndex > sourceIndex) {
            --finalIndex;
        }
        finalIndex = std::clamp(finalIndex, 0, q_ptr->count() - 1);
        if (finalIndex != sourceIndex) {
            tabBar->moveTab(sourceIndex, finalIndex);
        }
        return true;
    }

    if (target->indexOf(transfer.page) >= 0) {
        return false;
    }

    QPointer<ZzTabWidget> guardedSource = q_ptr;
    QPointer<ZzTabWidget> guardedTarget = target;
    QPointer<QWidget> guardedPage = transfer.page;
    q_ptr->removeTab(sourceIndex);
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
    guardedTarget->setCurrentIndex(insertedIndex);
    Q_EMIT guardedTarget->tabTransferred(
        guardedSource,
        transfer.sourceIndex,
        insertedIndex,
        guardedPage);
    return true;
}

} // namespace ZzFluentUI
