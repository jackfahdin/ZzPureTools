#include "ZzWorkspaceWindowCoordinatorPrivate.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <string_view>
#include <utility>

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QScopeGuard>
#include <QtCore/QThread>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzLog/ZzLog.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzCoordinatorFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<ZzValue>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

struct ZzGroupSnapshot final
{
    QUuid windowId;
    ZzFluentUI::ZzTabGroupId group;
    QList<QPointer<QWidget>> pages;
};

struct ZzReclaimMove final
{
    QPointer<QWidget> page;
    ZzFluentUI::ZzWorkspacePageId pageId;
    ZzFluentUI::ZzTabGroupId sourceGroup;
    int sourceIndex = -1;
    QUuid targetWindowId;
    ZzFluentUI::ZzTabGroupId targetGroup;
    int targetIndex = -1;
    quint64 originSequence = 0;
    bool pinned = false;
};

struct ZzTargetPlan final
{
    ZzGroupSnapshot snapshot;
    QList<QPointer<QWidget>> desiredPages;
    QList<bool> desiredPinned;
};

class ZzScopedSignalMute final
{
public:
    explicit ZzScopedSignalMute(
        const std::vector<QPointer<QObject>> &objects)
    {
        entries_.reserve(objects.size());
        for (const auto &object : objects) {
            if (object.isNull()
                || std::any_of(entries_.cbegin(),
                    entries_.cend(),
                    [&object](const ZzEntry &entry) {
                        return entry.object == object;
                    })) {
                continue;
            }
            entries_.push_back({object, object->signalsBlocked()});
            object->blockSignals(true);
        }
    }

    ~ZzScopedSignalMute()
    {
        for (auto iterator = entries_.rbegin();
             iterator != entries_.rend();
             ++iterator) {
            if (!iterator->object.isNull()) {
                iterator->object->blockSignals(iterator->previouslyBlocked);
            }
        }
    }

    Q_DISABLE_COPY_MOVE(ZzScopedSignalMute)

private:
    struct ZzEntry final
    {
        QPointer<QObject> object;
        bool previouslyBlocked = false;
    };

    std::vector<ZzEntry> entries_;
};

[[nodiscard]] QList<QPointer<QWidget>> zzGroupPages(
    ZzFluentUI::ZzSplitWorkspace *workspace,
    const ZzFluentUI::ZzTabGroupId &group)
{
    QList<QPointer<QWidget>> pages;
    auto *const tabs = workspace != nullptr
                           ? workspace->tabWidget(group)
                           : nullptr;
    for (int index = 0; tabs != nullptr && index < tabs->count(); ++index) {
        pages.push_back(tabs->widget(index));
    }
    return pages;
}

[[nodiscard]] QList<QWidget *> zzRawPages(
    const QList<QPointer<QWidget>> &guardedPages,
    bool *complete = nullptr)
{
    QList<QWidget *> pages;
    pages.reserve(guardedPages.size());
    bool allAlive = true;
    for (const auto &page : guardedPages) {
        if (page.isNull()) {
            allAlive = false;
            continue;
        }
        pages.push_back(page.data());
    }
    if (complete != nullptr) *complete = allAlive;
    return pages;
}

[[nodiscard]] ZzFluentUI::ZzTabGroupId zzPageGroup(
    ZzFluentUI::ZzSplitWorkspace *workspace,
    QWidget *page)
{
    if (workspace == nullptr || page == nullptr) return {};
    for (const auto &group : workspace->groupIds()) {
        auto *const tabs = workspace->tabWidget(group);
        if (tabs != nullptr && tabs->indexOf(page) >= 0) return group;
    }
    return {};
}

} // namespace

ZzWorkspaceWindowCoordinatorPrivate::ZzWorkspaceWindowCoordinatorPrivate(
    ZzWorkspaceWindowCoordinator *publicObject)
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzWorkspaceWindowCoordinatorPrivate::removeRecord(
    std::size_t index) noexcept
{
    if (index >= records.size()) return;

    auto &record = records.at(index);
    QObject::disconnect(record.windowDestroyedConnection);
    QObject::disconnect(record.shellDestroyedConnection);
    QObject::disconnect(record.tearOffConnection);
    QObject::disconnect(record.transferConnection);
    if (record.window) record.window->removeEventFilter(q_ptr);
    if (record.shell) record.shell->removeEventFilter(q_ptr);
    records.erase(records.begin() + static_cast<std::ptrdiff_t>(index));
    pageOrigins.erase(std::remove_if(
        pageOrigins.begin(),
        pageOrigins.end(),
        [](const ZzPageOrigins &entry) { return entry.page.isNull(); }),
        pageOrigins.end());
}

