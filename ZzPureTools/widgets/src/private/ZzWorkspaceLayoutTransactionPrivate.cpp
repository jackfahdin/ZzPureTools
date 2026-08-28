#include "ZzWorkspaceLayoutTransactionPrivate.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtCore/QIODevice>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include "ZzWorkspaceLayoutCodecPrivate.h"
#include "ZzWorkspaceLayoutStatePrivate.h"
#include "ZzWorkspaceShellPrivate.h"

namespace ZzPureTools {
namespace {

using ZzLayoutState = ZzWorkspaceLayoutStatePrivate;
using ZzProjection = ZzLayoutState::ZzWorkspaceProjection;
using ZzSnapshot = ZzLayoutState::ZzWorkspaceSnapshot;
using ZzSide = ZzLayoutState::ZzSideProjection;

constexpr quint16 zzQtDockStateVersion = 1;
constexpr quint16 zzWorkspaceSchemaVersion = 3;
constexpr auto zzStreamVersion = QDataStream::Qt_6_8;
constexpr qsizetype zzMaximumLayoutSize = qsizetype{1024} * 1024;

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(
        ZzCore::ZzError(code, std::move(message), std::move(context)));
}

[[nodiscard]] ZzLayoutState::ZzPanelKind zzPanelKind(
    ZzWorkspaceShellPrivate::ZzPanelKind kind) noexcept
{
    switch (kind) {
    case ZzWorkspaceShellPrivate::ZzPanelKind::Side:
        return ZzLayoutState::ZzPanelKind::Side;
    case ZzWorkspaceShellPrivate::ZzPanelKind::Bottom:
        return ZzLayoutState::ZzPanelKind::Bottom;
    case ZzWorkspaceShellPrivate::ZzPanelKind::Dock:
        return ZzLayoutState::ZzPanelKind::Dock;
    }
    return ZzLayoutState::ZzPanelKind::Side;
}

[[nodiscard]] ZzLayoutState::ZzTitleMode zzTitleMode(
    ZzWorkspaceTitleMode mode) noexcept
{
    return static_cast<ZzLayoutState::ZzTitleMode>(mode);
}

[[nodiscard]] ZzWorkspaceTitleMode zzTitleMode(
    ZzLayoutState::ZzTitleMode mode) noexcept
{
    return static_cast<ZzWorkspaceTitleMode>(mode);
}

[[nodiscard]] QString zzEffectiveTitle(
    const ZzWorkspaceShellPrivate &shell)
{
    QString pageTitle;
    if (shell.activeTabs != nullptr
        && shell.activeTabs->currentWidget() != nullptr) {
        pageTitle = shell.activeTabs->currentWidget()->windowTitle();
        if (pageTitle.isEmpty()) {
            pageTitle = shell.activeTabs->tabText(
                shell.activeTabs->currentIndex());
        }
    }
    switch (shell.titleMode) {
    case ZzWorkspaceTitleMode::Application:
        return shell.applicationTitle;
    case ZzWorkspaceTitleMode::CurrentTab:
        return pageTitle.isEmpty() ? shell.applicationTitle : pageTitle;
    case ZzWorkspaceTitleMode::CurrentTabAndApplication:
        if (pageTitle.isEmpty()) return shell.applicationTitle;
        if (shell.applicationTitle.isEmpty()) return pageTitle;
        return pageTitle + QStringLiteral(" - ") + shell.applicationTitle;
    case ZzWorkspaceTitleMode::Custom:
        return shell.customTitle.isEmpty()
            ? shell.applicationTitle : shell.customTitle;
    }
    return {};
}

[[nodiscard]] ZzLayoutState::ZzSubsystemIdentity zzIdentity(
    QObject *object)
{
    return {object, object};
}

struct ZzRuntimeGuards final
{
    QPointer<QMainWindow> host;
    QPointer<ZzFluentUI::ZzSplitWorkspace> split;
    QPointer<ZzFluentUI::ZzSidePane> leftSide;
    QPointer<ZzFluentUI::ZzSidePane> rightSide;
    QPointer<ZzFluentUI::ZzBottomPane> bottom;
    QPointer<ZzFluentUI::ZzActivityBar> leftActivity;
    QPointer<ZzFluentUI::ZzActivityBar> rightActivity;
    QPointer<QAbstractListModel> activityModel;
    QPointer<ZzFluentUI::ZzFluentTitleBar> titleBar;
};

struct ZzSideOwnerIdentity final
{
    QPointer<QWidget> object;
    QWidget *rawObject = nullptr;
};

using ZzSideOwnerIndex = QHash<QString, ZzSideOwnerIdentity>;

struct ZzRuntimeSnapshot final
{
    ZzSnapshot projection;
    ZzRuntimeGuards guards;
    QHash<QString, int> panelRows;
    ZzSideOwnerIndex sideOwners;
};

struct ZzRuntimeIndexes final
{
    QHash<QWidget *, QString> widgetIds;
    QHash<QString, int> activityRows;
    QHash<int, QString> activityIds;
};

/** @brief 仅允许事务声明的 Side 内容换父，并在合法 attach 事件内固定新 frame。 */
class ZzSideOwnerObserver final : public QObject
{
public:
    ZzSideOwnerObserver(
        const ZzProjection &projection,
        ZzSideOwnerIndex *owners)
        : owners_(owners)
    {
        for (const auto &identity : projection.identities) {
            if (identity.kind != ZzLayoutState::ZzPanelKind::Side
                || identity.widget == nullptr) {
                continue;
            }
            watchedContents_.append(identity.widget);
            identity.widget->installEventFilter(this);
        }
    }

    ~ZzSideOwnerObserver() override
    {
        for (const QPointer<QWidget> &content : std::as_const(watchedContents_)) {
            if (content != nullptr) {
                content->removeEventFilter(this);
            }
        }
    }

    void beginDetach(const QString &id, QWidget *content) noexcept
    {
        beginMutation(ZzMutation::Detach, id, content, nullptr);
    }

    void beginAttach(
        const QString &id,
        QWidget *content,
        ZzFluentUI::ZzPanelStack *stack) noexcept
    {
        beginMutation(ZzMutation::Attach, id, content, stack);
    }

