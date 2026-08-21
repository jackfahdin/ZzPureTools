#include "ZzTabWidgetPrivate.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QSignalBlocker>

#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzFluentUI {

namespace {

/** @brief 返回页面父对象链中的标签容器，用于识别外部接管。 */
QTabWidget *zzOwningTabWidget(QWidget *page)
{
    for (QObject *current = page != nullptr ? page->parent() : nullptr;
         current != nullptr;
         current = current->parent()) {
        if (auto *tabs = qobject_cast<QTabWidget *>(current);
            tabs != nullptr) {
            return tabs;
        }
    }
    return nullptr;
}

} // namespace

ZzTabWidgetPrivate::ZzTabWidgetPrivate(ZzTabWidget *q) noexcept
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

ZzTabWidgetPrivate::ZzMetadata ZzTabWidgetPrivate::metadata(QWidget *page) const
{
    return metadataByPage.value(page);
}

ZzTabWidgetPrivate::ZzMetadata &ZzTabWidgetPrivate::ensureMetadata(QWidget *page)
{
    auto &state = metadataByPage[page];
    if (page != nullptr && !state.destroyedConnection) {
        state.destroyedConnection = QObject::connect(
            page,
            &QObject::destroyed,
            q_ptr,
            [this](QObject *object) {
                metadataByPage.remove(static_cast<QWidget *>(object));
            });
    }
    return state;
}

void ZzTabWidgetPrivate::removeMetadata(QObject *object)
{
    auto it = metadataByPage.find(static_cast<QWidget *>(object));
    if (it == metadataByPage.end()) {
        return;
    }
    QObject::disconnect(it->destroyedConnection);
    metadataByPage.erase(it);
}

void ZzTabWidgetPrivate::disconnectMetadataObservers() noexcept
{
    for (auto it = metadataByPage.begin(); it != metadataByPage.end(); ++it) {
        QObject::disconnect(it->destroyedConnection);
    }
    metadataByPage.clear();
}