void ZzWorkspaceWindowCoordinatorPrivate::removeRecordForObject(
    const QObject *object) noexcept
{
    const auto iterator = std::find_if(records.begin(),
        records.end(),
        [object](const ZzWindowRecord &record) {
            return record.windowIdentity == object
                || record.shellIdentity == object;
        });
    if (iterator != records.end()) {
        removeRecord(static_cast<std::size_t>(
            std::distance(records.begin(), iterator)));
    }
}

void ZzWorkspaceWindowCoordinatorPrivate::clear() noexcept
{
    while (!records.empty()) removeRecord(records.size() - 1);
}

void ZzWorkspaceWindowCoordinatorPrivate::recordTransfer(
    ZzApplicationWindow *targetWindow,
    ZzFluentUI::ZzSplitWorkspace *sourceWorkspace,
    const ZzFluentUI::ZzTabGroupId &sourceGroup,
    int sourceIndex,
    QWidget *page,
    const ZzFluentUI::ZzWorkspacePageId &pageId)
{
    QElapsedTimer auditTimer;
    auditTimer.start();
    if (sourceWorkspace == nullptr || page == nullptr) return;
    const auto source = std::find_if(records.cbegin(),
        records.cend(),
        [sourceWorkspace](const ZzWindowRecord &record) {
            return record.shell
                && record.shell->splitWorkspace() == sourceWorkspace;
        });
    const auto target = std::find_if(records.cbegin(),
        records.cend(),
        [targetWindow](const ZzWindowRecord &record) {
            return record.windowIdentity == targetWindow;
        });
    if (source == records.cend() || target == records.cend()) return;
    auto history = std::find_if(pageOrigins.begin(),
        pageOrigins.end(),
        [page](const ZzPageOrigins &value) { return value.page == page; });
    if (history == pageOrigins.end()) {
        pageOrigins.push_back({page, {}});
        history = std::prev(pageOrigins.end());
    }
    const auto existing = std::find_if(history->origins.cbegin(),
        history->origins.cend(),
        [&target](const ZzPageOrigin &origin) {
            return origin.windowId == target->windowId;
        });
    if (existing != history->origins.cend()) {
        history->origins.erase(existing, history->origins.end());
    } else {
        history->origins.push_back({source->windowId,
            sourceGroup,
            sourceIndex,
            nextTransferSequence++});
    }
    writePageAudit(QStringLiteral("page.transfer"),
        pageId,
        source->windowId,
        target->windowId,
        QStringLiteral("commit"),
        auditTimer,
        true);
}

bool ZzWorkspaceWindowCoordinatorPrivate::closeTransactionActive(
    const ZzWindowRecord &record) const noexcept
{
    return record.closeState == ZzCloseState::Reclaiming
        || record.closeState == ZzCloseState::DispatchingClose
        || record.closeState == ZzCloseState::NotifyingClose
        || record.closeState == ZzCloseState::CloseAccepted;
}