    void finishMutation() noexcept
    {
        if (mutation_ != ZzMutation::None && !consumed_) {
            valid_ = false;
        }
        mutation_ = ZzMutation::None;
        expectedId_.clear();
        expectedContent_ = nullptr;
        expectedStack_ = nullptr;
        consumed_ = false;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return valid_;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event == nullptr || event->type() != QEvent::ParentChange) {
            return QObject::eventFilter(watched, event);
        }
        QWidget *const content = qobject_cast<QWidget *>(watched);
        if (mutation_ == ZzMutation::None || consumed_
            || content == nullptr || content != expectedContent_) {
            valid_ = false;
            return QObject::eventFilter(watched, event);
        }
        consumed_ = true;
        if (mutation_ == ZzMutation::Detach) {
            if (content->parent() != nullptr) {
                valid_ = false;
            } else if (owners_ != nullptr) {
                owners_->insert(expectedId_, {});
            }
        } else {
            QWidget *const owner = content->parentWidget();
            if (owner == nullptr || expectedStack_ == nullptr
                || !expectedStack_->isAncestorOf(owner)) {
                valid_ = false;
            } else if (owners_ != nullptr) {
                owners_->insert(expectedId_, {owner, owner});
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    enum class ZzMutation : unsigned char
    {
        None,
        Detach,
        Attach
    };

    void beginMutation(
        ZzMutation mutation,
        const QString &id,
        QWidget *content,
        ZzFluentUI::ZzPanelStack *stack) noexcept
    {
        if (mutation_ != ZzMutation::None || content == nullptr
            || id.isEmpty()) {
            valid_ = false;
            return;
        }
        mutation_ = mutation;
        expectedId_ = id;
        expectedContent_ = content;
        expectedStack_ = stack;
        consumed_ = false;
    }

    QList<QPointer<QWidget>> watchedContents_;
    ZzSideOwnerIndex *owners_ = nullptr;
    QString expectedId_;
    QPointer<QWidget> expectedContent_;
    QPointer<ZzFluentUI::ZzPanelStack> expectedStack_;
    ZzMutation mutation_ = ZzMutation::None;
    bool consumed_ = false;
    bool valid_ = true;
};

[[nodiscard]] ZzRuntimeIndexes zzBuildRuntimeIndexes(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime)
{
    ZzRuntimeIndexes indexes;
    indexes.widgetIds.reserve(runtime.projection.identities.size());
    for (const auto &identity : runtime.projection.identities) {
        if (identity.rawWidget != nullptr) {
            indexes.widgetIds.insert(identity.rawWidget, identity.id);
        }
    }
    const auto rows = shell.activityRows();
    indexes.activityRows.reserve(rows.size());
    indexes.activityIds.reserve(rows.size());
    for (qsizetype row = 0; row < rows.size(); ++row) {
        const QString id = rows.at(row).id.value();
        indexes.activityRows.insert(id, static_cast<int>(row));
        indexes.activityIds.insert(static_cast<int>(row), id);
    }
    return indexes;
}

[[nodiscard]] QString zzIdForWidget(
    const QHash<QWidget *, QString> &ids,
    QWidget *widget)
{
    return widget != nullptr ? ids.value(widget) : QString{};
}

void zzWriteSide(
    QDataStream &stream,
    ZzFluentUI::ZzSidePane *pane,
    const QHash<QWidget *, QString> &ids)
{
    const QString current = pane != nullptr
        ? zzIdForWidget(ids, pane->currentWidget()) : QString{};
    stream << static_cast<quint8>(
                  pane != nullptr && pane->isCollapsed() ? 1 : 0)
           << static_cast<qint32>(
                  pane != nullptr ? pane->paneWidth() : 280)
           << current;
}

/** @brief 只桥接真实 Qt 状态到 codec；所有有界解析仍由 codec 完成。 */
[[nodiscard]] QByteArray zzObservedEnvelope(
    const ZzWorkspaceShellPrivate &shell)
{
    if (shell.host == nullptr || shell.leftSidePane == nullptr
        || shell.rightSidePane == nullptr || shell.splitWorkspace == nullptr
        || shell.bottomPane == nullptr || shell.activityModel == nullptr) {
        return {};
    }
    QHash<QWidget *, QString> ids;
    ids.reserve(shell.panels.size());
    for (const auto &record : shell.panels) {
        if (record.content != nullptr
            && record.content.data() == record.contentIdentity) {
            ids.insert(record.contentIdentity, record.id.value());
        }
    }

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(zzStreamVersion);
    stream << shell.host->saveState(zzQtDockStateVersion);
    zzWriteSide(stream, shell.leftSidePane, ids);
    zzWriteSide(stream, shell.rightSidePane, ids);

    const auto rows = shell.activityRows();
    stream << static_cast<quint32>(rows.size());
    std::array<int, 4> areaOrders{};
    for (const auto &row : rows) {
        const auto areaIndex = static_cast<std::size_t>(row.area);
        stream << row.id.value() << static_cast<quint8>(row.area)
               << static_cast<qint32>(areaOrders.at(areaIndex)++);
    }
    stream << shell.splitWorkspace->saveLayout()
           << static_cast<quint8>(shell.bottomPane->isCollapsed() ? 1 : 0)
           << static_cast<qint32>(shell.bottomPane->paneHeight())
           << zzIdForWidget(ids, shell.bottomPane->currentWidget())
           << static_cast<quint8>(zzTitleMode(shell.titleMode));
    if (stream.status() != QDataStream::Ok) {
        return {};
    }

    QByteArray encoded;
    QDataStream envelope(&encoded, QIODevice::WriteOnly);
    envelope.setVersion(zzStreamVersion);
    if (envelope.writeRawData("ZZWS", 4) != 4) {
        return {};
    }
    envelope << zzWorkspaceSchemaVersion
             << static_cast<quint16>(zzStreamVersion)
             << static_cast<quint32>(payload.size());
    if (envelope.writeRawData(payload.constData(), payload.size())
            != payload.size()
        || envelope.status() != QDataStream::Ok) {
        return {};
    }
    encoded.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return encoded.size() <= zzMaximumLayoutSize ? encoded : QByteArray{};
}

[[nodiscard]] bool zzCaptureSideRuntime(
    const QHash<QWidget *, QString> &ids,
    ZzFluentUI::ZzSidePane *pane,
    ZzSide *side,
    ZzSideOwnerIndex *owners,
    const QString &sideName,
    QString *failureContext)
{
    const auto fail = [&sideName, failureContext](const QString &reason) {
        if (failureContext != nullptr) {
            *failureContext = QStringLiteral("%1/%2").arg(sideName, reason);
        }
        return false;
    };
    if (pane == nullptr || pane->panelStack() == nullptr) {
        return fail(QStringLiteral("container-unavailable"));
    }
    ZzFluentUI::ZzPanelStack *const stack = pane->panelStack();
    side->paneIdentity = zzIdentity(pane);
    side->stackIdentity = zzIdentity(stack);
    side->order.clear();
    side->contents.clear();
    for (QWidget *const content : stack->panels()) {
        const QString id = ids.value(content);
        QWidget *const owner = content != nullptr
            ? content->parentWidget() : nullptr;
        if (id.isEmpty()) {
            return fail(QStringLiteral("unregistered-content"));
        }
        if (owner == nullptr) {
            return fail(QStringLiteral("owner-missing/%1").arg(id));
        }
        if (owners->contains(id)) {
            return fail(QStringLiteral("duplicate-owner/%1").arg(id));
        }
        if (!stack->isAncestorOf(owner)) {
            return fail(QStringLiteral("owner-outside-stack/%1").arg(id));
        }
        owners->insert(id, {owner, owner});
        side->order.append(id);
        side->contents.append({
            id, side->stackIdentity,
            {side->paneIdentity, side->stackIdentity}});
    }
    // schema v3 不保存 stack 分配；快照仍保留它以支持失败事务的精确回滚。
    if (!side->current.isEmpty()) {
        side->visible = {side->current};
        side->sizes = {std::max(stack->panelSizes().value(0), 1)};
    }
    return true;
}

[[nodiscard]] std::optional<ZzRuntimeSnapshot> zzCaptureSnapshot(
    const ZzWorkspaceShellPrivate &shell,
    QString *failureContext = nullptr)
{
    if (failureContext != nullptr) {
        failureContext->clear();
    }
    const auto fail = [failureContext](const QString &reason) {
        if (failureContext != nullptr) {
            *failureContext = reason;
        }
        return std::optional<ZzRuntimeSnapshot>{};
    };
    const QByteArray observed = zzObservedEnvelope(shell);
    if (observed.isEmpty()) {
        return fail(QStringLiteral("observed-envelope-empty"));
    }
    auto decoded = ZzWorkspaceLayoutCodecPrivate::decode(observed);
    if (!decoded) {
        return fail(QStringLiteral("observed-envelope-invalid"));
    }
    auto decodedRequest = std::move(decoded).value();
    if (!decodedRequest.projection.has_value()) {
        return fail(QStringLiteral("projection-missing"));
    }

    ZzRuntimeSnapshot result;
    static_cast<ZzProjection &>(result.projection) =
        std::move(decodedRequest.projection).value();
    result.guards = {
        shell.host, shell.splitWorkspace, shell.leftSidePane,
        shell.rightSidePane, shell.bottomPane, shell.leftActivityBar,
        shell.rightActivityBar, shell.activityModel, shell.titleBar};

    QHash<QWidget *, QString> ids;
    ids.reserve(shell.panels.size());
    result.panelRows.reserve(shell.panels.size());
    result.projection.identities.clear();
    result.projection.dock.docks.clear();
    result.projection.dock.visible.clear();
    for (qsizetype row = 0; row < shell.panels.size(); ++row) {
        const auto &record = shell.panels.at(row);
        const QString id = record.id.value();
        if (result.panelRows.contains(id)) {
            return fail(QStringLiteral("duplicate-panel-id/%1").arg(id));
        }
        result.panelRows.insert(id, static_cast<int>(row));
        if (record.contentIdentity != nullptr) {
            ids.insert(record.contentIdentity, id);
        }
        ZzLayoutState::ZzPanelIdentity identity{
            id, zzPanelKind(record.kind), record.content,
            record.contentIdentity, record.registrationGeneration,
            record.dock, record.dockIdentity};
        result.projection.identities.append(identity);
        if (record.kind == ZzWorkspaceShellPrivate::ZzPanelKind::Dock) {
            auto *const dock = qobject_cast<ZzFluentUI::ZzDockPanel *>(
                record.dock.data());
            ZzLayoutState::ZzDockPlacement placement;
            placement.panel = identity;
            placement.area = dock != nullptr
                ? shell.host->dockWidgetArea(dock) : Qt::NoDockWidgetArea;
            placement.floating = dock != nullptr && dock->isFloating();
            placement.visible = dock != nullptr && !dock->isHidden();
            placement.actualOwnerIdentity = zzIdentity(
                dock != nullptr ? dock->parentWidget() : nullptr);
            result.projection.dock.docks.append(std::move(placement));
            if (dock != nullptr && !dock->isHidden()) {
                result.projection.dock.visible.append(id);
            }
        }
    }
    result.sideOwners.reserve(shell.panels.size());
    if (!zzCaptureSideRuntime(
            ids, shell.leftSidePane, &result.projection.leftSide,
            &result.sideOwners, QStringLiteral("left-side"), failureContext)
        || !zzCaptureSideRuntime(
            ids, shell.rightSidePane, &result.projection.rightSide,
            &result.sideOwners, QStringLiteral("right-side"), failureContext)) {
        return std::nullopt;
    }
    qsizetype readySideCount = 0;
    for (const auto &record : shell.panels) {
        if (record.kind != ZzWorkspaceShellPrivate::ZzPanelKind::Side) {
            continue;
        }
        if (record.materialization
            == ZzWorkspaceShellPrivate::ZzMaterializationState::Pending) {
            if (record.content != nullptr || record.contentIdentity != nullptr
                || !record.factory || result.sideOwners.contains(
                    record.id.value())) {
                return fail(QStringLiteral("pending-panel-invalid/%1")
                    .arg(record.id.value()));
            }
            continue;
        }
        if (record.materialization
                != ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
            || record.content == nullptr
            || record.content.data() != record.contentIdentity
            || record.factory
            || !result.sideOwners.contains(record.id.value())) {
            return fail(QStringLiteral("ready-panel-invalid/%1")
                .arg(record.id.value()));
        }
        ++readySideCount;
    }
    if (result.projection.leftSide.order.size()
            + result.projection.rightSide.order.size()
        != readySideCount) {
        return fail(QStringLiteral("ready-side-count-mismatch"));
    }

    result.projection.bottom.paneIdentity = zzIdentity(shell.bottomPane);
    auto *const bottomStack = shell.bottomPane != nullptr
        ? shell.bottomPane->findChild<QStackedWidget *>() : nullptr;
    result.projection.bottom.stackIdentity = zzIdentity(bottomStack);
    result.projection.bottom.order.clear();
    result.projection.bottom.contents.clear();
    if (bottomStack == nullptr) {
        return fail(QStringLiteral("bottom-stack-unavailable"));
    }
    for (int index = 0; index < bottomStack->count(); ++index) {
        QWidget *const content = bottomStack->widget(index);
        const QString id = ids.value(content);
        if (id.isEmpty()) {
            return fail(QStringLiteral("bottom-content-unregistered"));
        }
        result.projection.bottom.order.append(id);
        result.projection.bottom.contents.append({
            id, result.projection.bottom.stackIdentity,
            {result.projection.bottom.paneIdentity,
             result.projection.bottom.stackIdentity}});
    }
    const qsizetype registeredBottomCount = std::count_if(
        shell.panels.cbegin(), shell.panels.cend(), [](const auto &record) {
            return record.kind
                == ZzWorkspaceShellPrivate::ZzPanelKind::Bottom;
        });
    if (result.projection.bottom.order.size() != registeredBottomCount) {
        return fail(QStringLiteral("bottom-count-mismatch"));
    }
    result.projection.activity.modelIdentity = zzIdentity(shell.activityModel);
    result.projection.title.mode = zzTitleMode(shell.titleMode);
    result.projection.title.applicationTitle = shell.applicationTitle;
    result.projection.title.customTitle = shell.customTitle;
    result.projection.title.hostTitle = shell.host != nullptr
        ? shell.host->windowTitle() : QString{};
    result.projection.title.titleBarTitle = shell.titleBar != nullptr
        ? shell.titleBar->title() : QString{};
    return result;
}

[[nodiscard]] bool zzSideOrderMatchesActivity(
    const ZzRuntimeSnapshot &runtime)
{
    const auto &snapshot = runtime.projection;
    QSet<QString> readySideIds;
    QSet<QString> logicalSideIds;
    readySideIds.reserve(snapshot.identities.size());
    logicalSideIds.reserve(snapshot.identities.size());
    for (const auto &identity : snapshot.identities) {
        if (identity.kind != ZzLayoutState::ZzPanelKind::Side
            || identity.id.isEmpty() || logicalSideIds.contains(identity.id)) {
            continue;
        }
        logicalSideIds.insert(identity.id);
        if (identity.widget != nullptr
            && identity.widget.data() == identity.rawWidget) {
            readySideIds.insert(identity.id);
        }
    }
    QSet<QString> activitySideIds;
    for (const QStringList *rows : {
             &snapshot.activity.leftPrimary,
             &snapshot.activity.leftSecondary,
             &snapshot.activity.rightPrimary,
             &snapshot.activity.rightSecondary}) {
        for (const QString &id : *rows) {
            if (id.isEmpty() || activitySideIds.contains(id)) {
                return false;
            }
            activitySideIds.insert(id);
        }
    }
    if (activitySideIds != logicalSideIds) {
        return false;
    }
    const auto readyOrder = [&readySideIds](const QStringList &logicalOrder) {
        QStringList result;
        for (const QString &id : logicalOrder) {
            if (readySideIds.contains(id)) {
                result.append(id);
            }
        }
        return result;
    };
    return snapshot.leftSide.order == readyOrder(
               snapshot.activity.leftPrimary
                   + snapshot.activity.leftSecondary)
        && snapshot.rightSide.order == readyOrder(
               snapshot.activity.rightPrimary
                   + snapshot.activity.rightSecondary);
}

[[nodiscard]] const ZzWorkspaceShellPrivate::ZzPanelRecord *zzRecord(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const QString &id)
{
    const auto found = runtime.panelRows.constFind(id);
    if (found == runtime.panelRows.cend() || found.value() < 0
        || found.value() >= shell.panels.size()
        || shell.panels.at(found.value()).id.value() != id) {
        return nullptr;
    }
    return &shell.panels.at(found.value());
}

[[nodiscard]] ZzWorkspaceShellPrivate::ZzPanelRecord *zzRecord(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const QString &id)
{
    return const_cast<ZzWorkspaceShellPrivate::ZzPanelRecord *>(
        zzRecord(std::as_const(shell), runtime, id));
}

[[nodiscard]] bool zzStableGuards(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeGuards &guards)
{
    return guards.host != nullptr && guards.host == shell.host
        && guards.split != nullptr && guards.split == shell.splitWorkspace
        && guards.leftSide != nullptr && guards.leftSide == shell.leftSidePane
        && guards.rightSide != nullptr && guards.rightSide == shell.rightSidePane
        && guards.bottom != nullptr && guards.bottom == shell.bottomPane
        && guards.leftActivity != nullptr
        && guards.leftActivity == shell.leftActivityBar
        && guards.rightActivity != nullptr
        && guards.rightActivity == shell.rightActivityBar
        && guards.activityModel != nullptr
        && guards.activityModel == shell.activityModel
        && guards.titleBar == shell.titleBar;
}

[[nodiscard]] bool zzStablePanels(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime)
{
    if (shell.panels.size() != runtime.projection.identities.size()) {
        return false;
    }
    for (const auto &identity : runtime.projection.identities) {
        const auto *const record = zzRecord(shell, runtime, identity.id);
        if (record == nullptr
            || zzPanelKind(record->kind) != identity.kind
            || record->registrationGeneration
                != identity.registrationGeneration
            || record->dockIdentity != identity.rawDock
            || record->dock != identity.dock) {
            return false;
        }
        if (record->kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
            && record->materialization
                == ZzWorkspaceShellPrivate::ZzMaterializationState::Pending) {
            if (record->content != nullptr || record->contentIdentity != nullptr
                || identity.widget != nullptr || identity.rawWidget != nullptr
                || !record->factory) {
                return false;
            }
            continue;
        }
        if (record->contentIdentity != identity.rawWidget
            || record->content == nullptr
            || record->content.data() != identity.rawWidget
            || identity.widget == nullptr
            || identity.widget.data() != identity.rawWidget
            || (record->kind
                    == ZzWorkspaceShellPrivate::ZzPanelKind::Side
                && (record->materialization
                        != ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
                    || record->factory))) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 从布局 DTO 的逻辑 Side 顺序生成当前 Ready QWidget 的物理投影。
 *
 * sideEntries/Activity 保留全部逻辑 ID；SidePane 的顺序、可见项、尺寸和
 * current 只引用当前已经创建的内容。
 */
void zzKeepReadyPhysicalProjection(
    const ZzWorkspaceShellPrivate &shell,
    ZzProjection *target)
{
    QSet<QString> readyIds;
    readyIds.reserve(shell.panels.size());
    for (const auto &record : shell.panels) {
        if (record.kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
            && record.materialization
                == ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
            && record.content != nullptr
            && record.content.data() == record.contentIdentity) {
            readyIds.insert(record.id.value());
        }
    }
    const auto filter = [&readyIds](ZzSide *side) {
        QStringList order;
        for (const QString &id : std::as_const(side->order)) {
            if (readyIds.contains(id)) {
                order.append(id);
            }
        }
        QStringList visible;
        QList<int> sizes;
        for (qsizetype index = 0; index < side->visible.size(); ++index) {
            const QString &id = side->visible.at(index);
            if (readyIds.contains(id)) {
                visible.append(id);
                sizes.append(index < side->sizes.size()
                        ? side->sizes.at(index) : 1);
            }
        }
        side->order = std::move(order);
        side->visible = std::move(visible);
        side->sizes = std::move(sizes);
        if (!side->visible.contains(side->current)) {
            side->current.clear();
        }
        if (!side->current.isEmpty()) {
            const qsizetype currentIndex = side->visible.indexOf(side->current);
            const int currentSize = currentIndex >= 0
                    && currentIndex < side->sizes.size()
                ? std::max(side->sizes.at(currentIndex), 1) : 1;
            // v2 persisted multiple visible panels; the shell now has one
            // visible current panel per side.
            side->visible = {side->current};
            side->sizes = {currentSize};
        }
        side->contents.clear();
        side->contents.reserve(side->order.size());
        for (const QString &id : std::as_const(side->order)) {
            side->contents.append({id, side->stackIdentity,
                {side->paneIdentity, side->stackIdentity}});
        }
    };
    filter(&target->leftSide);
    filter(&target->rightSide);
    target->activity.leftCurrent = target->leftSide.current;
    target->activity.rightCurrent = target->rightSide.current;
    target->activity.leftActive = QSet<QString>(
        target->leftSide.visible.cbegin(), target->leftSide.visible.cend());
    target->activity.rightActive = QSet<QString>(
        target->rightSide.visible.cbegin(), target->rightSide.visible.cend());
}

/** @brief 为纯值恢复规划补回 Activity 中的完整逻辑 Side 顺序。 */
[[nodiscard]] ZzSnapshot zzLogicalPlanningSnapshot(
    const ZzRuntimeSnapshot &runtime)
{
    ZzSnapshot result = runtime.projection;
    result.leftSide.order = result.activity.leftPrimary
        + result.activity.leftSecondary;
    result.rightSide.order = result.activity.rightPrimary
        + result.activity.rightSecondary;
    return result;
}

[[nodiscard]] ZzFluentUI::ZzSidePane *zzOwningSide(
    ZzWorkspaceShellPrivate &shell,
    QWidget *content);

/** @brief 记录布局恢复本轮新建内容及已推进内部状态的原 factory。 */
struct ZzRestoreMaterialization final
{
    ZzWorkspacePanelId id;
    std::uint64_t generation = 0;
    ZzWorkspacePanelFactory factory;
    QPointer<QWidget> content;
    QWidget *contentIdentity = nullptr;
};

/**
 * @brief 创建一个目标可见的 Pending Side 内容，但保持其物理状态 hidden。
 *
 * factory 在调用后从记录移动到日志；这样成功提交会自然释放 factory，失败
 * 回滚则能恢复同一个 mutable 实例，而不是从副本重置内部状态。
 */
[[nodiscard]] ZzCore::ZzResult<void> zzMaterializeForRestore(
    ZzWorkspaceShellPrivate &shell,
    const ZzWorkspacePanelId &id,
    QVector<ZzRestoreMaterialization> *log)
{
    auto createdResult = shell.createPendingSidePanelContent(id);
    if (!createdResult) {
        return ZzCore::ZzResult<void>::failure(createdResult.error());
    }
    std::unique_ptr<QWidget> content = std::move(createdResult).value();
    int panelIndex = shell.indexOf(id);
    if (panelIndex < 0 || content == nullptr
        || shell.panels.at(panelIndex).materialization
            != ZzWorkspaceShellPrivate::ZzMaterializationState::Materializing
        || !shell.panels.at(panelIndex).factory) {
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel registration changed during creation"));
    }
    const std::uint64_t generation =
        shell.panels.at(panelIndex).registrationGeneration;
    const auto restorePending = [&] {
        panelIndex = shell.indexOf(id);
        if (panelIndex >= 0
            && shell.panels.at(panelIndex).registrationGeneration == generation
            && shell.panels.at(panelIndex).materialization
                == ZzWorkspaceShellPrivate::ZzMaterializationState::Materializing) {
            shell.panels[panelIndex].materialization =
                ZzWorkspaceShellPrivate::ZzMaterializationState::Pending;
            shell.panels[panelIndex].registrationInProgress = false;
        }
    };
    const QPointer<QWidget> contentGuard(content.get());
    contentGuard->hide();
    panelIndex = shell.indexOf(id);
    const bool contentDestroyed = contentGuard == nullptr;
    const bool contentParented = !contentDestroyed
        && contentGuard->parent() != nullptr;
    const bool wrongThread = !contentDestroyed
        && contentGuard->thread() != QThread::currentThread();
    if (contentDestroyed || panelIndex < 0 || shell.host == nullptr
        || shell.panels.at(panelIndex).registrationGeneration != generation
        || shell.panels.at(panelIndex).materialization
            != ZzWorkspaceShellPrivate::ZzMaterializationState::Materializing
        || !shell.panels.at(panelIndex).factory
        || contentParented || wrongThread
        || contentGuard->thread() != shell.host->thread()
        || contentGuard->isVisible()) {
        restorePending();
        if (contentDestroyed) {
            [[maybe_unused]] QWidget *const destroyedContent = content.release();
        } else if (wrongThread) {
            QWidget *const foreignContent = content.release();
            static_cast<void>(QMetaObject::invokeMethod(
                foreignContent, &QObject::deleteLater,
                Qt::QueuedConnection));
        }
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel content is not hidden for restore"));
    }
    ZzWorkspacePanelFactory factory =
        std::move(shell.panels[panelIndex].factory);
    auto adopted = shell.adoptSidePanelContent(
        id, generation, content.get(), false);
    if (!adopted) {
        panelIndex = shell.indexOf(id);
        if (panelIndex >= 0
            && shell.panels.at(panelIndex).registrationGeneration
                == generation) {
            shell.panels[panelIndex].factory = std::move(factory);
            shell.panels[panelIndex].materialization =
                ZzWorkspaceShellPrivate::ZzMaterializationState::Pending;
            shell.panels[panelIndex].registrationInProgress = false;
        }
        if (contentGuard == nullptr) {
            [[maybe_unused]] QWidget *const destroyedContent = content.release();
        }
        return adopted;
    }
    [[maybe_unused]] QWidget *const adoptedContent = content.release();
    log->append({id, generation, std::move(factory),
        contentGuard, contentGuard.data()});
    panelIndex = shell.indexOf(id);
    if (panelIndex < 0 || contentGuard == nullptr
        || shell.panels.at(panelIndex).registrationGeneration != generation
        || shell.panels.at(panelIndex).materialization
            != ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
        || shell.panels.at(panelIndex).content != contentGuard
        || shell.panels.at(panelIndex).contentIdentity != contentGuard.data()) {
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel materialization commit was interrupted"));
    }
    return ZzCore::ZzResult<void>::success();
}

/**
 * @brief 逆序撤销本轮新建内容，恢复 Pending 记录与原 mutable factory。
 *
 * 只销毁仍由对应 SidePane 托管且处于 Shell GUI 线程的内容；所有权已被
 * 外部污染时失败闭合，不跨线程 direct delete。
 */
[[nodiscard]] bool zzRollbackMaterializations(
    ZzWorkspaceShellPrivate &shell,
    QVector<ZzRestoreMaterialization> *log)
{
    bool complete = true;
    for (qsizetype position = log->size(); position > 0; --position) {
        ZzRestoreMaterialization &created = (*log)[position - 1];
        int panelIndex = shell.indexOf(created.id);
        if (panelIndex < 0 || created.content == nullptr
            || created.content.data() != created.contentIdentity
            || created.content->thread() != QThread::currentThread()
            || shell.panels.at(panelIndex).registrationGeneration
                != created.generation
            || shell.panels.at(panelIndex).materialization
                != ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
            || shell.panels.at(panelIndex).content != created.content
            || shell.panels.at(panelIndex).contentIdentity
                != created.contentIdentity) {
            complete = false;
            continue;
        }
        QObject::disconnect(
            shell.panels[panelIndex].contentDestroyedConnection);
        shell.panels[panelIndex].removalInProgress = true;
        ZzFluentUI::ZzSidePane *const pane =
            zzOwningSide(shell, created.contentIdentity);
        QWidget *const taken = pane != nullptr
            ? pane->takeWidget(created.contentIdentity) : nullptr;
        panelIndex = shell.indexOf(created.id);
        if (taken != created.contentIdentity || created.content == nullptr
            || taken == nullptr || taken->parent() != nullptr
            || panelIndex < 0
            || shell.panels.at(panelIndex).registrationGeneration
                != created.generation) {
            if (panelIndex >= 0 && created.content != nullptr) {
                shell.panels[panelIndex].removalInProgress = false;
                shell.connectPanelContentDestroyed(
                    created.id, created.content.data());
            }
            complete = false;
            continue;
        }
        auto &record = shell.panels[panelIndex];
        record.content = nullptr;
        record.contentIdentity = nullptr;
        record.contentOwner = nullptr;
        record.contentOwnerIdentity = nullptr;
        record.contentDestroyedConnection = {};
        record.factory = std::move(created.factory);
        record.materialization =
            ZzWorkspaceShellPrivate::ZzMaterializationState::Pending;
        record.registrationInProgress = false;
        record.removalInProgress = false;
        std::unique_ptr<QWidget> owned(taken);
    }
    shell.syncSideEdgeVisibility();
    return complete;
}

[[nodiscard]] QStringList zzIds(
    const QList<QWidget *> &widgets,
    const ZzRuntimeIndexes &indexes)
{
    QStringList result;
    result.reserve(widgets.size());
    for (QWidget *const widget : widgets) {
        const QString id = indexes.widgetIds.value(widget);
        if (!id.isEmpty()) {
            result.append(id);
        }
    }
    return result;
}

[[nodiscard]] bool zzSideOwnerMatches(
    const ZzSideOwnerIndex &owners,
    const QString &id,
    QWidget *content,
    ZzFluentUI::ZzPanelStack *stack)
{
    const auto owner = owners.constFind(id);
    return owner != owners.cend() && content != nullptr && stack != nullptr
        && owner->object != nullptr
        && owner->object.data() == owner->rawObject
        && content->parentWidget() == owner->rawObject
        && stack->isAncestorOf(owner->object.data());
}

[[nodiscard]] bool zzAuditSide(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzSide &expected,
    ZzFluentUI::ZzSidePane *pane,
    const ZzRuntimeIndexes &indexes,
    const ZzSideOwnerIndex &owners)
{
    if (pane == nullptr || expected.paneIdentity.object == nullptr
        || expected.paneIdentity.object.data() != expected.paneIdentity.rawObject
        || pane != expected.paneIdentity.rawObject
        || pane->panelStack() == nullptr
        || expected.stackIdentity.object == nullptr
        || pane->panelStack() != expected.stackIdentity.rawObject) {
        return false;
    }
    const QList<QWidget *> physicalPanels = pane->panelStack()->panels();
    const QList<QWidget *> physicalVisible = pane->visibleWidgets();
    if (physicalPanels.size() != expected.order.size()
        || physicalVisible.size() != expected.visible.size()) {
        return false;
    }
    const QStringList observedOrder = zzIds(physicalPanels, indexes);
    const QStringList observedVisible = zzIds(physicalVisible, indexes);
    const QList<int> observedSizes = pane->panelStack()->panelSizes();
    const QString observedCurrent = zzIds(
        {pane->currentWidget()}, indexes).value(0);
    if (observedOrder != expected.order
        || observedVisible != expected.visible
        || observedSizes != expected.sizes
        || observedCurrent != expected.current
        || pane->isCollapsed() != expected.collapsed
        || pane->paneWidth() != expected.width) {
        return false;
    }
    for (const QString &id : expected.order) {
        const auto *const record = zzRecord(shell, runtime, id);
        if (record == nullptr || record->content == nullptr
            || !zzSideOwnerMatches(
                owners, id, record->content, pane->panelStack())
            || !pane->isAncestorOf(record->content)
            || !pane->panelStack()->isAncestorOf(record->content)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool zzAuditBottom(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzLayoutState::ZzBottomProjection &expected,
    const ZzRuntimeIndexes &indexes)
{
    auto *const pane = shell.bottomPane.data();
    auto *const stack = pane != nullptr
        ? pane->findChild<QStackedWidget *>() : nullptr;
    if (pane == nullptr || stack == nullptr
        || expected.paneIdentity.object == nullptr
        || expected.paneIdentity.rawObject != pane
        || expected.stackIdentity.object == nullptr
        || expected.stackIdentity.rawObject != stack
        || pane->paneHeight() != expected.height
        || pane->isCollapsed() != expected.collapsed) {
        return false;
    }
    QStringList observedOrder;
        observedOrder.reserve(stack->count());
    for (int index = 0; index < stack->count(); ++index) {
        observedOrder.append(zzIds(
            {stack->widget(index)}, indexes).value(0));
    }
    if (pane->widgetCount() != expected.order.size()
        || observedOrder != expected.order) {
        return false;
    }
    const QString current = zzIds(
        {pane->currentWidget()}, indexes).value(0);
    if (!expected.current.isEmpty() && current != expected.current) {
        return false;
    }
    for (qsizetype index = 0; index < expected.order.size(); ++index) {
        const auto *const record = zzRecord(
            shell, runtime, expected.order.at(index));
        if (record == nullptr || record->content == nullptr
            || stack->indexOf(record->content) != index
            || !pane->isAncestorOf(record->content)
            || !stack->isAncestorOf(record->content)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool zzAuditDock(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzLayoutState::ZzDockProjection &expected)
{
    if (shell.host == nullptr) {
        return false;
    }
    for (const auto &placement : expected.docks) {
        const auto *const record = zzRecord(shell, runtime, placement.panel.id);
        auto *const dock = record != nullptr
            ? qobject_cast<ZzFluentUI::ZzDockPanel *>(record->dock.data())
            : nullptr;
        if (record == nullptr || dock == nullptr
            || record->dockIdentity != placement.panel.rawDock
            || shell.host->dockWidgetArea(dock) != placement.area
            || dock->isFloating() != placement.floating
            || (!dock->isHidden()) != placement.visible
            || dock->widget() != record->content
            || record->content == nullptr
            || !dock->isAncestorOf(record->content)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool zzAuditSplit(
    const ZzWorkspaceShellPrivate &shell,
    const ZzLayoutState::ZzSplitProjection &expected,
    const QString &migrationGroup = {},
    int migrationCurrent = -1)
{
    if (shell.splitWorkspace == nullptr) {
        return false;
    }
    auto canonical = ZzWorkspaceLayoutCodecPrivate::canonicalizeSplit(
        shell.splitWorkspace->saveLayout());
    if (!canonical || canonical.value() != expected.canonicalState) {
        return false;
    }
    if (migrationCurrent >= 0) {
        auto *const tabs = shell.splitWorkspace->tabWidget(
            ZzFluentUI::ZzTabGroupId(migrationGroup));
        return tabs != nullptr && tabs->currentIndex() == migrationCurrent;
    }
    return true;
}

[[nodiscard]] const QStringList *zzActivityRows(
    const ZzLayoutState::ZzActivityProjection &activity,
    ZzFluentUI::ZzActivityArea area)
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

[[nodiscard]] bool zzAuditActivity(
    const ZzWorkspaceShellPrivate &shell,
    const ZzLayoutState::ZzActivityProjection &expected,
    const ZzRuntimeIndexes &indexes)
{
    if (shell.activityModel == nullptr || shell.leftActivityBar == nullptr
        || shell.rightActivityBar == nullptr
        || expected.modelIdentity.object == nullptr
        || expected.modelIdentity.rawObject != shell.activityModel) {
        return false;
    }
    std::array<QStringList, 4> actual;
    for (const auto &row : shell.activityRows()) {
        actual.at(static_cast<std::size_t>(row.area)).append(row.id.value());
    }
    const auto idAtIndex = [&indexes](const QModelIndex &index) {
        return index.isValid()
            ? indexes.activityIds.value(index.row()) : QString{};
    };
    const auto idsAtIndexes = [&indexes](const QList<QModelIndex> &items) {
        QSet<QString> result;
        for (const QModelIndex &index : items) {
            if (index.isValid()) {
                const QString id = indexes.activityIds.value(index.row());
                if (!id.isEmpty()) {
                    result.insert(id);
                }
            }
        }
        return result;
    };
    const bool leftHasPanel = !expected.leftPrimary.isEmpty()
        || !expected.leftSecondary.isEmpty();
    const bool rightHasPanel = !expected.rightPrimary.isEmpty()
        || !expected.rightSecondary.isEmpty();
    if (actual.at(0) != expected.leftPrimary
        || actual.at(1) != expected.leftSecondary
        || actual.at(2) != expected.rightPrimary
        || actual.at(3) != expected.rightSecondary
        || shell.leftActivityBar->isHidden() == leftHasPanel
        || shell.rightActivityBar->isHidden() == rightHasPanel
        || idAtIndex(shell.leftActivityBar->currentSourceIndex())
            != expected.leftCurrent
        || idAtIndex(shell.rightActivityBar->currentSourceIndex())
            != expected.rightCurrent
        || idsAtIndexes(shell.leftActivityBar->activeSourceIndexes())
            != expected.leftActive
        || idsAtIndexes(shell.rightActivityBar->activeSourceIndexes())
            != expected.rightActive) {
        return false;
    }
    return true;
}

[[nodiscard]] bool zzAuditAll(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzProjection &expected,
    const ZzSideOwnerIndex &owners,
    const QString &migrationGroup = {},
    int migrationCurrent = -1)
{
    const ZzRuntimeIndexes indexes = zzBuildRuntimeIndexes(shell, runtime);
    return zzStableGuards(shell, runtime.guards)
        && zzStablePanels(shell, runtime)
        && zzAuditDock(shell, runtime, expected.dock)
        && zzAuditSplit(
            shell, expected.split, migrationGroup, migrationCurrent)
        && zzAuditSide(
            shell, runtime, expected.leftSide, shell.leftSidePane, indexes,
            owners)
        && zzAuditSide(
            shell, runtime, expected.rightSide, shell.rightSidePane, indexes,
            owners)
        && zzAuditBottom(shell, runtime, expected.bottom, indexes)
        && zzAuditActivity(shell, expected.activity, indexes)
        && shell.titleMode == zzTitleMode(expected.title.mode)
        && shell.host != nullptr
        && shell.host->windowTitle() == zzEffectiveTitle(shell)
        && (shell.titleBar == nullptr
            || shell.titleBar->title() == zzEffectiveTitle(shell));
}

[[nodiscard]] bool zzBuildDockTarget(
    const ZzRuntimeSnapshot &runtime,
    ZzLayoutState::ZzDockProjection *target)
{
    QMainWindow shadow;
    QHash<QString, QDockWidget *> docks;
    for (const auto &placement : runtime.projection.dock.docks) {
        auto *const dock = new QDockWidget(&shadow);
        dock->setObjectName(
            placement.panel.rawDock != nullptr
                ? placement.panel.rawDock->objectName() : QString{});
        shadow.addDockWidget(placement.area, dock);
        dock->setFloating(placement.floating);
        dock->setVisible(placement.visible);
        docks.insert(placement.panel.id, dock);
    }
    if (!shadow.restoreState(target->state, zzQtDockStateVersion)) {
        return false;
    }
    target->docks.clear();
    target->visible.clear();
    for (const auto &snapshotDock : runtime.projection.dock.docks) {
        QDockWidget *const dock = docks.value(snapshotDock.panel.id);
        if (dock == nullptr) {
            return false;
        }
        ZzLayoutState::ZzDockPlacement placement = snapshotDock;
        placement.area = shadow.dockWidgetArea(dock);
        placement.floating = dock->isFloating();
        placement.visible = !dock->isHidden();
        target->docks.append(placement);
        if (placement.visible) {
            target->visible.append(placement.panel.id);
        }
    }
    return true;
}

[[nodiscard]] bool zzSkipSplitString(QDataStream &stream)
{
    quint16 length = 0;
    stream >> length;
    for (quint16 index = 0; index < length; ++index) {
        quint16 character = 0;
        stream >> character;
    }
    return stream.status() == QDataStream::Ok;
}

[[nodiscard]] bool zzSkipSplitNode(QDataStream &stream, int depth)
{
    quint8 kind = 0;
    stream >> kind;
    if (stream.status() != QDataStream::Ok || depth > 16) {
        return false;
    }
    if (kind == 0) {
        return zzSkipSplitString(stream);
    }
    quint8 orientation = 0;
    quint16 childCount = 0;
    stream >> orientation >> childCount;
    if (stream.status() != QDataStream::Ok || kind != 1
        || childCount < 2 || childCount > 64) {
        return false;
    }
    for (quint16 index = 0; index < childCount; ++index) {
        if (!zzSkipSplitNode(stream, depth + 1)) {
            return false;
        }
    }
    quint16 sizeCount = 0;
    stream >> sizeCount;
    if (stream.status() != QDataStream::Ok || sizeCount != childCount) {
        return false;
    }
    for (quint16 index = 0; index < sizeCount; ++index) {
        qint32 size = 0;
        stream >> size;
    }
    return stream.status() == QDataStream::Ok;
}

[[nodiscard]] std::optional<QByteArray> zzVersionOneSplitTarget(
    ZzLayoutState::ZzSplitProjection *target,
    const QString &migrationGroup,
    int migrationCurrent)
{
    if (target == nullptr || target->canonicalState.size() < 44) {
        return std::nullopt;
    }
    QDataStream envelope(target->canonicalState);
    envelope.setVersion(zzStreamVersion);
    char magic[4]{};
    quint16 schema = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (envelope.readRawData(magic, 4) != 4) {
        return std::nullopt;
    }
    envelope >> schema >> streamVersion >> payloadLength;
    if (QByteArrayView(magic, 4) != QByteArrayView("ZZSW", 4)
        || schema != 1
        || streamVersion != static_cast<quint16>(zzStreamVersion)
        || payloadLength != static_cast<quint32>(
            target->canonicalState.size() - 44)) {
        return std::nullopt;
    }
    QByteArray payload = target->canonicalState.mid(
        12, static_cast<qsizetype>(payloadLength));
    QBuffer buffer(&payload);
    if (!buffer.open(QIODevice::ReadWrite)) {
        return std::nullopt;
    }
    QDataStream stream(&buffer);
    stream.setVersion(zzStreamVersion);
    if (!zzSkipSplitNode(stream, 1)
        || !zzSkipSplitString(stream)) {
        return std::nullopt;
    }
    quint16 pageCount = 0;
    stream >> pageCount;
    if (stream.status() != QDataStream::Ok || pageCount > 4096) {
        return std::nullopt;
    }
    for (quint16 index = 0; index < pageCount; ++index) {
        QString key;
        QString group;
        const auto readString = [&stream](QString *value) {
            quint16 length = 0;
            stream >> length;
            value->clear();
            value->reserve(length);
            for (quint16 characterIndex = 0;
                 characterIndex < length; ++characterIndex) {
                quint16 character = 0;
                stream >> character;
                value->append(QChar(character));
            }
            return stream.status() == QDataStream::Ok;
        };
        qint32 order = 0;
        quint8 current = 0;
        if (!readString(&key) || !readString(&group)) {
            return std::nullopt;
        }
        stream >> order;
        const qint64 currentPosition = buffer.pos();
        stream >> current;
        if (stream.status() != QDataStream::Ok || current > 1) {
            return std::nullopt;
        }
        const quint8 desired = group == migrationGroup
            && order == migrationCurrent ? 1 : 0;
        if (group == migrationGroup && current != desired) {
            if (!buffer.seek(currentPosition)) {
                return std::nullopt;
            }
            stream << desired;
        }
        if (!buffer.seek(currentPosition + 1)) {
            return std::nullopt;
        }
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
        return std::nullopt;
    }
    for (auto &page : target->savedPages) {
        if (page.groupId == migrationGroup) {
            page.current = page.order == migrationCurrent;
        }
    }
    QByteArray encoded = target->canonicalState.left(12);
    encoded.append(payload);
    encoded.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    auto canonical = ZzWorkspaceLayoutCodecPrivate::canonicalizeSplit(encoded);
    if (!canonical || canonical.value() != encoded) {
        return std::nullopt;
    }
    return encoded;
}

[[nodiscard]] bool zzCanonicalizeSplitTarget(
    ZzFluentUI::ZzSplitWorkspace *observed,
    ZzLayoutState::ZzSplitProjection *target,
    const QString &migrationGroup = {},
    int migrationCurrent = -1)
{
    if (observed == nullptr) {
        return false;
    }
    if (!migrationGroup.isEmpty()) {
        auto *const tabs = observed->tabWidget(
            ZzFluentUI::ZzTabGroupId(migrationGroup));
        if (tabs == nullptr || migrationCurrent >= tabs->count()) {
            return false;
        }
        auto canonical = zzVersionOneSplitTarget(
            target, migrationGroup, migrationCurrent);
        if (!canonical.has_value()) {
            return false;
        }
        target->canonicalState = std::move(*canonical);
        return true;
    }
    ZzFluentUI::ZzSplitWorkspace shadow;
    shadow.resize(observed->size());
    auto *const initialTabs = shadow.tabWidget(shadow.activeGroupId());
    if (initialTabs == nullptr) {
        return false;
    }
    for (const auto &page : target->savedPages) {
        auto *const dummy = new QWidget;
        initialTabs->addTab(dummy, page.key);
        if (!shadow.setPageLayoutKey(dummy, page.key)) {
            return false;
        }
    }
    if (!shadow.restoreLayout(target->canonicalState)) {
        return false;
    }
    auto canonical = ZzWorkspaceLayoutCodecPrivate::canonicalizeSplit(
        shadow.saveLayout());
    if (!canonical) {
        return false;
    }
    target->canonicalState = std::move(canonical).value();
    return true;
}

class ZzLayoutTransactionScope final
{
public:
    explicit ZzLayoutTransactionScope(
        ZzWorkspaceShellPrivate &shell) noexcept
        : shell_(shell)
    {
        shell_.transactionKind =
            ZzWorkspaceShellPrivate::ZzTransactionKind::LayoutRestore;
    }

    ~ZzLayoutTransactionScope()
    {
        shell_.transactionKind =
            ZzWorkspaceShellPrivate::ZzTransactionKind::None;
    }

private:
    ZzWorkspaceShellPrivate &shell_;
};

[[nodiscard]] bool zzApplyDock(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzLayoutState::ZzDockProjection &target)
{
    return shell.host != nullptr
        && shell.host->restoreState(target.state, zzQtDockStateVersion)
        && zzStableGuards(shell, runtime.guards)
        && zzStablePanels(shell, runtime)
        && zzAuditDock(shell, runtime, target);
}

[[nodiscard]] bool zzApplySplit(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzLayoutState::ZzSplitProjection &target,
    const QString &migrationGroup,
    int migrationCurrent,
    const QByteArray &alternateCanonical = {})
{
    const QPointer<ZzFluentUI::ZzSplitWorkspace> guard(shell.splitWorkspace);
    std::optional<QByteArray> observedCanonical;
    if (guard != nullptr) {
        auto canonical = ZzWorkspaceLayoutCodecPrivate::canonicalizeSplit(
            guard->saveLayout());
        if (canonical) {
            observedCanonical = std::move(canonical).value();
        }
    }
    const bool preservesExistingTree = !migrationGroup.isEmpty();
    const bool alreadyMatches = observedCanonical.has_value()
        && *observedCanonical == target.canonicalState;
    const bool acceptableExistingTree = preservesExistingTree
        && observedCanonical.has_value()
        && (alreadyMatches
            || *observedCanonical == alternateCanonical);
    const bool restored = acceptableExistingTree || alreadyMatches
        || (guard != nullptr && guard->restoreLayout(target.canonicalState));
    if (guard == nullptr || !restored
        || !zzStableGuards(shell, runtime.guards)) {
        return false;
    }
    if (migrationCurrent >= 0) {
        auto *const tabs = guard->tabWidget(
            ZzFluentUI::ZzTabGroupId(migrationGroup));
        if (tabs == nullptr || migrationCurrent >= tabs->count()) {
            return false;
        }
        tabs->setCurrentIndex(migrationCurrent);
        if (guard == nullptr || !zzStableGuards(shell, runtime.guards)) {
            return false;
        }
    }
    return zzStablePanels(shell, runtime)
        && zzAuditSplit(
            shell, target, migrationGroup, migrationCurrent);
}

[[nodiscard]] ZzFluentUI::ZzSidePane *zzOwningSide(
    ZzWorkspaceShellPrivate &shell,
    QWidget *content)
{
    for (ZzFluentUI::ZzSidePane *const pane : {
             shell.leftSidePane.data(), shell.rightSidePane.data()}) {
        if (pane != nullptr && pane->panelStack() != nullptr
            && pane->isAncestorOf(content)
            && pane->panelStack()->isAncestorOf(content)) {
            return pane;
        }
    }
    return nullptr;
}

[[nodiscard]] bool zzApplySide(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzProjection &target,
    bool strict,
    ZzSideOwnerObserver &ownerObserver,
    const ZzSideOwnerIndex &owners)
{
    bool complete = true;
    const auto stableBoundary = [&] {
        return ownerObserver.isValid()
            && zzStableGuards(shell, runtime.guards);
    };
    const auto place = [&](const ZzSide &side,
                           ZzFluentUI::ZzSidePane *destination) {
        const QPointer<ZzFluentUI::ZzSidePane> destinationGuard(destination);
        for (const QString &id : side.order) {
            auto *const record = zzRecord(shell, runtime, id);
            if (record == nullptr || record->content == nullptr
                || record->content.data() != record->contentIdentity) {
                complete = false;
                if (strict) return false;
                continue;
            }
            const QPointer<QWidget> content(record->content);
            ZzFluentUI::ZzSidePane *const owner =
                zzOwningSide(shell, content);
            if (owner == nullptr && content->parent() != nullptr) {
                complete = false;
                if (strict) return false;
                continue;
            }
            if (owner != destinationGuard) {
                if (owner != nullptr) {
                    ownerObserver.beginDetach(id, content);
                    QWidget *const taken = owner->takeWidget(content);
                    ownerObserver.finishMutation();
                    if (taken != content || content == nullptr
                        || content->parent() != nullptr
                        || !stableBoundary()) {
                        complete = false;
                        if (strict) return false;
                        continue;
                    }
                }
                ownerObserver.beginAttach(
                    id, content,
                    destinationGuard != nullptr
                        ? destinationGuard->panelStack() : nullptr);
                const bool added = destinationGuard != nullptr
                    && content != nullptr
                    && destinationGuard->addWidget(content, record->title);
                ownerObserver.finishMutation();
                if (!added
                    || destinationGuard == nullptr || content == nullptr
                    || !zzSideOwnerMatches(
                        owners, id, content, destinationGuard->panelStack())
                    || !destinationGuard->isAncestorOf(content)
                    || !destinationGuard->panelStack()->isAncestorOf(content)
                    || !stableBoundary()) {
                    complete = false;
                    if (strict) return false;
                    continue;
                }
            }
        }
        if (destinationGuard == nullptr) {
            complete = false;
            return false;
        }
        for (qsizetype index = 0; index < side.order.size(); ++index) {
            const auto *const record = zzRecord(shell, runtime, side.order.at(index));
            if (record == nullptr || record->content == nullptr
                || !destinationGuard->panelStack()->movePanel(
                    record->content, static_cast<int>(index))
                || destinationGuard == nullptr
                || !stableBoundary()) {
                complete = false;
                if (strict) return false;
            }
        }
        for (const QString &id : side.order) {
            const auto *const record = zzRecord(shell, runtime, id);
            if (record == nullptr || record->content == nullptr
                || !destinationGuard->setWidgetVisible(
                    record->content, side.visible.contains(id))
                || destinationGuard == nullptr
                || !stableBoundary()) {
                complete = false;
                if (strict) return false;
            }
        }
        if (!side.current.isEmpty()) {
            const auto *const current = zzRecord(shell, runtime, side.current);
            if (current == nullptr || current->content == nullptr
                || !destinationGuard->setCurrentWidget(current->content)
                || destinationGuard == nullptr
                || !stableBoundary()) {
                complete = false;
                if (strict) return false;
            }
        }
        const bool sizesApplied = side.sizes.isEmpty()
            ? true
            : destinationGuard->panelStack()->setPanelSizes(side.sizes);
        if (!sizesApplied || destinationGuard == nullptr
            || !stableBoundary()) {
            complete = false;
            if (strict) return false;
        }
        destinationGuard->setPaneWidth(side.width);
        if (destinationGuard == nullptr
            || !stableBoundary()) {
            complete = false;
            if (strict) return false;
        }
        destinationGuard->setCollapsed(side.collapsed);
        if (destinationGuard == nullptr
            || !stableBoundary()) {
            complete = false;
            if (strict) return false;
        }
        return true;
    };
    static_cast<void>(place(target.leftSide, shell.leftSidePane));
    static_cast<void>(place(target.rightSide, shell.rightSidePane));
    const ZzRuntimeIndexes indexes = zzBuildRuntimeIndexes(shell, runtime);
    const bool audited = ownerObserver.isValid()
        && zzStablePanels(shell, runtime)
        && zzAuditSide(
            shell, runtime, target.leftSide, shell.leftSidePane, indexes,
            owners)
        && zzAuditSide(
            shell, runtime, target.rightSide, shell.rightSidePane, indexes,
            owners);
    return complete && audited;
}

[[nodiscard]] bool zzApplyBottom(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzLayoutState::ZzBottomProjection &target,
    bool allowPaneOwnedDetached = false)
{
    const QPointer<ZzFluentUI::ZzBottomPane> pane(shell.bottomPane);
    const QPointer<QStackedWidget> stack = pane != nullptr
        ? pane->findChild<QStackedWidget *>() : nullptr;
    if (pane == nullptr || stack == nullptr) {
        return false;
    }
    const ZzRuntimeIndexes indexes = zzBuildRuntimeIndexes(shell, runtime);
    QStringList observedOrder;
    observedOrder.reserve(stack->count());
    for (int index = 0; index < stack->count(); ++index) {
        observedOrder.append(zzIds(
            {stack->widget(index)}, indexes).value(0));
    }
    if (observedOrder != target.order
        || pane->widgetCount() != target.order.size()) {
        QList<QPointer<QWidget>> contents;
        contents.reserve(target.order.size());
        for (const QString &id : target.order) {
            const auto *const record = zzRecord(shell, runtime, id);
            if (record == nullptr || record->content == nullptr) {
                return false;
            }
            contents.append(record->content);
        }
        for (const QPointer<QWidget> &content : std::as_const(contents)) {
            if (content == nullptr) {
                return false;
            }
            if (stack->indexOf(content) >= 0) {
                QWidget *const taken = pane->takeWidget(content);
                if (taken != content || pane == nullptr || stack == nullptr
                    || content == nullptr || content->parent() != nullptr
                    || !zzStableGuards(shell, runtime.guards)
                    || !zzStablePanels(shell, runtime)) {
                    return false;
                }
            } else {
                // A registered Bottom content that remains owned by the pane
                // but is absent from the stack is invalid during forward
                // commit. Rollback may only repair the Qt removeWidget case,
                // where the stack still owns the content.
                if (!allowPaneOwnedDetached
                    || content->parent() != stack
                    || !pane->isAncestorOf(content)) {
                    return false;
                }
                content->setParent(nullptr);
                if (pane == nullptr || stack == nullptr || content == nullptr
                    || content->parent() != nullptr
                    || !zzStableGuards(shell, runtime.guards)
                    || !zzStablePanels(shell, runtime)) {
                    return false;
                }
            }
        }
        for (qsizetype index = 0; index < target.order.size(); ++index) {
            const auto *const record = zzRecord(
                shell, runtime, target.order.at(index));
            const QPointer<QWidget> &content = contents.at(index);
            if (record == nullptr || content == nullptr
                || !pane->addWidget(content, record->title, record->icon)
                || pane == nullptr || stack == nullptr || content == nullptr
                || stack->indexOf(content) != index
                || !zzStableGuards(shell, runtime.guards)
                || !zzStablePanels(shell, runtime)) {
                return false;
            }
        }
    }
    if (!target.current.isEmpty()) {
        const auto *const current = zzRecord(shell, runtime, target.current);
        if (current == nullptr || current->content == nullptr
            || !pane->setCurrentWidget(current->content)
            || pane == nullptr || !zzStableGuards(shell, runtime.guards)) {
            return false;
        }
    }
    pane->setPaneHeight(target.height);
    if (pane == nullptr || !zzStableGuards(shell, runtime.guards)
        || pane->paneHeight() != target.height) {
        return false;
    }
    pane->setCollapsed(target.collapsed);
    return pane != nullptr && zzStableGuards(shell, runtime.guards)
        && zzStablePanels(shell, runtime)
        && zzAuditBottom(shell, runtime, target, indexes);
}

[[nodiscard]] QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry>
zzRowsForTarget(const ZzProjection &target)
{
    QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> result;
    for (const auto area : {
             ZzFluentUI::ZzActivityArea::LeftPrimary,
             ZzFluentUI::ZzActivityArea::LeftSecondary,
             ZzFluentUI::ZzActivityArea::RightPrimary,
             ZzFluentUI::ZzActivityArea::RightSecondary}) {
        const QStringList &ids = *zzActivityRows(target.activity, area);
        for (qsizetype index = 0; index < ids.size(); ++index) {
            result.append({ZzWorkspacePanelId(ids.at(index)), area,
                static_cast<int>(result.size())});
        }
    }
    return result;
}

[[nodiscard]] QModelIndex zzIndexForId(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeIndexes &indexes,
    const QString &id)
{
    const auto found = indexes.activityRows.constFind(id);
    if (found != indexes.activityRows.cend()
        && shell.activityModel != nullptr) {
        return shell.activityModel->index(found.value(), 0);
    }
    return {};
}

[[nodiscard]] QList<QModelIndex> zzIndexesForIds(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeIndexes &indexes,
    const QStringList &ids)
{
    QList<QModelIndex> result;
    result.reserve(ids.size());
    for (const QString &id : ids) {
        const QModelIndex index = zzIndexForId(shell, indexes, id);
        if (index.isValid()) {
            result.append(index);
        }
    }
    return result;
}

[[nodiscard]] bool zzApplyActivityAndTitle(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzProjection &target)
{
    for (const auto area : {
             ZzFluentUI::ZzActivityArea::LeftPrimary,
             ZzFluentUI::ZzActivityArea::LeftSecondary,
             ZzFluentUI::ZzActivityArea::RightPrimary,
             ZzFluentUI::ZzActivityArea::RightSecondary}) {
        for (const QString &id : *zzActivityRows(target.activity, area)) {
            auto *const record = zzRecord(shell, runtime, id);
            if (record == nullptr) {
                return false;
            }
            record->activityArea = area;
        }
    }
    const auto rows = zzRowsForTarget(target);
    if (rows.size() != shell.activityRows().size()
        || !shell.replaceActivityRows(rows)
        || !zzStableGuards(shell, runtime.guards)
        || !zzStablePanels(shell, runtime)) {
        return false;
    }
    shell.syncSideEdgeVisibility();
    if (!zzStableGuards(shell, runtime.guards)
        || !zzStablePanels(shell, runtime)) {
        return false;
    }
    const ZzRuntimeIndexes indexes = zzBuildRuntimeIndexes(shell, runtime);
    shell.leftActivityBar->setCurrentSourceIndex(
        zzIndexForId(shell, indexes, target.activity.leftCurrent));
    if (!zzStableGuards(shell, runtime.guards)) return false;
    shell.rightActivityBar->setCurrentSourceIndex(
        zzIndexForId(shell, indexes, target.activity.rightCurrent));
    if (!zzStableGuards(shell, runtime.guards)) return false;
    shell.leftActivityBar->setActiveSourceIndexes(
        zzIndexesForIds(shell, indexes, target.leftSide.visible));
    if (!zzStableGuards(shell, runtime.guards)) return false;
    shell.rightActivityBar->setActiveSourceIndexes(
        zzIndexesForIds(shell, indexes, target.rightSide.visible));
    if (!zzStableGuards(shell, runtime.guards)
        || !zzAuditActivity(shell, target.activity, indexes)) {
        return false;
    }
    shell.titleMode = zzTitleMode(target.title.mode);
    shell.refreshCurrentTabConnection();
    return zzStableGuards(shell, runtime.guards)
        && shell.titleMode == zzTitleMode(target.title.mode)
        && shell.host != nullptr
        && shell.host->windowTitle() == zzEffectiveTitle(shell)
        && (shell.titleBar == nullptr
            || shell.titleBar->title() == zzEffectiveTitle(shell));
}

/** @brief 回滚后按实际合法 owner 修复 Activity；第三方 owner 只清托管。 */
void zzSynchronizeAfterFailedRollback(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime)
{
    struct ZzRemoval final
    {
        ZzWorkspacePanelId id;
        QPointer<QWidget> content;
        QWidget *contentIdentity = nullptr;
        quint64 registrationGeneration = 0;
        ZzLayoutState::ZzPanelKind kind =
            ZzLayoutState::ZzPanelKind::Side;
        std::optional<ZzWorkspaceShellPrivate::ZzActivityRowSnapshot>
            activityRow;
        int takeAttempts = 0;
    };
    struct ZzPhysicalSideState final
    {
        bool auditable = false;
        qsizetype occurrences = 0;
        QVector<QPointer<ZzFluentUI::ZzSidePane>> containingPanes;
        QPointer<ZzFluentUI::ZzSidePane> validOwner;
    };
    QHash<QString, int> currentPanelRows;
    currentPanelRows.reserve(shell.panels.size());
    for (qsizetype index = 0; index < shell.panels.size(); ++index) {
        currentPanelRows.insert(
            shell.panels.at(index).id.value(), static_cast<int>(index));
    }
    QVector<ZzRemoval> removals;
    for (const auto &identity : runtime.projection.identities) {
        const int row = currentPanelRows.value(identity.id, -1);
        if (row < 0 || row >= shell.panels.size()) {
            continue;
        }
        auto &record = shell.panels[row];
        if (record.id.value() != identity.id
            || zzPanelKind(record.kind) != identity.kind
            || record.contentIdentity != identity.rawWidget
            || record.registrationGeneration
                != identity.registrationGeneration
            || record.content == nullptr
            || record.content.data() != identity.rawWidget
            || identity.widget == nullptr
            || identity.widget.data() != identity.rawWidget) {
            continue;
        }
        if (identity.kind == ZzLayoutState::ZzPanelKind::Side) {
            ZzFluentUI::ZzSidePane *const pane =
                zzOwningSide(shell, record.content);
            const qsizetype occurrences = (shell.leftSidePane != nullptr
                                        && shell.leftSidePane->panelStack()
                                                   != nullptr
                    ? shell.leftSidePane->panelStack()->panels().count(
                          record.contentIdentity)
                    : 0)
                + (shell.rightSidePane != nullptr
                                        && shell.rightSidePane->panelStack()
                                                   != nullptr
                    ? shell.rightSidePane->panelStack()->panels().count(
                          record.contentIdentity)
                    : 0);
            if (pane == nullptr || occurrences != 1) {
                removals.append({
                    record.id, record.content, record.contentIdentity,
                    record.registrationGeneration, identity.kind,
                    std::nullopt, 0});
                continue;
            }
            const bool secondary = record.activityArea
                    == ZzFluentUI::ZzActivityArea::LeftSecondary
                || record.activityArea
                    == ZzFluentUI::ZzActivityArea::RightSecondary;
            if (pane == shell.leftSidePane) {
                record.activityArea = secondary
                    ? ZzFluentUI::ZzActivityArea::LeftSecondary
                    : ZzFluentUI::ZzActivityArea::LeftPrimary;
            } else {
                record.activityArea = secondary
                    ? ZzFluentUI::ZzActivityArea::RightSecondary
                    : ZzFluentUI::ZzActivityArea::RightPrimary;
            }
        } else if (identity.kind == ZzLayoutState::ZzPanelKind::Bottom) {
            auto *const stack = shell.bottomPane != nullptr
                ? shell.bottomPane->findChild<QStackedWidget *>() : nullptr;
            if (stack == nullptr || !stack->isAncestorOf(record.content)) {
                removals.append({
                    record.id, record.content, record.contentIdentity,
                    record.registrationGeneration, identity.kind,
                    std::nullopt, 0});
            }
        }
    }
    const QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard =
        shell.leftSidePane;
    const QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard =
        shell.rightSidePane;
    const QPointer<ZzFluentUI::ZzPanelStack> leftStackGuard =
        leftPaneGuard != nullptr ? leftPaneGuard->panelStack() : nullptr;
    const QPointer<ZzFluentUI::ZzPanelStack> rightStackGuard =
        rightPaneGuard != nullptr ? rightPaneGuard->panelStack() : nullptr;
    const QPointer<QAbstractListModel> activityModelGuard =
        shell.activityModel;
    const auto originalActivityRows = shell.activityRows();
    QHash<QString, int> originalActivityOrders;
    originalActivityOrders.reserve(originalActivityRows.size());
    for (const auto &row : originalActivityRows) {
        originalActivityOrders.insert(row.id.value(), row.order);
    }
    for (ZzRemoval &removal : removals) {
        if (removal.kind == ZzLayoutState::ZzPanelKind::Side) {
            removal.activityRow = shell.activityRowSnapshot(removal.id);
        }
    }
    const auto activityModelStable = [&shell, &activityModelGuard] {
        return activityModelGuard != nullptr
            && shell.activityModel == activityModelGuard;
    };
    const auto identityRowFor = [&shell](const ZzRemoval &removal) {
        const int row = shell.indexOf(removal.id);
        if (row < 0
            || zzPanelKind(shell.panels.at(row).kind) != removal.kind
            || shell.panels.at(row).contentIdentity
                != removal.contentIdentity
            || shell.panels.at(row).registrationGeneration
                != removal.registrationGeneration) {
            return -1;
        }
        return row;
    };
    const auto currentRowFor = [&shell, &identityRowFor](
                                   const ZzRemoval &removal) {
        const int row = identityRowFor(removal);
        if (row < 0 || shell.panels.at(row).content == nullptr
            || shell.panels.at(row).content.data()
                != removal.contentIdentity
            || removal.content == nullptr
            || removal.content.data() != removal.contentIdentity) {
            return -1;
        }
        return row;
    };
    const auto inspectPhysicalSide =
        [&shell, &leftPaneGuard, &rightPaneGuard,
            &leftStackGuard, &rightStackGuard](const ZzRemoval &removal) {
        ZzPhysicalSideState state;
        if (leftPaneGuard == nullptr || rightPaneGuard == nullptr
            || leftStackGuard == nullptr || rightStackGuard == nullptr
            || shell.leftSidePane != leftPaneGuard
            || shell.rightSidePane != rightPaneGuard
            || leftPaneGuard->panelStack() != leftStackGuard
            || rightPaneGuard->panelStack() != rightStackGuard) {
            return state;
        }
        state.auditable = true;
        state.containingPanes.reserve(2);
        for (const QPointer<ZzFluentUI::ZzSidePane> &pane : {
                 leftPaneGuard, rightPaneGuard}) {
            ZzFluentUI::ZzPanelStack *const stack = pane->panelStack();
            const qsizetype paneOccurrences =
                stack->panels().count(removal.contentIdentity);
            state.occurrences += paneOccurrences;
            if (paneOccurrences <= 0) {
                continue;
            }
            state.containingPanes.append(pane);
            if (removal.content != nullptr
                && removal.content.data() == removal.contentIdentity
                && pane->isAncestorOf(removal.content)
                && stack->isAncestorOf(removal.content)) {
                state.validOwner = pane;
            }
        }
        return state;
    };
    const auto preservePhysicalSide =
        [&shell, &currentRowFor](const ZzRemoval &removal,
            const ZzPhysicalSideState &state) {
            if (!state.auditable || state.occurrences != 1
                || state.validOwner == nullptr) {
                return false;
            }
            const int row = currentRowFor(removal);
            if (row < 0) {
                return false;
            }
            auto &record = shell.panels[row];
            const bool secondary = record.activityArea
                    == ZzFluentUI::ZzActivityArea::LeftSecondary
                || record.activityArea
                    == ZzFluentUI::ZzActivityArea::RightSecondary;
            if (state.validOwner == shell.leftSidePane) {
                record.activityArea = secondary
                    ? ZzFluentUI::ZzActivityArea::LeftSecondary
                    : ZzFluentUI::ZzActivityArea::LeftPrimary;
                return true;
            }
            if (state.validOwner == shell.rightSidePane) {
                record.activityArea = secondary
                    ? ZzFluentUI::ZzActivityArea::RightSecondary
                    : ZzFluentUI::ZzActivityArea::RightPrimary;
                return true;
            }
            return false;
        };
    for (const ZzRemoval &removal : std::as_const(removals)) {
        if (removal.kind != ZzLayoutState::ZzPanelKind::Side
            && currentRowFor(removal) >= 0) {
            shell.cleanupInterruptedPanelRemoval(
                removal.id, removal.contentIdentity,
                removal.registrationGeneration);
        }
    }
    const auto insertionOrderFor =
        [&shell, &originalActivityOrders](const ZzRemoval &removal) {
            const int originalOrder = originalActivityOrders.value(
                removal.id.value(),
                removal.activityRow.has_value()
                    ? removal.activityRow->order : 0);
            int insertionOrder = 0;
            for (const auto &row : shell.activityRows()) {
                const auto currentOrder =
                    originalActivityOrders.constFind(row.id.value());
                if (currentOrder != originalActivityOrders.cend()
                    && currentOrder.value() < originalOrder) {
                    ++insertionOrder;
                }
            }
            return insertionOrder;
        };
    const auto settledSideRemovalIds =
        [&shell, &activityModelStable, &identityRowFor,
            &inspectPhysicalSide](const QVector<ZzRemoval> &pending)
            -> std::optional<QSet<QString>> {
            if (!activityModelStable()) {
                return std::nullopt;
            }
            QSet<QString> result;
            result.reserve(pending.size());
            for (const ZzRemoval &removal : pending) {
                if (removal.kind != ZzLayoutState::ZzPanelKind::Side) {
                    continue;
                }
                const int panelRow = identityRowFor(removal);
                const ZzPhysicalSideState physical =
                    inspectPhysicalSide(removal);
                if (panelRow < 0) {
                    continue;
                }
                if (!physical.auditable) {
                    return std::nullopt;
                }
                if (physical.occurrences == 0
                    && !shell.activityRowSnapshot(removal.id).has_value()) {
                    result.insert(removal.id.value());
                }
            }
            return result;
        };
    const auto eraseSettledSideRegistrations =
        [&shell, &activityModelStable, &identityRowFor,
            &inspectPhysicalSide](const QVector<ZzRemoval> &pending,
                const QSet<QString> &logicallyRemoved) {
            if (!activityModelStable()) {
                return;
            }
            for (const ZzRemoval &removal : pending) {
                if (removal.kind != ZzLayoutState::ZzPanelKind::Side
                    || !logicallyRemoved.contains(removal.id.value())) {
                    continue;
                }
                const int panelRow = identityRowFor(removal);
                const ZzPhysicalSideState physical =
                    inspectPhysicalSide(removal);
                if (panelRow < 0 || !physical.auditable
                    || physical.occurrences != 0
                    || shell.activityRowSnapshot(removal.id).has_value()) {
                    continue;
                }
                QObject::disconnect(
                    shell.panels[panelRow].contentDestroyedConnection);
                shell.panels.removeAt(panelRow);
            }
        };
    enum class ZzSideUiStep : unsigned char
    {
        Unavailable,
        Mutated,
        Stable
    };
    const auto sideUiAvailable = [&shell, &runtime, &activityModelStable,
                                     &leftPaneGuard, &rightPaneGuard,
                                     &leftStackGuard, &rightStackGuard] {
        return activityModelStable()
            && zzStableGuards(shell, runtime.guards)
            && leftPaneGuard != nullptr && rightPaneGuard != nullptr
            && leftStackGuard != nullptr && rightStackGuard != nullptr
            && leftPaneGuard->panelStack() == leftStackGuard
            && rightPaneGuard->panelStack() == rightStackGuard;
    };
    const auto synchronizeOneSideUiStep =
        [&shell, &runtime, &sideUiAvailable, &leftPaneGuard,
            &rightPaneGuard](const QSet<QString> &logicallyRemoved) {
            if (!sideUiAvailable()) {
                return ZzSideUiStep::Unavailable;
            }
            const auto hasPanelForEdge =
                [&shell, &logicallyRemoved](bool left) {
                    return std::any_of(
                        shell.panels.cbegin(), shell.panels.cend(),
                        [&logicallyRemoved, left](const auto &record) {
                            return record.kind
                                    == ZzWorkspaceShellPrivate::ZzPanelKind::Side
                                && record.content != nullptr
                                && !logicallyRemoved.contains(record.id.value())
                                && (record.activityArea
                                            == ZzFluentUI::ZzActivityArea::LeftPrimary
                                        || record.activityArea
                                            == ZzFluentUI::ZzActivityArea::LeftSecondary)
                                    == left;
                        });
                };
            const auto synchronizeEdge =
                [&hasPanelForEdge](ZzFluentUI::ZzSidePane *pane,
                    ZzFluentUI::ZzActivityBar *bar, bool left) {
                    if (pane == nullptr || bar == nullptr) {
                        return ZzSideUiStep::Unavailable;
                    }
                    const bool hasPanel = hasPanelForEdge(left);
                    if (bar->isHidden() == hasPanel) {
                        bar->setVisible(hasPanel);
                        return ZzSideUiStep::Mutated;
                    }
                    if (!hasPanel
                        && bar->currentSourceIndex().isValid()) {
                        bar->setCurrentSourceIndex({});
                        return ZzSideUiStep::Mutated;
                    }
                    if (!hasPanel && !pane->isCollapsed()) {
                        pane->setCollapsed(true);
                        return ZzSideUiStep::Mutated;
                    }
                    return ZzSideUiStep::Stable;
                };
            ZzSideUiStep step = synchronizeEdge(
                leftPaneGuard, shell.leftActivityBar, true);
            if (step != ZzSideUiStep::Stable) {
                return step;
            }
            step = synchronizeEdge(
                rightPaneGuard, shell.rightActivityBar, false);
            if (step != ZzSideUiStep::Stable || !sideUiAvailable()) {
                return step == ZzSideUiStep::Stable
                    ? ZzSideUiStep::Unavailable : step;
            }

            const ZzRuntimeIndexes indexes =
                zzBuildRuntimeIndexes(shell, runtime);
            const auto desiredCurrent =
                [&shell, &indexes](ZzFluentUI::ZzSidePane *pane) {
                    return zzIndexForId(shell, indexes, zzIds(
                        {pane->currentWidget()}, indexes).value(0));
                };
            const auto desiredActive =
                [&shell, &indexes](ZzFluentUI::ZzSidePane *pane) {
                    return zzIndexesForIds(shell, indexes, zzIds(
                        pane->visibleWidgets(), indexes));
                };
            const QModelIndex leftCurrent =
                desiredCurrent(leftPaneGuard);
            if (shell.leftActivityBar->currentSourceIndex() != leftCurrent) {
                shell.leftActivityBar->setCurrentSourceIndex(leftCurrent);
                return ZzSideUiStep::Mutated;
            }
            const QList<QModelIndex> leftActive =
                desiredActive(leftPaneGuard);
            if (shell.leftActivityBar->activeSourceIndexes() != leftActive) {
                shell.leftActivityBar->setActiveSourceIndexes(leftActive);
                return ZzSideUiStep::Mutated;
            }
            const QModelIndex rightCurrent =
                desiredCurrent(rightPaneGuard);
            if (shell.rightActivityBar->currentSourceIndex() != rightCurrent) {
                shell.rightActivityBar->setCurrentSourceIndex(rightCurrent);
                return ZzSideUiStep::Mutated;
            }
            const QList<QModelIndex> rightActive =
                desiredActive(rightPaneGuard);
            if (shell.rightActivityBar->activeSourceIndexes() != rightActive) {
                shell.rightActivityBar->setActiveSourceIndexes(rightActive);
                return ZzSideUiStep::Mutated;
            }
            return ZzSideUiStep::Stable;
        };
    constexpr int maximumTakeAttempts = 4;
    constexpr int maximumReconciliationPasses = 24;
    for (int pass = 0; pass < maximumReconciliationPasses; ++pass) {
        bool crossedCallbackBoundary = false;
        for (ZzRemoval &removal : removals) {
            if (removal.kind != ZzLayoutState::ZzPanelKind::Side
                || identityRowFor(removal) < 0) {
                continue;
            }
            const ZzPhysicalSideState physical =
                inspectPhysicalSide(removal);
            if (!physical.auditable) {
                continue;
            }
            if (preservePhysicalSide(removal, physical)) {
                const int panelRow = currentRowFor(removal);
                if (panelRow < 0 || !activityModelStable()) {
                    continue;
                }
                const auto currentActivity =
                    shell.activityRowSnapshot(removal.id);
                if (currentActivity.has_value()) {
                    if (!removal.activityRow.has_value()) {
                        removal.activityRow = currentActivity;
                    }
                    if (currentActivity->area
                        != shell.panels.at(panelRow).activityArea) {
                        static_cast<void>(shell.setActivityRowArea(
                            removal.id,
                            shell.panels.at(panelRow).activityArea));
                        crossedCallbackBoundary = true;
                    }
                } else if (removal.activityRow.has_value()) {
                    auto restoredRow = *removal.activityRow;
                    restoredRow.area =
                        shell.panels.at(panelRow).activityArea;
                    restoredRow.order = insertionOrderFor(removal);
                    const bool wasRemovalInProgress =
                        shell.panels.at(panelRow).removalInProgress;
                    shell.panels[panelRow].removalInProgress = true;
                    static_cast<void>(
                        shell.restoreActivityRow(restoredRow));
                    const int restoredPanelRow = identityRowFor(removal);
                    if (restoredPanelRow >= 0) {
                        shell.panels[restoredPanelRow].removalInProgress =
                            wasRemovalInProgress;
                    }
                    crossedCallbackBoundary = true;
                }
                continue;
            }
            if (physical.occurrences != 0) {
                if (removal.content == nullptr
                    || removal.takeAttempts >= maximumTakeAttempts
                    || physical.containingPanes.isEmpty()) {
                    continue;
                }
                QPointer<ZzFluentUI::ZzSidePane> paneToTake;
                for (const QPointer<ZzFluentUI::ZzSidePane> &pane :
                     physical.containingPanes) {
                    if (physical.validOwner == nullptr
                        || pane != physical.validOwner) {
                        paneToTake = pane;
                        break;
                    }
                }
                if (paneToTake == nullptr) {
                    paneToTake = physical.containingPanes.constFirst();
                }
                ++removal.takeAttempts;
                static_cast<void>(
                    paneToTake->takeWidget(removal.contentIdentity));
                crossedCallbackBoundary = true;
                continue;
            }
            if (!activityModelStable()) {
                continue;
            }
            const auto currentActivity =
                shell.activityRowSnapshot(removal.id);
            if (currentActivity.has_value()) {
                if (!removal.activityRow.has_value()) {
                    removal.activityRow = currentActivity;
                }
                const int panelRow = identityRowFor(removal);
                if (panelRow < 0) {
                    continue;
                }
                const bool wasRemovalInProgress =
                    shell.panels.at(panelRow).removalInProgress;
                shell.panels[panelRow].removalInProgress = true;
                static_cast<void>(shell.removeActivityRow(removal.id));
                const int removedPanelRow = identityRowFor(removal);
                if (removedPanelRow >= 0) {
                    shell.panels[removedPanelRow].removalInProgress =
                        wasRemovalInProgress;
                }
                crossedCallbackBoundary = true;
            }
        }
        if (activityModelStable()) {
            QHash<QString, int> livePanelRows;
            livePanelRows.reserve(shell.panels.size());
            for (qsizetype row = 0; row < shell.panels.size(); ++row) {
                livePanelRows.insert(
                    shell.panels.at(row).id.value(),
                    static_cast<int>(row));
            }
            const auto currentActivityRows = shell.activityRows();
            for (const auto &activityRow : currentActivityRows) {
                const int panelRow = livePanelRows.value(
                    activityRow.id.value(), -1);
                if (panelRow < 0 || panelRow >= shell.panels.size()
                    || shell.panels.at(panelRow).id != activityRow.id
                    || shell.panels.at(panelRow).kind
                        != ZzWorkspaceShellPrivate::ZzPanelKind::Side
                    || shell.panels.at(panelRow).content == nullptr
                    || shell.panels.at(panelRow).content.data()
                        != shell.panels.at(panelRow).contentIdentity
                    || activityRow.area
                        == shell.panels.at(panelRow).activityArea) {
                    continue;
                }
                static_cast<void>(shell.setActivityRowArea(
                    activityRow.id,
                    shell.panels.at(panelRow).activityArea));
                crossedCallbackBoundary = true;
            }
        }
        if (crossedCallbackBoundary) {
            continue;
        }
        const auto logicallyRemoved =
            settledSideRemovalIds(removals);
        if (!logicallyRemoved.has_value()) {
            break;
        }
        const ZzSideUiStep uiStep =
            synchronizeOneSideUiStep(*logicallyRemoved);
        if (uiStep == ZzSideUiStep::Mutated) {
            continue;
        }
        if (uiStep == ZzSideUiStep::Unavailable) {
            break;
        }
        // No callback-producing work remains. Re-audit immediately before
        // the signal-free registry erase, then perform no further UI writes.
        eraseSettledSideRegistrations(removals, *logicallyRemoved);
        return;
    }
    // Exhaustion and destroyed subsystem guards are fail-closed: without a
    // signal-free final audit, both registry and any surviving UI state stay.
}

[[nodiscard]] bool zzRollback(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const QString &migrationGroup,
    int migrationCurrent,
    const QByteArray &alternateSplitCanonical)
{
    ZzSideOwnerIndex owners = runtime.sideOwners;
    ZzSideOwnerObserver ownerObserver(runtime.projection, &owners);
    bool complete = zzApplyActivityAndTitle(
        shell, runtime, runtime.projection);
    complete = zzApplyBottom(
        shell, runtime, runtime.projection.bottom, true)
        && complete;
    complete = zzApplySide(
        shell, runtime, runtime.projection, false, ownerObserver, owners)
        && complete;
    complete = zzApplySplit(
        shell, runtime, runtime.projection.split,
        migrationGroup, migrationCurrent,
        alternateSplitCanonical) && complete;
    complete = zzApplyDock(shell, runtime, runtime.projection.dock)
        && complete;
    if (!complete || !ownerObserver.isValid()
        || !zzAuditAll(shell, runtime, runtime.projection, owners)) {
        zzSynchronizeAfterFailedRollback(shell, runtime);
        return false;
    }
    return true;
}

} // namespace

ZzWorkspaceLayoutTransactionPrivate::
ZzWorkspaceLayoutTransactionPrivate(
    ZzWorkspaceShellPrivate &shell) noexcept
    : shell_(shell)
{
}

ZzCore::ZzResult<QByteArray>
ZzWorkspaceLayoutTransactionPrivate::save() const
{
    if (shell_.transactionKind
            != ZzWorkspaceShellPrivate::ZzTransactionKind::None) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"));
    }
    QString captureFailure;
    const auto captured = zzCaptureSnapshot(shell_, &captureFailure);
    if (!captured.has_value()) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace layout projection is invalid"),
            captureFailure);
    }
    if (!zzSideOrderMatchesActivity(*captured)) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral(
                "Workspace side stack order differs from activity order"));
    }
    ZzLayoutState::ZzLayoutRequest request;
    request.projection = static_cast<const ZzProjection &>(
        captured->projection);
    // codec 的 side.order 仅是 sideEntries 的派生 DTO，不会写入新字段。
    request.projection->leftSide.order =
        request.projection->activity.leftPrimary
        + request.projection->activity.leftSecondary;
    request.projection->rightSide.order =
        request.projection->activity.rightPrimary
        + request.projection->activity.rightSecondary;
    request.leftCurrent = captured->projection.leftSide.current;
    request.rightCurrent = captured->projection.rightSide.current;
    request.sourceSchema =
        ZzLayoutState::ZzLayoutRequest::ZzSourceSchema::VersionThree;
    return ZzWorkspaceLayoutCodecPrivate::encodeVersionThree(request);
}

ZzCore::ZzResult<void>
ZzWorkspaceLayoutTransactionPrivate::restore(
    const QByteArray &encoded)
{
    if (shell_.transactionKind
            != ZzWorkspaceShellPrivate::ZzTransactionKind::None) {
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"));
    }
    auto decoded = ZzWorkspaceLayoutCodecPrivate::decode(encoded);
    if (!decoded) {
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace layout envelope is invalid"));
    }
    QString initialCaptureFailure;
    const auto captured = zzCaptureSnapshot(shell_, &initialCaptureFailure);
    if (!captured.has_value()) {
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"),
            initialCaptureFailure.isEmpty()
                ? QStringLiteral("initial-capture")
                : QStringLiteral("initial-capture/%1")
                    .arg(initialCaptureFailure));
    }
    const ZzRuntimeSnapshot &initialSnapshot = *captured;
    ZzLayoutState::ZzLayoutRequest request = std::move(decoded).value();
    if (!request.projection.has_value()) {
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    ZzProjection &requestProjection = request.projection.value();
    const bool versionOne = request.sourceSchema
        == ZzLayoutState::ZzLayoutRequest::ZzSourceSchema::VersionOne;
    if (versionOne) {
        const ZzLayoutState::ZzTitleProjection requestedTitle =
            requestProjection.title;
        requestProjection.title = initialSnapshot.projection.title;
        requestProjection.title.mode = requestedTitle.mode;
    }
    const int migrationCurrent = versionOne
        ? requestProjection.split.root.currentIndex : -1;
    const QString migrationGroup = versionOne
        ? initialSnapshot.projection.split.groupOrder.value(0) : QString{};
    auto *const snapshotMigrationTabs = !migrationGroup.isEmpty()
        ? shell_.splitWorkspace->tabWidget(
            ZzFluentUI::ZzTabGroupId(migrationGroup))
        : nullptr;
    const int snapshotMigrationCurrent = snapshotMigrationTabs != nullptr
        ? snapshotMigrationTabs->currentIndex() : -1;
    if (versionOne) {
        if (migrationGroup.isEmpty() || snapshotMigrationTabs == nullptr) {
            return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace split projection is invalid"));
        }
        requestProjection.split = initialSnapshot.projection.split;
    }
    requestProjection.bottom.order = initialSnapshot.projection.bottom.order;
    const bool dockTargetReady = zzBuildDockTarget(
        initialSnapshot, &requestProjection.dock);
    const bool splitTargetReady = dockTargetReady
        && zzCanonicalizeSplitTarget(
            shell_.splitWorkspace, &requestProjection.split,
            migrationGroup, migrationCurrent);
    const ZzSnapshot initialPlanningSnapshot =
        zzLogicalPlanningSnapshot(initialSnapshot);
    const auto validatedTarget = splitTargetReady
        ? ZzLayoutState::buildRestoreTarget(
            initialPlanningSnapshot, request)
        : std::optional<ZzProjection>{};
    if (!validatedTarget.has_value()) {
        const ZzLayoutTransactionScope transaction(shell_);
        const bool rolledBack = zzRollback(
            shell_, initialSnapshot, migrationGroup,
            snapshotMigrationCurrent, requestProjection.split.canonicalState);
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            rolledBack
                ? QStringLiteral(
                    "Workspace layout restore failed and was rolled back")
                : QStringLiteral(
                    "Workspace layout restore failed and rollback failed"),
            QStringLiteral("validation"));
    }

    QStringList materializationIds = validatedTarget->leftSide.visible
        + validatedTarget->rightSide.visible;
    materializationIds.append(validatedTarget->leftSide.current);
    materializationIds.append(validatedTarget->rightSide.current);
    materializationIds.removeAll(QString{});
    QSet<QString> seenMaterializationIds;
    QVector<ZzRestoreMaterialization> materializations;
    const ZzLayoutTransactionScope transaction(shell_);
    const auto rollbackMaterializations = [&] {
        return materializations.isEmpty()
            || zzRollbackMaterializations(shell_, &materializations);
    };
    const auto rollbackPreparation = [&] {
        const bool materializationsRolledBack = rollbackMaterializations();
        const bool initialRolledBack = zzRollback(
            shell_, initialSnapshot, migrationGroup,
            snapshotMigrationCurrent,
            requestProjection.split.canonicalState);
        return materializationsRolledBack && initialRolledBack;
    };
    for (const QString &id : std::as_const(materializationIds)) {
        if (seenMaterializationIds.contains(id)) {
            continue;
        }
        seenMaterializationIds.insert(id);
        const int panelIndex = shell_.indexOf(ZzWorkspacePanelId(id));
        if (panelIndex < 0
            || shell_.panels.at(panelIndex).kind
                != ZzWorkspaceShellPrivate::ZzPanelKind::Side
            || shell_.panels.at(panelIndex).materialization
                != ZzWorkspaceShellPrivate::ZzMaterializationState::Pending) {
            continue;
        }
        auto materialized = zzMaterializeForRestore(
            shell_, ZzWorkspacePanelId(id), &materializations);
        if (!materialized) {
            const ZzCore::ZzError &error = materialized.error();
            if (rollbackPreparation()) {
                return ZzCore::ZzResult<void>::failure(error);
            }
            return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral(
                    "Workspace layout restore failed and rollback failed"),
                QStringLiteral("materialization"));
        }
    }

    QString preparedCaptureFailure;
    const auto preparedCaptured = zzCaptureSnapshot(
        shell_, &preparedCaptureFailure);
    if (!preparedCaptured.has_value()) {
        const bool rolledBack = rollbackPreparation();
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            rolledBack
                ? QStringLiteral(
                    "Workspace layout restore failed and was rolled back")
                : QStringLiteral(
                    "Workspace layout restore failed and rollback failed"),
            preparedCaptureFailure.isEmpty()
                ? QStringLiteral("prepared-capture")
                : QStringLiteral("prepared-capture/%1")
                    .arg(preparedCaptureFailure));
    }
    const ZzRuntimeSnapshot &snapshot = *preparedCaptured;
    const ZzSnapshot planningSnapshot = zzLogicalPlanningSnapshot(snapshot);
    auto planned = ZzLayoutState::buildRestoreTarget(
        planningSnapshot, request);
    if (planned.has_value()) {
        zzKeepReadyPhysicalProjection(shell_, &*planned);
    }
    const char *failedStage = "planning";
    bool committed = planned.has_value();
    if (committed) {
        const ZzProjection &target = *planned;
        ZzSideOwnerIndex targetOwners = snapshot.sideOwners;
        ZzSideOwnerObserver ownerObserver(target, &targetOwners);
        failedStage = "dock";
        committed = zzApplyDock(shell_, snapshot, target.dock);
        if (committed) {
            failedStage = "split";
            committed = zzApplySplit(
                shell_, snapshot, target.split,
                migrationGroup, migrationCurrent,
                snapshot.projection.split.canonicalState);
        }
        if (committed) {
            failedStage = "side";
            committed = zzApplySide(
                shell_, snapshot, target, true,
                ownerObserver, targetOwners);
        }
        if (committed) {
            failedStage = "bottom";
            committed = zzApplyBottom(shell_, snapshot, target.bottom);
        }
        if (committed) {
            failedStage = "activity-title";
            committed = zzApplyActivityAndTitle(shell_, snapshot, target);
        }
        if (committed) {
            failedStage = "final-audit";
            committed = ownerObserver.isValid()
                && zzAuditAll(
                    shell_, snapshot, target, targetOwners,
                    migrationGroup, migrationCurrent);
        }
    }
    if (committed) {
        return ZzCore::ZzResult<void>::success();
    }
    const QString failureContext = QString::fromLatin1(failedStage);
    const QByteArray alternateSplitCanonical = planned.has_value()
        ? planned->split.canonicalState : QByteArray{};
    const bool preparedRolledBack = zzRollback(
        shell_, snapshot, migrationGroup, snapshotMigrationCurrent,
        alternateSplitCanonical);
    const bool materializationsRolledBack = rollbackMaterializations();
    const bool rolledBack =
        preparedRolledBack && materializationsRolledBack;
    return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
        rolledBack
            ? QStringLiteral(
                "Workspace layout restore failed and was rolled back")
            : QStringLiteral(
                "Workspace layout restore failed and rollback failed"),
        failureContext);
}

} // namespace ZzPureTools