void ZzTabWidgetPrivate::normalizePinnedOrder()
{
    if (normalizing) {
        return;
    }
    QPointer<ZzTabWidget> guardedWidget = q_ptr;
    normalizing = true;
    int pinnedEnd = 0;
    for (int i = 0; i < guardedWidget->count(); ++i) {
        if (metadata(guardedWidget->widget(i)).pinned) {
            if (i != pinnedEnd) {
                QPointer<ZzTabBar> guardedTabBar = tabBar;
                {
                    const QSignalBlocker blocker(guardedTabBar);
                    guardedTabBar->moveTab(i, pinnedEnd);
                }
                QMetaObject::invokeMethod(
                    guardedTabBar,
                    "tabMoved",
                    Qt::DirectConnection,
                    Q_ARG(int, i),
                    Q_ARG(int, pinnedEnd));
                if (guardedWidget.isNull()
                    || guardedTabBar.isNull()) {
                    return;
                }
            }
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
    const ZzMetadata state = metadata(result.page);
    result.pinned = state.pinned;
    result.modified = state.modified;
    result.attention = state.attention;
    result.closeEnabled = state.closeEnabled;
    return result;
}

bool ZzTabWidgetPrivate::restoreMetadata(
    ZzTabWidget *target,
    int index,
    const ZzTabTransferSnapshot &snapshotValue)
{
    QPointer<ZzTabWidget> guardedTarget = target;
    QPointer<QWidget> guardedPage = snapshotValue.page;
    if (guardedTarget.isNull() || guardedPage.isNull()) {
        return false;
    }

    const auto resolveIndex = [&]() {
        if (guardedTarget.isNull() || guardedPage.isNull()) {
            return -1;
        }
        return guardedTarget->indexOf(guardedPage);
    };
    index = resolveIndex();
    if (index < 0) {
        return false;
    }
    guardedTarget->setTabText(index, snapshotValue.text);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTarget->setTabIcon(index, snapshotValue.icon);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTarget->setTabToolTip(index, snapshotValue.toolTip);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTarget->setTabWhatsThis(index, snapshotValue.whatsThis);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTarget->setTabEnabled(index, snapshotValue.enabled);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTarget->fluentTabBar()->setTabData(index, snapshotValue.data);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    guardedTarget->fluentTabBar()->setTabTextColor(
        index,
        snapshotValue.textColor);
    if ((index = resolveIndex()) < 0) {
        return false;
    }
    auto &state = guardedTarget->d_ptr->ensureMetadata(guardedPage);
    state.pinned = snapshotValue.pinned;
    state.modified = snapshotValue.modified;
    state.attention = snapshotValue.attention;
    state.closeEnabled = snapshotValue.closeEnabled;
    return resolveIndex() >= 0;
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
    int pinnedCount = 0;
    for (int index = 0; index < target->count(); ++index) {
        if (target->isTabPinned(index)) {
            ++pinnedCount;
        }
    }
    if (target == q_ptr) {
        requestedSlot = transfer.pinned
            ? std::clamp(requestedSlot, 0, pinnedCount)
            : std::clamp(
                requestedSlot,
                pinnedCount,
                target->count());
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
    const auto rollback = [&]() {
        if (guardedSource.isNull() || guardedPage.isNull()) {
            return false;
        }

        int sourcePageIndex = guardedSource->indexOf(guardedPage);
        if (sourcePageIndex < 0 && !guardedTarget.isNull()) {
            const int targetPageIndex =
                guardedTarget->indexOf(guardedPage);
            if (targetPageIndex >= 0) {
                guardedTarget->d_ptr->removeMetadata(guardedPage);
                guardedTarget->removeTab(targetPageIndex);
                if (guardedSource.isNull() || guardedPage.isNull()) {
                    return false;
                }
                if (guardedTarget.isNull()
                    || guardedTarget->indexOf(guardedPage) >= 0) {
                    return false;
                }
            }
        }

        sourcePageIndex = guardedSource->indexOf(guardedPage);
        if (sourcePageIndex < 0) {
            QTabWidget *const owner = zzOwningTabWidget(guardedPage);
            if (owner != nullptr && owner != guardedSource
                && owner != guardedTarget) {
                return false;
            }
            ++guardedSource->d_ptr->transferInsertionDepth;
            guardedSource->insertTab(
                std::clamp(
                    transfer.sourceIndex,
                    0,
                    guardedSource->count()),
                guardedPage,
                transfer.icon,
                transfer.text);
            if (!guardedSource.isNull()) {
                --guardedSource->d_ptr->transferInsertionDepth;
            }
            if (guardedSource.isNull() || guardedPage.isNull()
                || guardedSource->indexOf(guardedPage) < 0) {
                return false;
            }
        }

        sourcePageIndex = guardedSource->indexOf(guardedPage);
        if (!restoreMetadata(
                guardedSource,
                sourcePageIndex,
                transfer)) {
            return false;
        }
        guardedSource->d_ptr->normalizePinnedOrder();
        if (guardedSource.isNull() || guardedPage.isNull()) {
            return false;
        }
        sourcePageIndex = guardedSource->indexOf(guardedPage);
        if (sourcePageIndex < 0) {
            return false;
        }
        guardedSource->setCurrentIndex(sourcePageIndex);
        return !guardedSource.isNull() && !guardedPage.isNull()
            && guardedSource->indexOf(guardedPage) >= 0;
    };

    removeMetadata(transfer.page);
    q_ptr->removeTab(sourceIndex);
    if (guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull()) {
        rollback();
        return false;
    }
    QTabWidget *const ownerAfterRemoval =
        zzOwningTabWidget(guardedPage);
    if (guardedSource->indexOf(guardedPage) >= 0
        || (ownerAfterRemoval != nullptr
            && ownerAfterRemoval != guardedSource)) {
        rollback();
        return false;
    }

    ++guardedTarget->d_ptr->transferInsertionDepth;
    const int insertedIndex = guardedTarget->insertTab(
        requestedSlot,
        guardedPage,
        transfer.icon,
        transfer.text);
    if (!guardedTarget.isNull()) {
        --guardedTarget->d_ptr->transferInsertionDepth;
    }
    if (insertedIndex < 0 || guardedSource.isNull()
        || guardedTarget.isNull() || guardedPage.isNull()
        || guardedTarget->indexOf(guardedPage) < 0) {
        rollback();
        return false;
    }

    if (!restoreMetadata(guardedTarget, insertedIndex, transfer)
        || guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull()) {
        rollback();
        return false;
    }
    guardedTarget->d_ptr->normalizePinnedOrder();
    if (guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull()) {
        rollback();
        return false;
    }
    const int actualTargetIndex = guardedTarget->indexOf(guardedPage);
    if (actualTargetIndex < 0) {
        rollback();
        return false;
    }
    guardedTarget->setCurrentIndex(actualTargetIndex);
    if (guardedSource.isNull() || guardedTarget.isNull()
        || guardedPage.isNull()) {
        rollback();
        return false;
    }
    const int committedTargetIndex =
        guardedTarget->indexOf(guardedPage);
    if (committedTargetIndex < 0) {
        rollback();
        return false;
    }
    Q_EMIT guardedTarget->tabTransferred(
        guardedSource,
        transfer.sourceIndex,
        committedTargetIndex,
        guardedPage);
    return true;
}

} // namespace ZzFluentUI