void ZzWorkspaceWindowCoordinatorPrivate::invalidateCloseRequest(
    ZzWindowRecord &record) noexcept
{
    if (record.closeState == ZzCloseState::DelegatePending
        || record.closeState == ZzCloseState::DelegateApproved) {
        record.closeState = ZzCloseState::Idle;
    }
    record.internalCloseDispatch = false;
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinatorPrivate::closeWindow(
    ZzApplicationWindow *window,
    bool currentCloseEvent)
{
    if (QThread::currentThread() != q_ptr->thread() || shuttingDown) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting close requests"));
    }
    const auto findWindow = [this](ZzApplicationWindow *candidate) {
        return std::find_if(records.begin(),
            records.end(),
            [candidate](const ZzWindowRecord &record) {
                return record.windowIdentity == candidate;
            });
    };
    const auto findWindowId = [this](const QUuid &windowId) {
        return std::find_if(records.begin(),
            records.end(),
            [&windowId](const ZzWindowRecord &record) {
                return record.windowId == windowId;
            });
    };
    auto record = findWindow(window);
    if (window == nullptr || record == records.end()) {
        return zzCoordinatorFailure<void>(
            window == nullptr ? ZzCore::ZzErrorCode::InvalidArgument
                              : ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("workspace window is not registered"));
    }
    if (closeTransactionActive(*record)
        && !(currentCloseEvent
            && record->closeState == ZzCloseState::DispatchingClose)) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window close transaction is already active"));
    }
    if (record->configuration.closePolicy == ZzWindowClosePolicy::Deny) {
        invalidateCloseRequest(*record);
        return ZzCore::ZzResult<void>::success();
    }
    if (record->configuration.closePolicy == ZzWindowClosePolicy::Delegate
        && record->closeState != ZzCloseState::DelegateApproved) {
        if (record->closeState == ZzCloseState::Idle) {
            record->closeState = ZzCloseState::DelegatePending;
            Q_EMIT q_ptr->windowCloseApprovalRequested(window);
        }
        return ZzCore::ZzResult<void>::success();
    }

    if (!currentCloseEvent) {
        const ZzCloseState dispatchState = record->closeState;
        if (dispatchState != ZzCloseState::DelegateApproved) {
            record->closeState = ZzCloseState::DispatchingClose;
        }
        record->internalCloseDispatch = true;
        const bool accepted = record->window->close();
        record = findWindow(window);
        if (record != records.end()) {
            record->internalCloseDispatch = false;
            if (!accepted && (record->closeState == ZzCloseState::DispatchingClose
                || record->closeState == ZzCloseState::DelegateApproved)) {
                record->closeState = ZzCloseState::Idle;
            }
        }
        if (!accepted) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window rejected close"));
        }
        return ZzCore::ZzResult<void>::success();
    }

    if (record->closeState == ZzCloseState::DispatchingClose) {
        record->internalCloseDispatch = false;
    }

    const QUuid closingWindowId = record->windowId;
    const QPointer<ZzApplicationWindow> guardedWindow = record->window;
    const QPointer<ZzFluentUI::ZzSplitWorkspace> sourceWorkspace =
        record->shell ? record->shell->splitWorkspace() : nullptr;
    record->closeState = ZzCloseState::Reclaiming;
    const auto resetState = [&] {
        auto current = findWindowId(closingWindowId);
        if (current != records.end()) {
            current->internalCloseDispatch = false;
            current->closeState = ZzCloseState::Idle;
        }
    };
    if (sourceWorkspace.isNull()) {
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window has no live split workspace"));
    }

    std::vector<ZzGroupSnapshot> sourceSnapshots;
    QList<QPointer<QWidget>> guardedPages;
    try {
        const auto groups = sourceWorkspace->groupIds();
        sourceSnapshots.reserve(static_cast<std::size_t>(groups.size()));
        for (const auto &group : groups) {
            auto pages = zzGroupPages(sourceWorkspace, group);
            guardedPages.append(pages);
            sourceSnapshots.push_back(
                {closingWindowId, group, std::move(pages)});
        }
    } catch (...) {
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to capture page reclaim source state"));
    }

    const bool hasFallback = std::any_of(records.cbegin(),
        records.cend(),
        [window](const ZzWindowRecord &candidate) {
            return candidate.windowIdentity != window
                && !candidate.window.isNull() && !candidate.shell.isNull()
                && candidate.shell->splitWorkspace() != nullptr;
        });
    const auto reportOrphans = [&] {
        auto current = findWindowId(closingWindowId);
        if (current != records.end()) {
            current->closeState = ZzCloseState::NotifyingClose;
        }
        Q_EMIT q_ptr->orphanedPages(zzRawPages(guardedPages));
        resetState();
        return ZzCore::ZzResult<void>::success();
    };
    if (!guardedPages.isEmpty() && !hasFallback) return reportOrphans();

    std::vector<ZzReclaimMove> moves;
    try {
        moves.reserve(static_cast<std::size_t>(guardedPages.size()));
        for (const auto &sourceSnapshot : sourceSnapshots) {
            for (int sourceIndex = 0;
                 sourceIndex < sourceSnapshot.pages.size();
                 ++sourceIndex) {
                const auto page = sourceSnapshot.pages.at(sourceIndex);
                auto target = records.end();
                ZzFluentUI::ZzTabGroupId targetGroup;
                int targetIndex = -1;
                quint64 originSequence = 0;
                const auto history = std::find_if(pageOrigins.cbegin(),
                    pageOrigins.cend(),
                    [&page](const ZzPageOrigins &value) {
                        return value.page == page;
                    });
                if (history != pageOrigins.cend()) {
                    for (auto origin = history->origins.rbegin();
                         origin != history->origins.rend();
                         ++origin) {
                        const auto candidate = std::find_if(records.begin(),
                            records.end(),
                            [&origin, window](const ZzWindowRecord &value) {
                                return value.windowId == origin->windowId
                                    && value.windowIdentity != window
                                    && !value.window.isNull()
                                    && !value.shell.isNull()
                                    && value.shell->splitWorkspace() != nullptr;
                            });
                        if (candidate == records.end()) continue;
                        target = candidate;
                        auto *const targetWorkspace =
                            candidate->shell->splitWorkspace();
                        if (targetWorkspace->tabWidget(origin->group)
                            != nullptr) {
                            targetGroup = origin->group;
                            targetIndex = origin->index;
                            originSequence = origin->sequence;
                        } else {
                            targetGroup = targetWorkspace->activeGroupId();
                        }
                        break;
                    }
                }
                if (target == records.end()) {
                    const auto primary = std::find_if(records.begin(),
                        records.end(),
                        [window](const ZzWindowRecord &value) {
                            return value.primary
                                && value.windowIdentity != window
                                && !value.window.isNull()
                                && !value.shell.isNull()
                                && value.shell->splitWorkspace() != nullptr;
                        });
                    target = primary != records.end()
                        ? primary
                        : std::find_if(records.begin(),
                              records.end(),
                              [window](const ZzWindowRecord &value) {
                                  return value.windowIdentity != window
                                      && !value.window.isNull()
                                      && !value.shell.isNull()
                                      && value.shell->splitWorkspace() != nullptr;
                              });
                    if (target != records.end()) {
                        targetGroup =
                            target->shell->splitWorkspace()->activeGroupId();
                    }
                }
                if (target == records.end()) return reportOrphans();
                const auto pageId = !page.isNull()
                    ? sourceWorkspace->pageId(page)
                    : ZzFluentUI::ZzWorkspacePageId {};
                auto *const sourceTabs =
                    sourceWorkspace->tabWidget(sourceSnapshot.group);
                const int currentIndex = sourceTabs != nullptr
                    && !page.isNull()
                    ? sourceTabs->indexOf(page)
                    : -1;
                moves.push_back({page,
                    pageId,
                    sourceSnapshot.group,
                    sourceIndex,
                    target->windowId,
                    targetGroup,
                    targetIndex,
                    originSequence,
                    currentIndex >= 0
                        && sourceTabs->isTabPinned(currentIndex)});
            }
        }
    } catch (...) {
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to plan page reclaim transaction"));
    }

    std::vector<ZzTargetPlan> targetPlans;
    try {
        for (std::size_t moveIndex = 0;
             moveIndex < moves.size();
             ++moveIndex) {
            const auto &move = moves.at(moveIndex);
            const auto target = findWindowId(move.targetWindowId);
            auto *const targetWorkspace =
                target != records.end() && target->shell
                    ? target->shell->splitWorkspace()
                    : nullptr;
            auto *const sourceTabs = sourceWorkspace->tabWidget(
                move.sourceGroup);
            auto *const targetTabs = targetWorkspace != nullptr
                ? targetWorkspace->tabWidget(move.targetGroup)
                : nullptr;
            if (move.page.isNull() || !move.pageId.isValid()
                || sourceTabs == nullptr
                || sourceTabs->indexOf(move.page) < 0
                || targetTabs == nullptr) {
                resetState();
                return zzCoordinatorFailure<void>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("page reclaim target capacity changed"));
            }
            if (targetWorkspace->pageForId(move.pageId) != nullptr) {
                resetState();
                return zzCoordinatorFailure<void>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("page reclaim identity already exists"));
            }
            const QString layoutKey = sourceWorkspace->pageLayoutKey(move.page);
            if (!layoutKey.isEmpty()) {
                bool keyExists = false;
                for (const auto &group : targetWorkspace->groupIds()) {
                    auto *const tabs = targetWorkspace->tabWidget(group);
                    for (int index = 0;
                         tabs != nullptr && index < tabs->count();
                         ++index) {
                        if (targetWorkspace->pageLayoutKey(tabs->widget(index))
                            == layoutKey) {
                            keyExists = true;
                            break;
                        }
                    }
                    if (keyExists) break;
                }
                if (keyExists) {
                    resetState();
                    return zzCoordinatorFailure<void>(
                        ZzCore::ZzErrorCode::InvalidState,
                        QStringLiteral("page reclaim layout key already exists"));
                }
            }
            const auto additions = static_cast<int>(std::count_if(
                moves.cbegin(),
                moves.cbegin() + static_cast<std::ptrdiff_t>(moveIndex),
                [&move](const ZzReclaimMove &planned) {
                    return planned.targetWindowId == move.targetWindowId
                        && planned.targetGroup == move.targetGroup;
                }));
            if (targetTabs->count()
                > std::numeric_limits<int>::max() - additions - 1) {
                resetState();
                return zzCoordinatorFailure<void>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("page reclaim target capacity exceeded"));
            }
            const auto plan = std::find_if(targetPlans.begin(),
                targetPlans.end(),
                [&move](const ZzTargetPlan &value) {
                    return value.snapshot.windowId == move.targetWindowId
                        && value.snapshot.group == move.targetGroup;
                });
            if (plan == targetPlans.end()) {
                auto pages = zzGroupPages(targetWorkspace, move.targetGroup);
                QList<bool> pinned;
                pinned.reserve(pages.size());
                for (int index = 0; index < pages.size(); ++index) {
                    pinned.push_back(targetTabs->isTabPinned(index));
                }
                targetPlans.push_back({
                    {move.targetWindowId, move.targetGroup, pages},
                    pages,
                    pinned});
            }
        }
        for (auto &plan : targetPlans) {
            std::vector<const ZzReclaimMove *> historical;
            for (const auto &move : moves) {
                if (move.targetWindowId == plan.snapshot.windowId
                    && move.targetGroup == plan.snapshot.group
                    && move.targetIndex >= 0) {
                    historical.push_back(&move);
                }
            }
            std::stable_sort(historical.begin(),
                historical.end(),
                [](const ZzReclaimMove *left,
                    const ZzReclaimMove *right) {
                    return left->originSequence > right->originSequence;
                });
            for (const auto *move : historical) {
                const int index = std::clamp(move->targetIndex,
                    0,
                    static_cast<int>(plan.desiredPages.size()));
                plan.desiredPages.insert(index, move->page);
                plan.desiredPinned.insert(index, move->pinned);
            }
            for (const auto &move : moves) {
                if (move.targetWindowId == plan.snapshot.windowId
                    && move.targetGroup == plan.snapshot.group
                    && move.targetIndex < 0) {
                    const int index = move.pinned
                        ? static_cast<int>(std::count(
                              plan.desiredPinned.cbegin(),
                              plan.desiredPinned.cend(),
                              true))
                        : static_cast<int>(plan.desiredPages.size());
                    plan.desiredPages.insert(index, move.page);
                    plan.desiredPinned.insert(index, move.pinned);
                }
            }
        }
    } catch (...) {
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to capture page reclaim target state"));
    }

    bool notificationAuditOk = true;
    {
        std::vector<QPointer<ZzFluentUI::ZzSplitWorkspace>> workspaces;
        const auto freezeWorkspace = [&workspaces](
                                          ZzFluentUI::ZzSplitWorkspace *workspace) {
            if (workspace == nullptr || std::find(workspaces.cbegin(),
                                      workspaces.cend(),
                                      workspace)
                    != workspaces.cend()) {
                return;
            }
            workspaces.push_back(workspace);
            workspace->beginCoordinatorTransaction();
        };
        freezeWorkspace(sourceWorkspace);
        for (const auto &plan : targetPlans) {
            const auto target = findWindowId(plan.snapshot.windowId);
            freezeWorkspace(target != records.end() && target->shell
                    ? target->shell->splitWorkspace()
                    : nullptr);
        }
        const auto thawWorkspaces = qScopeGuard([&workspaces] {
            for (const auto &workspace : workspaces) {
                if (!workspace.isNull()) workspace->endCoordinatorTransaction();
            }
        });

        record = findWindowId(closingWindowId);
        if (record != records.end()) {
            record->closeState = ZzCloseState::NotifyingClose;
        }
        if (!guardedWindow.isNull()) {
            Q_EMIT q_ptr->windowAboutToClose(
                guardedWindow, zzRawPages(guardedPages));
        }

        record = findWindowId(closingWindowId);
        notificationAuditOk = record != records.end()
            && record->window == guardedWindow && !guardedWindow.isNull()
            && record->shell
            && record->shell->splitWorkspace() == sourceWorkspace
            && !sourceWorkspace.isNull();
        if (notificationAuditOk) {
            const auto groups = sourceWorkspace->groupIds();
            notificationAuditOk = groups.size()
                == static_cast<qsizetype>(sourceSnapshots.size());
            for (std::size_t index = 0;
                 notificationAuditOk && index < sourceSnapshots.size();
                 ++index) {
                const auto &snapshot = sourceSnapshots.at(index);
                notificationAuditOk = groups.at(static_cast<qsizetype>(index))
                        == snapshot.group
                    && sourceWorkspace->tabWidget(snapshot.group) != nullptr
                    && zzGroupPages(sourceWorkspace, snapshot.group)
                        == snapshot.pages;
            }
        }
        for (const auto &move : moves) {
            auto *const tabs = notificationAuditOk
                ? sourceWorkspace->tabWidget(move.sourceGroup)
                : nullptr;
            if (tabs == nullptr || move.page.isNull()
                || tabs->widget(move.sourceIndex) != move.page
                || sourceWorkspace->pageId(move.page) != move.pageId
                || sourceWorkspace->pageForId(move.pageId) != move.page) {
                notificationAuditOk = false;
                break;
            }
        }
        for (const auto &plan : targetPlans) {
            if (!notificationAuditOk) break;
            const auto target = findWindowId(plan.snapshot.windowId);
            auto *const workspace = target != records.end() && target->window
                    && target->shell
                ? target->shell->splitWorkspace()
                : nullptr;
            if (workspace == nullptr
                || workspace->tabWidget(plan.snapshot.group) == nullptr
                || zzGroupPages(workspace, plan.snapshot.group)
                    != plan.snapshot.pages) {
                notificationAuditOk = false;
            }
        }
    }
    if (!notificationAuditOk) {
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("window close notification changed reclaim state"));
    }
    record = findWindowId(closingWindowId);
    if (record == records.end()) {
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window disappeared before page reclaim"));
    }
    record->closeState = ZzCloseState::Reclaiming;

    std::vector<QPointer<QObject>> mutedObjects;
    try {
        mutedObjects.push_back(sourceWorkspace);
        for (const auto &snapshot : sourceSnapshots) {
            auto *const tabs = sourceWorkspace->tabWidget(snapshot.group);
            mutedObjects.push_back(tabs);
        }
        for (const auto &plan : targetPlans) {
            const auto target = findWindowId(plan.snapshot.windowId);
            auto *const workspace = target != records.end() && target->shell
                ? target->shell->splitWorkspace()
                : nullptr;
            mutedObjects.push_back(workspace);
            auto *const tabs = workspace != nullptr
                ? workspace->tabWidget(plan.snapshot.group)
                : nullptr;
            mutedObjects.push_back(tabs);
        }
    } catch (...) {
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to reserve page reclaim signal state"));
    }

    QElapsedTimer reclaimTimer;
    reclaimTimer.start();
    std::size_t completedCount = 0;
    bool transactionSucceeded = true;
    QString failureMessage = QStringLiteral("page reclaim transfer failed");
    {
        ZzScopedSignalMute mute(mutedObjects);
        for (; completedCount < moves.size(); ++completedCount) {
            const auto &move = moves.at(completedCount);
            const auto target = findWindowId(move.targetWindowId);
            auto *const targetWorkspace =
                target != records.end() && target->shell
                ? target->shell->splitWorkspace()
                : nullptr;
            auto *const sourceTabs = !sourceWorkspace.isNull()
                ? sourceWorkspace->tabWidget(move.sourceGroup)
                : nullptr;
            const int currentIndex = sourceTabs != nullptr
                    && !move.page.isNull()
                ? sourceTabs->indexOf(move.page)
                : -1;
            const auto transferred = targetWorkspace != nullptr
                    && targetWorkspace->tabWidget(move.targetGroup) != nullptr
                ? sourceWorkspace->transferTabToWorkspaceSilently(
                    move.sourceGroup,
                    currentIndex,
                    targetWorkspace,
                    move.targetGroup)
                : zzCoordinatorFailure<void>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("page reclaim target disappeared"));
            if (!transferred) {
                transactionSucceeded = false;
                failureMessage = transferred.error().technicalMessage();
                writePageAudit(QStringLiteral("page.reclaim"),
                    move.pageId,
                    closingWindowId,
                    move.targetWindowId,
                    QStringLiteral("transfer"),
                    reclaimTimer,
                    false);
                break;
            }
            if (sourceWorkspace.isNull() || guardedWindow.isNull()) {
                ++completedCount;
                transactionSucceeded = false;
                failureMessage =
                    QStringLiteral("page reclaim participant disappeared");
                writePageAudit(QStringLiteral("page.reclaim"),
                    move.pageId,
                    closingWindowId,
                    move.targetWindowId,
                    QStringLiteral("transfer"),
                    reclaimTimer,
                    false);
                break;
            }
        }
        if (transactionSucceeded) {
            for (const auto &plan : targetPlans) {
                const auto target = findWindowId(plan.snapshot.windowId);
                auto *const workspace =
                    target != records.end() && target->shell
                    ? target->shell->splitWorkspace()
                    : nullptr;
                bool complete = false;
                const auto pages = zzRawPages(
                    plan.desiredPages, &complete);
                if (!complete || workspace == nullptr
                    || !workspace->restoreGroupOrderSilently(
                        plan.snapshot.group, pages)) {
                    transactionSucceeded = false;
                    failureMessage =
                        QStringLiteral("page reclaim target order changed");
                    break;
                }
            }
        }
    }

    const auto rollback = [&](std::size_t count) {
        bool restored = !sourceWorkspace.isNull();
        {
            ZzScopedSignalMute mute(mutedObjects);
            for (std::size_t index = count; index > 0; --index) {
                const auto &move = moves.at(index - 1);
                const auto target = findWindowId(move.targetWindowId);
                auto *const targetWorkspace =
                    target != records.end() && target->shell
                    ? target->shell->splitWorkspace()
                    : nullptr;
                const auto actualGroup = zzPageGroup(
                    targetWorkspace, move.page);
                auto *const targetTabs = targetWorkspace != nullptr
                    ? targetWorkspace->tabWidget(actualGroup)
                    : nullptr;
                const int targetIndex = targetTabs != nullptr
                        && !move.page.isNull()
                    ? targetTabs->indexOf(move.page)
                    : -1;
                if (targetWorkspace == nullptr || !actualGroup.isValid()
                    || targetIndex < 0 || sourceWorkspace.isNull()
                    || sourceWorkspace->tabWidget(move.sourceGroup) == nullptr
                    || !targetWorkspace->transferTabToWorkspaceSilently(
                        actualGroup,
                        targetIndex,
                        sourceWorkspace,
                        move.sourceGroup)) {
                    restored = false;
                }
                writePageAudit(QStringLiteral("page.reclaim"),
                    move.pageId,
                    closingWindowId,
                    move.targetWindowId,
                    QStringLiteral("rollback"),
                    reclaimTimer,
                    false);
            }
            for (const auto &snapshot : sourceSnapshots) {
                bool complete = false;
                const auto pages = zzRawPages(snapshot.pages, &complete);
                if (!complete || sourceWorkspace.isNull()
                    || !sourceWorkspace->restoreGroupOrderSilently(
                        snapshot.group, pages)) {
                    restored = false;
                }
            }
            for (const auto &plan : targetPlans) {
                const bool touched = std::any_of(moves.cbegin(),
                    moves.cbegin() + static_cast<std::ptrdiff_t>(count),
                    [&plan](const ZzReclaimMove &move) {
                        return move.targetWindowId == plan.snapshot.windowId
                            && move.targetGroup == plan.snapshot.group;
                    });
                if (!touched) continue;
                const auto target = findWindowId(plan.snapshot.windowId);
                auto *const workspace =
                    target != records.end() && target->shell
                    ? target->shell->splitWorkspace()
                    : nullptr;
                bool complete = false;
                const auto pages = zzRawPages(
                    plan.snapshot.pages, &complete);
                if (!complete || workspace == nullptr
                    || !workspace->restoreGroupOrderSilently(
                        plan.snapshot.group, pages)) {
                    restored = false;
                }
            }
        }
        resetState();
        return restored;
    };

    if (!transactionSucceeded) {
        const bool restored = rollback(completedCount);
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            restored ? failureMessage
                     : QStringLiteral("page reclaim rollback failed"));
    }

    record = findWindowId(closingWindowId);
    if (record == records.end() || record->window.isNull()) {
        const bool restored = rollback(moves.size());
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            restored
                ? QStringLiteral("workspace window disappeared before close dispatch")
                : QStringLiteral("page reclaim rollback failed"));
    }
    for (const auto &move : moves) {
        const auto history = std::find_if(pageOrigins.begin(),
            pageOrigins.end(),
            [&move](const ZzPageOrigins &value) {
                return value.page == move.page;
            });
        if (history == pageOrigins.end()) continue;
        const auto targetOrigin = std::find_if(history->origins.begin(),
            history->origins.end(),
            [&move](const ZzPageOrigin &origin) {
                return origin.windowId == move.targetWindowId;
            });
        if (targetOrigin != history->origins.end()) {
            history->origins.erase(targetOrigin, history->origins.end());
        }
    }

    for (const auto &move : moves) {
        writePageAudit(QStringLiteral("page.reclaim"),
            move.pageId,
            closingWindowId,
            move.targetWindowId,
            QStringLiteral("commit"),
            reclaimTimer,
            true);
    }
    record = findWindowId(closingWindowId);
    if (record != records.end()) {
        record->internalCloseDispatch = false;
        record->closeState = ZzCloseState::CloseAccepted;
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void>
ZzWorkspaceWindowCoordinatorPrivate::approveDelegatedClose(
    ZzApplicationWindow *window)
{
    if (QThread::currentThread() != q_ptr->thread() || shuttingDown) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting close approvals"));
    }
    auto record = std::find_if(records.begin(),
        records.end(),
        [window](const ZzWindowRecord &value) {
            return value.windowIdentity == window;
        });
    if (record == records.end()
        || record->configuration.closePolicy != ZzWindowClosePolicy::Delegate
        || record->closeState != ZzCloseState::DelegatePending) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window has no delegated close pending"));
    }
    record->closeState = ZzCloseState::DelegateApproved;
    return closeWindow(window);
}

