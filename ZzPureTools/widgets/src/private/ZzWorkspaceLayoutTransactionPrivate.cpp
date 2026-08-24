#include "ZzWorkspaceLayoutTransactionPrivate.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QHash>
#include <QtCore/QIODevice>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QSet>
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
constexpr quint16 zzWorkspaceSchemaVersion = 2;
constexpr auto zzStreamVersion = QDataStream::Qt_6_8;
constexpr qsizetype zzMaximumLayoutSize = qsizetype{1024} * 1024;

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<ZzValue>::failure(
        ZzCore::ZzError(code, std::move(message)));
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
};

struct ZzRuntimeSnapshot final
{
    ZzSnapshot projection;
    ZzRuntimeGuards guards;
    QHash<QString, int> panelRows;
};

struct ZzRuntimeIndexes final
{
    QHash<QWidget *, QString> widgetIds;
    QHash<QString, int> activityRows;
    QHash<int, QString> activityIds;
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
    const QList<QWidget *> visible = pane != nullptr
        ? pane->visibleWidgets() : QList<QWidget *>{};
    const QList<int> sizes = pane != nullptr && pane->panelStack() != nullptr
        ? pane->panelStack()->panelSizes() : QList<int>{};
    stream << static_cast<quint8>(
                  pane != nullptr && pane->isCollapsed() ? 1 : 0)
           << static_cast<qint32>(
                  pane != nullptr ? pane->paneWidth() : 280)
           << current << static_cast<quint32>(visible.size());
    for (QWidget *const content : visible) {
        stream << zzIdForWidget(ids, content);
    }
    stream << static_cast<quint32>(sizes.size());
    for (const int size : sizes) {
        stream << static_cast<qint32>(size);
    }
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

void zzCaptureSideRuntime(
    const QHash<QWidget *, QString> &ids,
    ZzFluentUI::ZzSidePane *pane,
    ZzSide *side)
{
    if (pane == nullptr || pane->panelStack() == nullptr) {
        return;
    }
    side->paneIdentity = zzIdentity(pane);
    side->stackIdentity = zzIdentity(pane->panelStack());
    side->order.clear();
    side->contents.clear();
    for (QWidget *const content : pane->panelStack()->panels()) {
        const QString id = ids.value(content);
        if (!id.isEmpty()) {
            side->order.append(id);
            side->contents.append({
                id, side->stackIdentity,
                {side->paneIdentity, side->stackIdentity}});
        }
    }
}

[[nodiscard]] std::optional<ZzRuntimeSnapshot> zzCaptureSnapshot(
    const ZzWorkspaceShellPrivate &shell)
{
    const QByteArray observed = zzObservedEnvelope(shell);
    if (observed.isEmpty()) {
        return std::nullopt;
    }
    auto decoded = ZzWorkspaceLayoutCodecPrivate::decode(observed);
    if (!decoded || !decoded.value().projection.has_value()) {
        return std::nullopt;
    }

    ZzRuntimeSnapshot result;
    static_cast<ZzProjection &>(result.projection) =
        std::move(*decoded.value().projection);
    result.guards = {
        shell.host, shell.splitWorkspace, shell.leftSidePane,
        shell.rightSidePane, shell.bottomPane, shell.leftActivityBar,
        shell.rightActivityBar, shell.activityModel};

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
            return std::nullopt;
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
            auto *const dock = record.dock.data();
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
    zzCaptureSideRuntime(
        ids, shell.leftSidePane, &result.projection.leftSide);
    zzCaptureSideRuntime(
        ids, shell.rightSidePane, &result.projection.rightSide);

    result.projection.bottom.paneIdentity = zzIdentity(shell.bottomPane);
    auto *const bottomStack = shell.bottomPane != nullptr
        ? shell.bottomPane->findChild<QStackedWidget *>() : nullptr;
    result.projection.bottom.stackIdentity = zzIdentity(bottomStack);
    result.projection.bottom.order.clear();
    result.projection.bottom.contents.clear();
    if (bottomStack == nullptr) {
        return std::nullopt;
    }
    for (int index = 0; index < bottomStack->count(); ++index) {
        QWidget *const content = bottomStack->widget(index);
        const QString id = ids.value(content);
        if (id.isEmpty()) {
            return std::nullopt;
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
        return std::nullopt;
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
    return snapshot.leftSide.order
            == snapshot.activity.leftPrimary
                + snapshot.activity.leftSecondary
        && snapshot.rightSide.order
            == snapshot.activity.rightPrimary
                + snapshot.activity.rightSecondary;
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
        && guards.activityModel == shell.activityModel;
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
            || record->registrationGeneration != identity.registrationGeneration
            || record->contentIdentity != identity.rawWidget
            || record->content == nullptr
            || record->content.data() != identity.rawWidget
            || identity.widget == nullptr
            || identity.widget.data() != identity.rawWidget
            || record->dockIdentity != identity.rawDock
            || record->dock != identity.dock) {
            return false;
        }
    }
    return true;
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

[[nodiscard]] bool zzAuditSide(
    const ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const ZzSide &expected,
    ZzFluentUI::ZzSidePane *pane,
    const ZzRuntimeIndexes &indexes)
{
    if (pane == nullptr || expected.paneIdentity.object == nullptr
        || expected.paneIdentity.object.data() != expected.paneIdentity.rawObject
        || pane != expected.paneIdentity.rawObject
        || pane->panelStack() == nullptr
        || expected.stackIdentity.object == nullptr
        || pane->panelStack() != expected.stackIdentity.rawObject) {
        return false;
    }
    const QStringList observedOrder = zzIds(
        pane->panelStack()->panels(), indexes);
    const QStringList observedVisible = zzIds(
        pane->visibleWidgets(), indexes);
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
        auto *const dock = record != nullptr ? record->dock.data() : nullptr;
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
            shell, runtime, expected.leftSide, shell.leftSidePane, indexes)
        && zzAuditSide(
            shell, runtime, expected.rightSide, shell.rightSidePane, indexes)
        && zzAuditBottom(shell, runtime, expected.bottom, indexes)
        && zzAuditActivity(shell, expected.activity, indexes)
        && shell.titleMode == zzTitleMode(expected.title.mode);
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
    bool strict)
{
    bool complete = true;
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
                    QWidget *const taken = owner->takeWidget(content);
                    if (taken != content || content == nullptr
                        || content->parent() != nullptr
                        || !zzStableGuards(shell, runtime.guards)) {
                        complete = false;
                        if (strict) return false;
                        continue;
                    }
                }
                if (destinationGuard == nullptr || content == nullptr
                    || !destinationGuard->addWidget(content, record->title)
                    || destinationGuard == nullptr || content == nullptr
                    || !destinationGuard->isAncestorOf(content)
                    || !destinationGuard->panelStack()->isAncestorOf(content)
                    || !zzStableGuards(shell, runtime.guards)) {
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
                || !zzStableGuards(shell, runtime.guards)) {
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
                || !zzStableGuards(shell, runtime.guards)) {
                complete = false;
                if (strict) return false;
            }
        }
        if (!side.current.isEmpty()) {
            const auto *const current = zzRecord(shell, runtime, side.current);
            if (current == nullptr || current->content == nullptr
                || !destinationGuard->setCurrentWidget(current->content)
                || destinationGuard == nullptr
                || !zzStableGuards(shell, runtime.guards)) {
                complete = false;
                if (strict) return false;
            }
        }
        const bool sizesApplied = side.sizes.isEmpty()
            ? true
            : destinationGuard->panelStack()->setPanelSizes(side.sizes);
        if (!sizesApplied || destinationGuard == nullptr
            || !zzStableGuards(shell, runtime.guards)) {
            complete = false;
            if (strict) return false;
        }
        destinationGuard->setPaneWidth(side.width);
        if (destinationGuard == nullptr
            || !zzStableGuards(shell, runtime.guards)) {
            complete = false;
            if (strict) return false;
        }
        destinationGuard->setCollapsed(side.collapsed);
        if (destinationGuard == nullptr
            || !zzStableGuards(shell, runtime.guards)) {
            complete = false;
            if (strict) return false;
        }
        return true;
    };
    static_cast<void>(place(target.leftSide, shell.leftSidePane));
    static_cast<void>(place(target.rightSide, shell.rightSidePane));
    const ZzRuntimeIndexes indexes = zzBuildRuntimeIndexes(shell, runtime);
    const bool audited = zzStablePanels(shell, runtime)
        && zzAuditSide(
            shell, runtime, target.leftSide, shell.leftSidePane, indexes)
        && zzAuditSide(
            shell, runtime, target.rightSide, shell.rightSidePane, indexes);
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
            const QPointer<QWidget> content = contents.at(index);
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
        && shell.titleMode == zzTitleMode(target.title.mode);
}

