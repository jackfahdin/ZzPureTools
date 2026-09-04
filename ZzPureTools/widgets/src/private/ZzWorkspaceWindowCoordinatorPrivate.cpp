#include "ZzWorkspaceWindowCoordinatorPrivate.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <QtCore/QDataStream>
#include <QtCore/QCryptographicHash>
#include <QtCore/QIODevice>
#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QScopeGuard>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <private/ZzSplitWorkspaceTransactionPrivate.h>
#include <ZzLog/ZzLog.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>

#include "ZzWorkspaceTopologyCodecPrivate.h"

namespace ZzPureTools {

namespace {

struct ZzWorkspaceAuditState final
{
    QPointer<ZzFluentUI::ZzSplitWorkspace> workspace;
    std::shared_ptr<void> state;
};

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

struct ZzRawSplitPage final
{
    QString key;
    QString group;
    int order = -1;
};

struct ZzRawSplitInfo final
{
    int depth = 0;
    QSet<QString> groups;
    QList<ZzRawSplitPage> pages;
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

[[nodiscard]] QScreen *zzTopologyScreen(QStringView name)
{
    if (!name.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen != nullptr && screen->name() == name) return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}

/** @brief 将历史逻辑几何限制到屏幕可用区域并保留标题栏和内容区。 */
[[nodiscard]] QRect zzConvergedTopologyGeometry(
    const QRect &requested,
    const QRect &available)
{
    if (!available.isValid() || available.width() <= 0
        || available.height() <= 0) {
        return {};
    }
    constexpr int minimumContentWidth = 160;
    constexpr int minimumContentHeight = 120;
    constexpr int titleBarHeight = 30;
    QSize size = requested.size().expandedTo(
        QSize(minimumContentWidth, minimumContentHeight + titleBarHeight));
    size.setWidth(std::min(size.width(), available.width()));
    size.setHeight(std::min(size.height(), available.height()));
    size = size.expandedTo(QSize(1, 1));
    QRect result(requested.topLeft(), size);
    result.moveLeft(std::clamp(result.left(), available.left(),
        available.right() - result.width() + 1));
    result.moveTop(std::clamp(result.top(), available.top(),
        available.bottom() - result.height() + 1));
    return result;
}

[[nodiscard]] bool zzTopologyHasMagic(
    const QByteArray &state,
    QByteArrayView magic) noexcept
{
    return state.size() >= magic.size()
        && QByteArrayView(state.constData(), magic.size()) == magic;
}

[[nodiscard]] bool zzReadSplitString(
    QDataStream &stream,
    QString *value,
    bool allowEmpty = false)
{
    quint16 length = 0;
    stream >> length;
    if (stream.status() != QDataStream::Ok || length > 256) return false;
    value->clear();
    value->reserve(length);
    for (quint16 index = 0; index < length; ++index) {
        quint16 codeUnit = 0;
        stream >> codeUnit;
        if (stream.status() != QDataStream::Ok) return false;
        value->append(QChar(codeUnit));
    }
    *value = value->trimmed();
    return allowEmpty || !value->isEmpty();
}

[[nodiscard]] bool zzReadSplitNode(
    QDataStream &stream,
    int depth,
    ZzRawSplitInfo *info)
{
    if (depth > 16 || info == nullptr) return false;
    quint8 kind = 0;
    stream >> kind;
    if (stream.status() != QDataStream::Ok) return false;
    if (kind == 0) {
        QString group;
        if (!zzReadSplitString(stream, &group) || info->groups.contains(group)) {
            return false;
        }
        info->groups.insert(group);
        info->depth = std::max(info->depth, depth);
        return true;
    }
    if (kind != 1) return false;
    quint8 orientation = 0;
    quint16 childCount = 0;
    stream >> orientation >> childCount;
    if (stream.status() != QDataStream::Ok
        || (orientation != static_cast<quint8>(Qt::Horizontal)
            && orientation != static_cast<quint8>(Qt::Vertical))
        || childCount < 2 || childCount > 64) return false;
    for (quint16 index = 0; index < childCount; ++index) {
        if (!zzReadSplitNode(stream, depth + 1, info)) return false;
    }
    quint16 sizeCount = 0;
    stream >> sizeCount;
    if (stream.status() != QDataStream::Ok || sizeCount != childCount) return false;
    for (quint16 index = 0; index < sizeCount; ++index) {
        qint32 size = 0;
        stream >> size;
        if (stream.status() != QDataStream::Ok || size <= 0) return false;
    }
    return true;
}

[[nodiscard]] bool zzReadRawSplitInfo(
    const QByteArray &encoded,
    ZzRawSplitInfo *info)
{
    if (info == nullptr || encoded.size() < 44
        || !zzTopologyHasMagic(encoded, QByteArrayView("ZZSW", 4))) return false;
    QDataStream envelope(encoded);
    envelope.setVersion(QDataStream::Qt_6_8);
    char magic[4]{};
    quint16 schema = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (envelope.readRawData(magic, 4) != 4) return false;
    envelope >> schema >> streamVersion >> payloadLength;
    if (envelope.status() != QDataStream::Ok || schema != 1
        || streamVersion != static_cast<quint16>(QDataStream::Qt_6_8)
        || payloadLength > 1024 * 1024
        || static_cast<qint64>(12) + payloadLength + 32 != encoded.size()) return false;
    QByteArray payload(static_cast<qsizetype>(payloadLength), Qt::Uninitialized);
    if (payloadLength > 0
        && envelope.readRawData(payload.data(), static_cast<int>(payloadLength))
            != static_cast<int>(payloadLength)) return false;
    QByteArray digest(32, Qt::Uninitialized);
    if (envelope.readRawData(digest.data(), digest.size()) != digest.size()
        || envelope.status() != QDataStream::Ok || !envelope.atEnd()
        || digest != QCryptographicHash::hash(payload, QCryptographicHash::Sha256)) {
        return false;
    }
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_8);
    *info = {};
    if (!zzReadSplitNode(stream, 1, info)) return false;
    QString active;
    if (!zzReadSplitString(stream, &active) || !info->groups.contains(active)) return false;
    quint16 pageCount = 0;
    stream >> pageCount;
    if (stream.status() != QDataStream::Ok || pageCount > 4096) return false;
    QSet<QString> keys;
    QHash<QString, QSet<int>> orders;
    for (quint16 index = 0; index < pageCount; ++index) {
        ZzRawSplitPage page;
        quint8 current = 0;
        qint32 order = -1;
        if (!zzReadSplitString(stream, &page.key)
            || !zzReadSplitString(stream, &page.group)
            || !info->groups.contains(page.group)) return false;
        stream >> order >> current;
        if (stream.status() != QDataStream::Ok || current > 1 || order < 0
            || keys.contains(page.key) || orders[page.group].contains(order)) return false;
        page.order = order;
        keys.insert(page.key);
        orders[page.group].insert(order);
        info->pages.append(std::move(page));
    }
    return stream.status() == QDataStream::Ok && stream.atEnd();
}

[[nodiscard]] ZzWorkspaceWindowConfigurationPatch zzTopologyPatch(
    const ZzWorkspaceWindowConfiguration &configuration)
{
    ZzWorkspaceWindowConfigurationPatch patch;
    patch.title = configuration.title;
    patch.icon = configuration.icon;
    patch.titleMode = configuration.titleMode;
    patch.closePolicy = configuration.closePolicy;
    patch.alwaysOnTop = configuration.alwaysOnTop;
    patch.minimumSize = configuration.minimumSize;
    patch.maximumSize = configuration.maximumSize;
    patch.initialGeometry = configuration.initialGeometry;
    return patch;
}

void zzCloseTopologyStagedWindow(ZzApplicationWindow *window)
{
    if (window != nullptr) window->close();
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
    QObject::disconnect(record.activePageConnection);
    QObject::disconnect(record.pageActivityConnection);
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

    std::vector<ZzWorkspaceAuditState> auditSnapshots;
    auditSnapshots.push_back({
        sourceWorkspace,
        ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::capture(sourceWorkspace)});
    for (const auto &plan : targetPlans) {
        const auto target = findWindowId(plan.snapshot.windowId);
        auto *const workspace = target != records.end() && target->shell
            ? target->shell->splitWorkspace()
            : nullptr;
        if (workspace == nullptr) {
            resetState();
            return zzCoordinatorFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("page reclaim target disappeared"));
        }
        if (std::none_of(auditSnapshots.cbegin(), auditSnapshots.cend(),
                [workspace](const ZzWorkspaceAuditState &snapshot) {
                    return snapshot.workspace == workspace;
                })) {
            auditSnapshots.push_back({
                workspace,
                ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::capture(workspace)});
        }
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
            ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::beginCloseNotification(
                workspace);
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
                if (!workspace.isNull())
                    ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::endCloseNotification(
                        workspace);
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
        if (notificationAuditOk) {
            for (const auto &snapshot : auditSnapshots) {
                if (snapshot.workspace.isNull()
                    || !ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::matches(
                        snapshot.workspace, snapshot.state)) {
                    notificationAuditOk = false;
                    break;
                }
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
        bool restored = true;
        for (const auto &snapshot : auditSnapshots) {
            restored = !snapshot.workspace.isNull()
                && ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::restore(
                    snapshot.workspace, snapshot.state)
                && restored;
        }
        resetState();
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            restored
                ? QStringLiteral("window close notification changed reclaim state")
                : QStringLiteral("window close notification rollback failed"));
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
                ? ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::transferSilently(
                    sourceWorkspace,
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
                    || !ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::restoreGroupOrder(
                        workspace,
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
                    || !ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::transferSilently(
                        targetWorkspace,
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
                    || !ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::restoreGroupOrder(
                        sourceWorkspace,
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
                    || !ZzFluentUI::ZzSplitWorkspaceTransactionPrivate::restoreGroupOrder(
                        workspace,
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

ZzCore::ZzResult<QByteArray>
ZzWorkspaceWindowCoordinatorPrivate::saveTopology() const
{
    if (QThread::currentThread() != q_ptr->thread() || shuttingDown) {
        return zzCoordinatorFailure<QByteArray>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting topology saves"));
    }
    if (records.empty()) {
        return zzCoordinatorFailure<QByteArray>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace topology has no registered windows"));
    }

    ZzWorkspaceTopologyStatePrivate state;
    state.windows.reserve(static_cast<qsizetype>(records.size()));
    QSet<ZzFluentUI::ZzWorkspacePageId> pageIds;
    QSet<QString> layoutKeys;
    QSet<QWidget *> observedPages;
    for (const auto &record : records) {
        if (record.window.isNull() || record.shell.isNull()
            || record.windowId.isNull()) {
            return zzCoordinatorFailure<QByteArray>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window registration is incomplete"));
        }
        auto *const workspace = record.shell->splitWorkspace();
        if (workspace == nullptr) {
            return zzCoordinatorFailure<QByteArray>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window has no split workspace"));
        }
        const QByteArray workspaceState = workspace->saveLayout();
        if (workspaceState.isEmpty()) {
            return zzCoordinatorFailure<QByteArray>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("split workspace layout could not be observed"));
        }
        ZzRawSplitInfo splitInfo;
        if (!zzReadRawSplitInfo(workspaceState, &splitInfo)) {
            return zzCoordinatorFailure<QByteArray>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("split workspace layout could not be decoded"));
        }
        ZzWorkspaceTopologyStatePrivate::ZzWindowState windowState;
        windowState.windowId = record.windowId;
        windowState.configuration = record.configuration;
        windowState.geometry = record.window->geometry();
        if (!windowState.geometry.isValid()) {
            return zzCoordinatorFailure<QByteArray>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window geometry could not be observed"));
        }
        QScreen *screen = record.window->screen();
        if (screen == nullptr && record.window->windowHandle() != nullptr) {
            screen = record.window->windowHandle()->screen();
        }
        if (screen == nullptr) {
            screen = QGuiApplication::screenAt(windowState.geometry.center());
        }
        windowState.screenName = screen != nullptr ? screen->name() : QString {};
        windowState.visible = record.window->isVisible();
        windowState.maximized = record.window->isMaximized();
        windowState.alwaysOnTop = record.shell->isAlwaysOnTop();
        windowState.treeDepth = splitInfo.depth;
        windowState.workspaceState = workspaceState;

        const QSet<QString> &splitGroups = splitInfo.groups;
        QSet<QString> observedKeys;
        for (const auto &group : workspace->groupIds()) {
            auto *const tabs = workspace->tabWidget(group);
            if (tabs == nullptr) {
                return zzCoordinatorFailure<QByteArray>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("split workspace group could not be observed"));
            }
            if (!splitGroups.contains(group.value())) {
                return zzCoordinatorFailure<QByteArray>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("split workspace group is missing from its layout"));
            }
            for (int index = 0; index < tabs->count(); ++index) {
                QWidget *const page = tabs->widget(index);
                const auto id = workspace->pageId(page);
                const QString key = workspace->pageLayoutKey(page).trimmed();
                if (page == nullptr || observedPages.contains(page)
                    || !id.isValid() || pageIds.contains(id)
                    || key.isEmpty() || key != workspace->pageLayoutKey(page)
                    || layoutKeys.contains(key)
                    || !splitGroups.contains(group.value())) {
                    return zzCoordinatorFailure<QByteArray>(
                        ZzCore::ZzErrorCode::InvalidState,
                        QStringLiteral("workspace page identity or layout key is incomplete"));
                }
                observedPages.insert(page);
                pageIds.insert(id);
                layoutKeys.insert(key);
                observedKeys.insert(key);
                ZzWorkspaceTopologyStatePrivate::ZzPageState pageState;
                pageState.pageId = id;
                pageState.layoutKey = key;
                pageState.windowId = record.windowId;
                pageState.groupId = group.value();
                pageState.index = index;
                const auto history = std::find_if(
                    pageOrigins.cbegin(), pageOrigins.cend(),
                    [page](const ZzPageOrigins &entry) {
                        return entry.page == page;
                    });
                if (history != pageOrigins.cend()) {
                    if (history->page.isNull()
                        || history->origins.size()
                            > static_cast<std::size_t>(
                                ZzWorkspaceTopologyStatePrivate::MaximumOriginDepth)) {
                        return zzCoordinatorFailure<QByteArray>(
                            ZzCore::ZzErrorCode::InvalidState,
                            QStringLiteral("workspace page origin history is incomplete"));
                    }
                    for (const auto &origin : history->origins) {
                        if (origin.windowId.isNull() || !origin.group.isValid()
                            || origin.index < 0) {
                            return zzCoordinatorFailure<QByteArray>(
                                ZzCore::ZzErrorCode::InvalidState,
                                QStringLiteral("workspace page origin history is invalid"));
                        }
                        pageState.origins.append({origin.windowId,
                            origin.group.value(), origin.index});
                    }
                }
                windowState.pages.append(std::move(pageState));
            }
        }
        for (const auto &saved : splitInfo.pages) {
            if (saved.key.isEmpty() || !observedKeys.contains(saved.key)) {
                return zzCoordinatorFailure<QByteArray>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("split workspace layout does not describe every page"));
            }
        }
        if (observedKeys.size() != splitInfo.pages.size()) {
            return zzCoordinatorFailure<QByteArray>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("split workspace layout does not describe every page"));
        }
        state.windows.append(std::move(windowState));
    }
    if (!state.isValid()) {
        return zzCoordinatorFailure<QByteArray>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace topology contains invalid values"));
    }
    return ZzWorkspaceTopologyCodecPrivate::encode(state);
}

ZzCore::ZzResult<void>
ZzWorkspaceWindowCoordinatorPrivate::restoreTopology(
    const QByteArray &encoded,
    const ZzWorkspacePageResolver &pageResolver)
{
    if (QThread::currentThread() != q_ptr->thread() || shuttingDown) {
        return zzCoordinatorFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("workspace window coordinator is not accepting topology restores"));
    }

    // 页面非空时必须在任何解码、工厂或 resolver 调用前拒绝请求。
    for (const auto &record : records) {
        if (record.shell.isNull() || record.shell->splitWorkspace() == nullptr) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window registration is incomplete"));
        }
        auto *const workspace = record.shell->splitWorkspace();
        for (const auto &group : workspace->groupIds()) {
            auto *const tabs = workspace->tabWidget(group);
            if (tabs == nullptr || tabs->count() != 0) {
                return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("topology restore requires empty business pages"));
            }
        }
    }
    if (!pageResolver) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace page resolver is empty"));
    }

    auto decoded = ZzWorkspaceTopologyCodecPrivate::decode(encoded);
    if (!decoded) {
        return ZzCore::ZzResult<void>::failure(decoded.error());
    }
    ZzWorkspaceTopologyStatePrivate topology = std::move(decoded).value();
    const bool legacy = topology.windows.size() == 1
        && topology.windows.front().windowId.isNull()
        && zzTopologyHasMagic(encoded, QByteArrayView("ZZSW", 4));
    if (legacy) {
        auto &window = topology.windows.front();
        ZzRawSplitInfo splitInfo;
        if (!zzReadRawSplitInfo(window.workspaceState, &splitInfo)) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("legacy ZZSW workspace state is invalid"));
        }
        window.windowId = QUuid::createUuid();
        window.configuration = {};
        window.screenName.clear();
        if (QScreen *primary = QGuiApplication::primaryScreen(); primary != nullptr) {
            window.screenName = primary->name();
            const QRect available = primary->availableGeometry();
            window.geometry = QRect(available.center() - QPoint(400, 300),
                QSize(800, 600));
        } else {
            window.geometry = QRect(0, 0, 800, 600);
        }
        window.visible = true;
        window.maximized = false;
        window.alwaysOnTop = false;
        window.treeDepth = splitInfo.depth;
        window.pages.clear();
        for (const auto &saved : splitInfo.pages) {
            if (saved.key.isEmpty()) continue;
            ZzWorkspaceTopologyStatePrivate::ZzPageState page;
            page.pageId = ZzFluentUI::ZzWorkspacePageId::create();
            page.layoutKey = saved.key;
            page.windowId = window.windowId;
            page.groupId = saved.group;
            page.index = saved.order;
            window.pages.append(std::move(page));
        }
    }

    if (!topology.isValid()) {
        return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("workspace topology references are invalid"));
    }
    QSet<QUuid> knownWindowIds;
    for (const auto &record : records) {
        if (record.windowId.isNull()) continue;
        knownWindowIds.insert(record.windowId);
    }
    QSet<QString> allLayoutKeys;
    QSet<QWidget *> existingPages;
    for (const auto &window : topology.windows) {
        if (knownWindowIds.contains(window.windowId)) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window identity collides with current topology"));
        }
        ZzRawSplitInfo splitInfo;
        if (!zzReadRawSplitInfo(window.workspaceState, &splitInfo)) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("workspace state is invalid"));
        }
        if (splitInfo.depth != window.treeDepth) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("workspace tree depth does not match its state"));
        }
        const QSet<QString> &groups = splitInfo.groups;
        QSet<QString> stateKeys;
        for (const auto &saved : splitInfo.pages) stateKeys.insert(saved.key);
        QSet<QString> topologyKeys;
        for (const auto &page : window.pages) {
            const QString key = page.layoutKey.trimmed();
            if (key.isEmpty() || key != page.layoutKey || allLayoutKeys.contains(key)
                || !groups.contains(page.groupId) || !stateKeys.contains(key)) {
                return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("workspace page layout key or group is invalid"));
            }
            allLayoutKeys.insert(key);
            topologyKeys.insert(key);
        }
        if (topologyKeys != stateKeys) {
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("workspace page list does not match its layout state"));
        }
    }

    struct StagedWindow final {
        ZzWorkspaceWindowHandle handle;
        int stateIndex = -1;
    };
    struct StagedPage final {
        int windowIndex = -1;
        const ZzWorkspaceTopologyStatePrivate::ZzPageState *state = nullptr;
        std::unique_ptr<QWidget> owned;
        QPointer<QWidget> page;
    };
    std::vector<StagedWindow> stagedWindows;
    std::vector<StagedPage> stagedPages;
    std::vector<QPointer<QWidget>> attachedPages;
    const auto cleanup = [&] {
        for (auto &staged : stagedPages) {
            if (staged.owned) staged.owned.reset();
        }
        for (const auto &page : attachedPages) {
            if (page.isNull()) continue;
            for (const auto &staged : stagedWindows) {
                auto *const shell = staged.handle.shell.data();
                auto *const workspace = shell != nullptr ? shell->splitWorkspace() : nullptr;
                if (workspace == nullptr) continue;
                for (const auto &group : workspace->groupIds()) {
                    auto *const tabs = workspace->tabWidget(group);
                    const int index = tabs != nullptr ? tabs->indexOf(page) : -1;
                    if (index >= 0) tabs->removeTab(index);
                }
            }
            delete page.data();
        }
        for (auto iterator = stagedWindows.rbegin();
             iterator != stagedWindows.rend(); ++iterator) {
            if (!iterator->handle.window.isNull()) {
                static_cast<void>(q_ptr->unregisterWindow(
                    iterator->handle.window.data()));
                zzCloseTopologyStagedWindow(iterator->handle.window.data());
            }
        }
        QCoreApplication::sendPostedEvents(qApp, QEvent::MetaCall);
    };

    for (qsizetype index = 0; index < topology.windows.size(); ++index) {
        const auto &window = topology.windows.at(index);
        ZzWorkspaceWindowCreateOptions options;
        options.configurationSource = ZzWorkspaceConfigurationSource::Explicit;
        options.configuration = zzTopologyPatch(window.configuration);
        options.visibility = ZzApplicationWindowVisibility::Deferred;
        options.activate = false;
        auto created = q_ptr->createWindow(options);
        if (!created) {
            cleanup();
            return ZzCore::ZzResult<void>::failure(created.error());
        }
        StagedWindow staged;
        staged.handle = created.value();
        staged.stateIndex = static_cast<int>(index);
        if (staged.handle.window.isNull() || staged.handle.shell.isNull()
            || staged.handle.shell->splitWorkspace() == nullptr) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window factory returned an incomplete window"));
        }
        auto *const workspace = staged.handle.shell->splitWorkspace();
        bool empty = true;
        for (const auto &group : workspace->groupIds()) {
            auto *const tabs = workspace->tabWidget(group);
            empty = empty && tabs != nullptr && tabs->count() == 0;
        }
        if (!empty) {
            stagedWindows.push_back(std::move(staged));
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace window factory returned business pages"));
        }
        stagedWindows.push_back(std::move(staged));
    }

    for (qsizetype index = 0; index < topology.windows.size(); ++index) {
        const auto &window = topology.windows.at(index);
        for (const auto &pageState : window.pages) {
            std::unique_ptr<QWidget> page;
            try {
                auto resolved = pageResolver(QStringView(pageState.layoutKey));
                if (!resolved) {
                    cleanup();
                    return ZzCore::ZzResult<void>::failure(resolved.error());
                }
                page = std::move(resolved).value();
            } catch (const std::exception &exception) {
                cleanup();
                return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unknown,
                    QStringLiteral("workspace page resolver threw an exception: %1")
                        .arg(QString::fromLocal8Bit(exception.what())));
            } catch (...) {
                cleanup();
                return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unknown,
                    QStringLiteral("workspace page resolver threw an exception"));
            }
            if (!page || page->parent() != nullptr
                || page->thread() != q_ptr->thread()) {
                cleanup();
                return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("workspace page resolver must return unique parentless pages"));
            }
            if (existingPages.contains(page.get())) {
                // 解析器错误地重复返回同一裸指针时，避免两个 unique_ptr 二次释放。
                static_cast<void>(page.release());
                cleanup();
                return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("workspace page resolver must return unique parentless pages"));
            }
            existingPages.insert(page.get());
            stagedPages.push_back({static_cast<int>(index), &pageState,
                std::move(page), nullptr});
        }
    }

    std::vector<QPointer<QObject>> mutedObjects;
    for (const auto &staged : stagedWindows) {
        mutedObjects.push_back(staged.handle.shell->splitWorkspace());
        for (const auto &group : staged.handle.shell->splitWorkspace()->groupIds()) {
            mutedObjects.push_back(staged.handle.shell->splitWorkspace()->tabWidget(group));
        }
    }
    ZzScopedSignalMute mute(mutedObjects);
    for (auto &staged : stagedPages) {
        auto &window = topology.windows.at(staged.windowIndex);
        auto &stagedWindow = stagedWindows.at(static_cast<std::size_t>(staged.windowIndex));
        auto *const workspace = stagedWindow.handle.shell->splitWorkspace();
        const auto groups = workspace->groupIds();
        if (groups.isEmpty()) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace has no initial group"));
        }
        auto *const tabs = workspace->tabWidget(groups.constFirst());
        QWidget *const page = staged.owned.get();
        if (tabs == nullptr || page == nullptr
            || tabs->addTab(page, page->windowTitle().isEmpty()
                    ? staged.state->layoutKey : page->windowTitle()) < 0
            || !workspace->setPageLayoutKey(page, staged.state->layoutKey)) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace page could not be staged"));
        }
        staged.page = page;
        attachedPages.push_back(page);
        static_cast<void>(staged.owned.release());
        Q_UNUSED(window);
    }
    for (auto &staged : stagedWindows) {
        const auto &window = topology.windows.at(staged.stateIndex);
        if (!staged.handle.shell->splitWorkspace()->restoreLayout(
                window.workspaceState)) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace layout restore failed"));
        }
    }
    for (const auto &staged : stagedPages) {
        if (staged.page.isNull()) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("staged workspace page disappeared"));
        }
        const auto &expected = *staged.state;
        auto *const workspace = stagedWindows.at(
            static_cast<std::size_t>(staged.windowIndex)).handle.shell->splitWorkspace();
        const auto actualGroup = zzPageGroup(workspace, staged.page.data());
        auto *const tabs = workspace->tabWidget(actualGroup);
        const int actualIndex = tabs != nullptr ? tabs->indexOf(staged.page) : -1;
        if (!actualGroup.isValid() || actualGroup.value() != expected.groupId
            || actualIndex != expected.index
            || workspace->pageLayoutKey(staged.page) != expected.layoutKey) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("workspace page ownership does not match topology"));
        }
    }
    for (auto &staged : stagedWindows) {
        const auto &window = topology.windows.at(staged.stateIndex);
        const auto configured = q_ptr->applyConfiguration(
            staged.handle.window.data(), zzTopologyPatch(window.configuration));
        if (!configured) {
            cleanup();
            return configured;
        }
        auto *const screen = zzTopologyScreen(window.screenName);
        if (screen == nullptr) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unsupported,
                QStringLiteral("no screen is available for workspace restore"));
        }
        const QRect geometry = zzConvergedTopologyGeometry(
            window.geometry, screen->availableGeometry());
        if (!geometry.isValid()) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::Unsupported,
                QStringLiteral("workspace geometry cannot fit any screen"));
        }
        staged.handle.window->setGeometry(geometry);
        if (window.alwaysOnTop != staged.handle.shell->isAlwaysOnTop()) {
            const auto topMost = staged.handle.shell->setAlwaysOnTop(window.alwaysOnTop);
            if (!topMost) {
                cleanup();
                return topMost;
            }
        }
        staged.handle.window->setWindowState(window.maximized
            ? staged.handle.window->windowState() | Qt::WindowMaximized
            : staged.handle.window->windowState() & ~Qt::WindowMaximized);
    }

    // 所有可失败步骤完成后才写入稳定窗口身份、来源栈并执行一次显示提交。
    for (auto &staged : stagedWindows) {
        auto record = std::find_if(records.begin(), records.end(),
            [&staged](const ZzWindowRecord &value) {
                return value.windowIdentity == staged.handle.window.data();
            });
        if (record == records.end()) {
            cleanup();
            return zzCoordinatorFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("staged workspace window registration disappeared"));
        }
        record->windowId = topology.windows.at(staged.stateIndex).windowId;
    }
    for (const auto &staged : stagedPages) {
        ZzPageOrigins history;
        history.page = staged.page;
        for (const auto &origin : staged.state->origins) {
            history.origins.push_back({origin.windowId,
                ZzFluentUI::ZzTabGroupId(origin.groupId), origin.index,
                nextTransferSequence++});
        }
        pageOrigins.push_back(std::move(history));
    }
    for (const auto &staged : stagedWindows) {
        const auto &window = topology.windows.at(staged.stateIndex);
        if (window.visible) staged.handle.window->show();
    }
    for (const auto &staged : stagedWindows) {
        const auto &window = topology.windows.at(staged.stateIndex);
        if (window.visible && !staged.handle.window.isNull()) {
            staged.handle.window->raise();
            staged.handle.window->activateWindow();
            break;
        }
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<QByteArray> ZzWorkspaceWindowCoordinator::saveTopology() const
{
    return d_ptr->saveTopology();
}

ZzCore::ZzResult<void> ZzWorkspaceWindowCoordinator::restoreTopology(
    const QByteArray &state,
    const ZzWorkspacePageResolver &pageResolver)
{
    return d_ptr->restoreTopology(state, pageResolver);
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