bool ZzWorkspaceWindowCoordinatorPrivate::eventFilter(
    QObject *watched,
    QEvent *event)
{
    if (event == nullptr || event->type() != QEvent::Close) return false;
    auto record = std::find_if(records.begin(),
        records.end(),
        [watched](const ZzWindowRecord &value) {
            return value.windowIdentity == watched;
        });
    if (record == records.end()) return false;
    if (record->internalCloseDispatch) {
        return false;
    }
    if (closeTransactionActive(*record)) {
        event->ignore();
        return true;
    }
    if (record->configuration.closePolicy == ZzWindowClosePolicy::Deny) {
        invalidateCloseRequest(*record);
        event->ignore();
        return true;
    }
    if (record->configuration.closePolicy == ZzWindowClosePolicy::Delegate) {
        if (record->closeState == ZzCloseState::Idle) {
            record->closeState = ZzCloseState::DelegatePending;
            Q_EMIT q_ptr->windowCloseApprovalRequested(record->windowIdentity);
        }
        event->ignore();
        return true;
    }

    return false;
}

void ZzWorkspaceWindowCoordinatorPrivate::writeWindowAudit(
    QStringView event,
    const QUuid &windowId,
    QStringView phase,
    const QElapsedTimer &timer,
    bool success) const noexcept
{
    if (!ZzLog::shouldLog(ZzLog::ZzLogLevel::Info) || windowId.isNull()) {
        return;
    }
    const QByteArray message =
        QStringLiteral("%1 window_id=%2 phase=%3 elapsed_us=%4 result=%5")
            .arg(event, windowId.toString(QUuid::WithoutBraces), phase)
            .arg(std::max<qint64>(0, timer.nsecsElapsed() / 1000))
            .arg(success ? QStringLiteral("success")
                         : QStringLiteral("failure"))
            .toUtf8();
    ZzLog::writeText(ZzLog::ZzLogLevel::Info,
        std::string_view(message.constData(),
            static_cast<std::size_t>(message.size())));
}