/** @brief 回滚后按实际合法 owner 修复 Activity；第三方 owner 只清注册。 */
void zzSynchronizeAfterFailedRollback(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime)
{
    struct ZzRemoval final
    {
        int capturedRow = -1;
        ZzWorkspacePanelId id;
        QWidget *contentIdentity = nullptr;
        quint64 registrationGeneration = 0;
    };
    QVector<ZzRemoval> removals;
    for (const auto &identity : runtime.projection.identities) {
        const auto *const record = zzRecord(shell, runtime, identity.id);
        if (record == nullptr || record->content == nullptr) {
            continue;
        }
        if (identity.kind == ZzLayoutState::ZzPanelKind::Side) {
            ZzFluentUI::ZzSidePane *const pane =
                zzOwningSide(shell, record->content);
            if (pane == nullptr) {
                removals.append({
                    runtime.panelRows.value(identity.id, -1),
                    record->id, record->contentIdentity,
                    record->registrationGeneration});
                continue;
            }
            auto *const mutableRecord = zzRecord(shell, runtime, identity.id);
            mutableRecord->activityArea = pane == shell.leftSidePane
                ? ZzFluentUI::ZzActivityArea::LeftPrimary
                : ZzFluentUI::ZzActivityArea::RightPrimary;
        } else if (identity.kind == ZzLayoutState::ZzPanelKind::Bottom) {
            auto *const stack = shell.bottomPane != nullptr
                ? shell.bottomPane->findChild<QStackedWidget *>() : nullptr;
            if (stack == nullptr || !stack->isAncestorOf(record->content)) {
                removals.append({
                    runtime.panelRows.value(identity.id, -1),
                    record->id, record->contentIdentity,
                    record->registrationGeneration});
            }
        }
    }
    std::sort(removals.begin(), removals.end(),
        [](const ZzRemoval &left, const ZzRemoval &right) {
            return left.capturedRow > right.capturedRow;
        });
    for (const ZzRemoval &removal : std::as_const(removals)) {
        const int row = removal.capturedRow;
        if (row >= 0 && row < shell.panels.size()
            && shell.panels.at(row).id == removal.id
            && shell.panels.at(row).contentIdentity
                == removal.contentIdentity
            && shell.panels.at(row).registrationGeneration
                == removal.registrationGeneration) {
            shell.handlePanelContentDestroyed(
                removal.id, removal.contentIdentity);
        }
    }
    QHash<QString, int> currentPanelRows;
    currentPanelRows.reserve(shell.panels.size());
    for (qsizetype index = 0; index < shell.panels.size(); ++index) {
        currentPanelRows.insert(
            shell.panels.at(index).id.value(), static_cast<int>(index));
    }
    QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> rows =
        shell.activityRows();
    for (auto &row : rows) {
        const int panelRow = currentPanelRows.value(row.id.value(), -1);
        if (panelRow >= 0 && panelRow < shell.panels.size()
            && shell.panels.at(panelRow).id == row.id) {
            row.area = shell.panels.at(panelRow).activityArea;
        }
    }
    static_cast<void>(shell.replaceActivityRows(rows));
    const ZzRuntimeIndexes indexes = zzBuildRuntimeIndexes(shell, runtime);
    if (shell.leftActivityBar != nullptr && shell.leftSidePane != nullptr) {
        shell.leftActivityBar->setCurrentSourceIndex(
            zzIndexForId(shell, indexes, zzIds(
                {shell.leftSidePane->currentWidget()}, indexes).value(0)));
        shell.leftActivityBar->setActiveSourceIndexes(
            zzIndexesForIds(shell, indexes, zzIds(
                shell.leftSidePane->visibleWidgets(), indexes)));
    }
    if (shell.rightActivityBar != nullptr && shell.rightSidePane != nullptr) {
        shell.rightActivityBar->setCurrentSourceIndex(
            zzIndexForId(shell, indexes, zzIds(
                {shell.rightSidePane->currentWidget()}, indexes).value(0)));
        shell.rightActivityBar->setActiveSourceIndexes(
            zzIndexesForIds(shell, indexes, zzIds(
                shell.rightSidePane->visibleWidgets(), indexes)));
    }
}