void ZzWorkspaceWindowCoordinatorPrivate::writePageAudit(
    QStringView event,
    const ZzFluentUI::ZzWorkspacePageId &pageId,
    const QUuid &sourceWindowId,
    const QUuid &targetWindowId,
    QStringView phase,
    const QElapsedTimer &timer,
    bool success) const noexcept
{
    if (!ZzLog::shouldLog(ZzLog::ZzLogLevel::Info) || !pageId.isValid()
        || sourceWindowId.isNull() || targetWindowId.isNull()) {
        return;
    }
    const QByteArray message =
        QStringLiteral("%1 page_id=%2 source_window_id=%3 target_window_id=%4 "
                       "phase=%5 elapsed_us=%6 result=%7")
            .arg(event,
                pageId.toString(),
                sourceWindowId.toString(QUuid::WithoutBraces),
                targetWindowId.toString(QUuid::WithoutBraces),
                phase)
            .arg(std::max<qint64>(0, timer.nsecsElapsed() / 1000))
            .arg(success ? QStringLiteral("success")
                         : QStringLiteral("failure"))
            .toUtf8();
    ZzLog::writeText(ZzLog::ZzLogLevel::Info,
        std::string_view(message.constData(),
            static_cast<std::size_t>(message.size())));
}

} // namespace ZzPureTools