[[nodiscard]] bool zzRollback(
    ZzWorkspaceShellPrivate &shell,
    const ZzRuntimeSnapshot &runtime,
    const QString &migrationGroup,
    int migrationCurrent,
    const QByteArray &alternateSplitCanonical)
{
    bool complete = zzApplyActivityAndTitle(
        shell, runtime, runtime.projection);
    complete = zzApplyBottom(
        shell, runtime, runtime.projection.bottom, true)
        && complete;
    complete = zzApplySide(shell, runtime, runtime.projection, false)
        && complete;
    complete = zzApplySplit(
        shell, runtime, runtime.projection.split,
        migrationGroup, migrationCurrent,
        alternateSplitCanonical) && complete;
    complete = zzApplyDock(shell, runtime, runtime.projection.dock)
        && complete;
    if (!complete || !zzAuditAll(shell, runtime, runtime.projection)) {
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
    const auto captured = zzCaptureSnapshot(shell_);
    if (!captured.has_value()) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace layout projection is invalid"));
    }
    if (!zzSideOrderMatchesActivity(*captured)) {
        return zzFailure<QByteArray>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral(
                "Workspace side stack order differs from activity order"));
    }
    ZzLayoutState::ZzLayoutRequest request;
    request.projection = static_cast<const ZzProjection &>(
        captured->projection);
    request.leftCurrent = captured->projection.leftSide.current;
    request.rightCurrent = captured->projection.rightSide.current;
    request.sourceSchema =
        ZzLayoutState::ZzLayoutRequest::ZzSourceSchema::VersionTwo;
    return ZzWorkspaceLayoutCodecPrivate::encodeVersionTwo(request);
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
    const auto captured = zzCaptureSnapshot(shell_);
    if (!captured.has_value() || !decoded.value().projection.has_value()) {
        return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    const ZzRuntimeSnapshot snapshot = *captured;
    ZzLayoutState::ZzLayoutRequest request = std::move(decoded).value();
    const bool versionOne = request.sourceSchema
        == ZzLayoutState::ZzLayoutRequest::ZzSourceSchema::VersionOne;
    const int migrationCurrent = versionOne
        ? request.projection->split.root.currentIndex : -1;
    const QString migrationGroup = versionOne
        ? snapshot.projection.split.groupOrder.value(0) : QString{};
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
        request.projection->split = snapshot.projection.split;
    }
    request.projection->bottom.order = snapshot.projection.bottom.order;
    const bool dockTargetReady = zzBuildDockTarget(
        snapshot, &request.projection->dock);
    const bool splitTargetReady = dockTargetReady
        && zzCanonicalizeSplitTarget(
            shell_.splitWorkspace, &request.projection->split,
            migrationGroup, migrationCurrent);
    const auto planned = splitTargetReady
        ? ZzLayoutState::buildRestoreTarget(snapshot.projection, request)
        : std::optional<ZzProjection>{};
    const ZzLayoutTransactionScope transaction(shell_);
    bool committed = planned.has_value();
    if (committed) {
        const ZzProjection target = *planned;
        committed = zzApplyDock(shell_, snapshot, target.dock);
        if (committed) {
            committed = zzApplySplit(
                shell_, snapshot, target.split,
                migrationGroup, migrationCurrent,
                snapshot.projection.split.canonicalState);
        }
        if (committed) {
            committed = zzApplySide(shell_, snapshot, target, true);
        }
        if (committed) {
            committed = zzApplyBottom(shell_, snapshot, target.bottom);
        }
        if (committed) {
            committed = zzApplyActivityAndTitle(shell_, snapshot, target);
        }
        if (committed) {
            committed = zzAuditAll(
                shell_, snapshot, target,
                migrationGroup, migrationCurrent);
        }
    }
    if (committed) {
        return ZzCore::ZzResult<void>::success();
    }
    const QByteArray alternateSplitCanonical = planned.has_value()
        ? planned->split.canonicalState : QByteArray{};
    const bool rolledBack = zzRollback(
        shell_, snapshot, migrationGroup, snapshotMigrationCurrent,
        alternateSplitCanonical);
    return zzFailure<void>(ZzCore::ZzErrorCode::InvalidState,
        rolledBack
            ? QStringLiteral(
                "Workspace layout restore failed and was rolled back")
            : QStringLiteral(
                "Workspace layout restore failed and rollback failed"));
}

} // namespace ZzPureTools
