#include "ZzWorkspaceShellPrivate.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneMode.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzWorkspaceShell.h>

namespace ZzPureTools {

namespace {

constexpr qsizetype zzMaximumLayoutSize = qsizetype{1024} * 1024;
constexpr quint16 zzLayoutSchemaVersion = 1;
constexpr auto zzLayoutStreamVersion = QDataStream::Qt_6_8;
constexpr int zzLayoutDigestSize = 32;
constexpr int zzLayoutHeaderSize = 12;
// 现有基准覆盖 64 个侧面板；4096 保持足够兼容余量，同时拒绝异常布局。
constexpr quint32 zzMaximumSideLayoutEntries = 4096;

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzWorkspaceFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), std::move(context)));
}

[[nodiscard]] bool zzIsSideArea(
    ZzFluentUI::ZzActivityArea area) noexcept
{
    switch (area) {
    case ZzFluentUI::ZzActivityArea::LeftPrimary:
    case ZzFluentUI::ZzActivityArea::LeftSecondary:
    case ZzFluentUI::ZzActivityArea::RightPrimary:
    case ZzFluentUI::ZzActivityArea::RightSecondary:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzIsLeftArea(
    ZzFluentUI::ZzActivityArea area) noexcept
{
    return area == ZzFluentUI::ZzActivityArea::LeftPrimary
        || area == ZzFluentUI::ZzActivityArea::LeftSecondary;
}

[[nodiscard]] bool zzIsDockArea(Qt::DockWidgetArea area) noexcept
{
    return area == Qt::LeftDockWidgetArea
        || area == Qt::RightDockWidgetArea
        || area == Qt::TopDockWidgetArea
        || area == Qt::BottomDockWidgetArea;
}

[[nodiscard]] bool zzIsTitleMode(ZzWorkspaceTitleMode mode) noexcept
{
    switch (mode) {
    case ZzWorkspaceTitleMode::Application:
    case ZzWorkspaceTitleMode::CurrentTab:
    case ZzWorkspaceTitleMode::CurrentTabAndApplication:
    case ZzWorkspaceTitleMode::Custom:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzIsCurrentThread(const QObject *object) noexcept
{
    return object != nullptr
        && object->thread() == QThread::currentThread();
}

struct ZzActivityRow final
{
    ZzWorkspacePanelId id;
    QString title;
    ZzFluentUI::ZzIconDescriptor icon;
    ZzFluentUI::ZzActivityArea area =
        ZzFluentUI::ZzActivityArea::LeftPrimary;
    int badge = 0;
};

/** @brief 保存 Side Panel 注册顺序并投影 Activity Bar 所需角色。 */
class ZzWorkspaceActivityModel final : public QAbstractListModel
{
public:
    explicit ZzWorkspaceActivityModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    [[nodiscard]] int rowCount(
        const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }

    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= static_cast<int>(rows_.size())) {
            return {};
        }
        const ZzActivityRow &row = rows_.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return row.title;
        case Qt::DecorationRole:
            return QVariant::fromValue(row.icon);
        case static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area):
            return QVariant::fromValue(row.area);
        case static_cast<int>(ZzFluentUI::ZzActivityItemRole::Badge):
            return row.badge;
        default:
            return {};
        }
    }

    [[nodiscard]] Qt::ItemFlags flags(
        const QModelIndex &index) const override
    {
        return index.isValid()
            ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
            : Qt::NoItemFlags;
    }

    void append(ZzActivityRow row)
    {
        const int rowIndex = static_cast<int>(rows_.size());
        beginInsertRows({}, rowIndex, rowIndex);
        rows_.append(std::move(row));
        endInsertRows();
    }

    [[nodiscard]] bool remove(const ZzWorkspacePanelId &id)
    {
        const int row = indexOf(id);
        if (row < 0) {
            return false;
        }
        beginRemoveRows({}, row, row);
        rows_.removeAt(row);
        endRemoveRows();
        return true;
    }

    [[nodiscard]] bool setBadge(
        const ZzWorkspacePanelId &id,
        int value)
    {
        const int row = indexOf(id);
        if (row < 0) {
            return false;
        }
        if (rows_[row].badge == value) {
            return true;
        }
        rows_[row].badge = value;
        Q_EMIT dataChanged(
            index(row, 0), index(row, 0),
            {static_cast<int>(ZzFluentUI::ZzActivityItemRole::Badge)});
        return true;
    }

    void setArea(
        const ZzWorkspacePanelId &id,
        ZzFluentUI::ZzActivityArea area)
    {
        const int row = indexOf(id);
        if (row < 0 || rows_[row].area == area) {
            return;
        }
        rows_[row].area = area;
        Q_EMIT dataChanged(
            index(row, 0), index(row, 0),
            {static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area)});
    }

    void reorder(const QVector<ZzWorkspacePanelId> &orderedIds)
    {
        QVector<ZzActivityRow> reordered;
        reordered.reserve(rows_.size());
        for (const ZzWorkspacePanelId &id : orderedIds) {
            const int row = indexOf(id);
            if (row >= 0) {
                reordered.append(rows_.at(row));
            }
        }
        for (const ZzActivityRow &row : std::as_const(rows_)) {
            bool alreadyAdded = false;
            for (const ZzActivityRow &candidate : std::as_const(reordered)) {
                if (candidate.id == row.id) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) {
                reordered.append(row);
            }
        }
        beginResetModel();
        rows_ = std::move(reordered);
        endResetModel();
    }

    [[nodiscard]] ZzWorkspacePanelId idAt(int row) const
    {
        return row >= 0 && row < static_cast<int>(rows_.size())
            ? rows_.at(row).id : ZzWorkspacePanelId{};
    }

    [[nodiscard]] QModelIndex indexFor(
        const ZzWorkspacePanelId &id) const
    {
        const int row = indexOf(id);
        return row >= 0 ? index(row, 0) : QModelIndex{};
    }

private:
    [[nodiscard]] int indexOf(const ZzWorkspacePanelId &id) const noexcept
    {
        for (qsizetype row = 0; row < rows_.size(); ++row) {
            if (rows_.at(row).id == id) {
                return static_cast<int>(row);
            }
        }
        return -1;
    }

    QVector<ZzActivityRow> rows_;
};

[[nodiscard]] ZzWorkspaceActivityModel *zzActivityModel(
    QAbstractListModel *model) noexcept
{
    return static_cast<ZzWorkspaceActivityModel *>(model);
}

[[nodiscard]] bool zzWritePayload(
    const ZzWorkspaceShellPrivate::ZzLayoutState &state,
    QByteArray *payload)
{
    Q_ASSERT(payload != nullptr);
    payload->clear();
    QDataStream stream(payload, QIODevice::WriteOnly);
    stream.setVersion(zzLayoutStreamVersion);
    stream << state.qtState
           << state.leftCollapsed
           << static_cast<qint32>(state.leftWidth)
           << state.rightCollapsed
           << static_cast<qint32>(state.rightWidth)
           << state.leftCurrent.value()
           << state.rightCurrent.value()
           << static_cast<quint32>(state.sideEntries.size());
    for (const auto &entry : state.sideEntries) {
        stream << entry.id.value()
               << static_cast<quint8>(entry.area)
               << static_cast<qint32>(entry.order);
    }
    stream << static_cast<qint32>(state.currentTabIndex)
           << static_cast<quint8>(state.titleMode);
    return stream.status() == QDataStream::Ok;
}

[[nodiscard]] bool zzReadPayload(
    const QByteArray &payload,
    ZzWorkspaceShellPrivate::ZzLayoutState *state)
{
    Q_ASSERT(state != nullptr);
    QDataStream stream(payload);
    stream.setVersion(zzLayoutStreamVersion);
    qint32 leftWidth = 0;
    qint32 rightWidth = 0;
    QString leftCurrent;
    QString rightCurrent;
    quint32 sideCount = 0;
    stream >> state->qtState
           >> state->leftCollapsed
           >> leftWidth
           >> state->rightCollapsed
           >> rightWidth
           >> leftCurrent
           >> rightCurrent
           >> sideCount;
    if (stream.status() != QDataStream::Ok
        || sideCount > zzMaximumSideLayoutEntries
        || leftWidth <= 0 || rightWidth <= 0
        || leftWidth > zzMaximumLayoutSize
        || rightWidth > zzMaximumLayoutSize) {
        return false;
    }
    state->leftWidth = leftWidth;
    state->rightWidth = rightWidth;
    state->leftCurrent = ZzWorkspacePanelId(std::move(leftCurrent));
    state->rightCurrent = ZzWorkspacePanelId(std::move(rightCurrent));
    state->sideEntries.clear();
    state->sideEntries.reserve(static_cast<qsizetype>(sideCount));
    QSet<QString> seenSideIds;
    seenSideIds.reserve(static_cast<qsizetype>(sideCount));
    for (quint32 index = 0; index < sideCount; ++index) {
        QString idValue;
        quint8 areaValue = 0;
        qint32 order = 0;
        stream >> idValue >> areaValue >> order;
        ZzWorkspaceShellPrivate::ZzSideLayoutEntry entry{
            ZzWorkspacePanelId(std::move(idValue)),
            static_cast<ZzFluentUI::ZzActivityArea>(areaValue),
            order};
        if (stream.status() != QDataStream::Ok
            || !entry.id.isValid()
            || !zzIsSideArea(entry.area)
            || entry.order < 0
            || seenSideIds.contains(entry.id.value())) {
            return false;
        }
        seenSideIds.insert(entry.id.value());
        state->sideEntries.append(std::move(entry));
    }
    qint32 tabIndex = -1;
    quint8 titleMode = 0;
    stream >> tabIndex >> titleMode;
    state->currentTabIndex = tabIndex;
    state->titleMode = static_cast<ZzWorkspaceTitleMode>(titleMode);
    return stream.status() == QDataStream::Ok
        && stream.atEnd()
        && tabIndex >= -1
        && zzIsTitleMode(state->titleMode);
}

[[nodiscard]] bool zzDecodeLayout(
    const QByteArray &encoded,
    ZzWorkspaceShellPrivate::ZzLayoutState *state)
{
    if (encoded.size() < zzLayoutHeaderSize + zzLayoutDigestSize
        || encoded.size() > zzMaximumLayoutSize) {
        return false;
    }
    QDataStream stream(encoded);
    stream.setVersion(zzLayoutStreamVersion);
    char magic[4]{};
    quint16 schemaVersion = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (stream.readRawData(magic, 4) != 4) {
        return false;
    }
    stream >> schemaVersion >> streamVersion >> payloadLength;
    if (QByteArrayView(magic, 4) != QByteArrayView("ZZWS", 4)
        || schemaVersion != zzLayoutSchemaVersion
        || streamVersion != static_cast<quint16>(zzLayoutStreamVersion)
        || payloadLength
            != static_cast<quint32>(
                encoded.size() - zzLayoutHeaderSize - zzLayoutDigestSize)) {
        return false;
    }
    QByteArray payload(static_cast<qsizetype>(payloadLength), Qt::Uninitialized);
    if (stream.readRawData(
            payload.data(), static_cast<int>(payloadLength))
        != static_cast<int>(payloadLength)) {
        return false;
    }
    QByteArray digest(zzLayoutDigestSize, Qt::Uninitialized);
    if (stream.readRawData(digest.data(), zzLayoutDigestSize)
            != zzLayoutDigestSize
        || stream.status() != QDataStream::Ok
        || !stream.atEnd()) {
        return false;
    }
    if (digest != QCryptographicHash::hash(
            payload, QCryptographicHash::Sha256)) {
        return false;
    }
    return zzReadPayload(payload, state);
}

} // namespace

ZzWorkspaceShellPrivate::ZzWorkspaceShellPrivate(
    ZzWorkspaceShell *publicObject,
    QMainWindow *hostWindow,
    ZzFluentUI::ZzFluentTitleBar *fluentTitleBar)
    : q_ptr(publicObject)
    , host(hostWindow)
    , titleBar(fluentTitleBar)
    , applicationTitle(hostWindow->windowTitle())
{
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(hostWindow != nullptr);
    workspaceRoot = new QWidget(hostWindow);
    workspaceRoot->setObjectName(QStringLiteral("zzWorkspaceRoot"));
    leftActivityBar = new ZzFluentUI::ZzActivityBar(
        ZzFluentUI::ZzSidePaneEdge::Left, workspaceRoot);
    leftSidePane = new ZzFluentUI::ZzSidePane(
        ZzFluentUI::ZzSidePaneEdge::Left, workspaceRoot);
    leftSidePane->setMode(ZzFluentUI::ZzSidePaneMode::Stacked);
    centerHost = new QWidget(workspaceRoot);
    centerHost->setObjectName(QStringLiteral("zzWorkspaceCenterHost"));
    splitWorkspace = new ZzFluentUI::ZzSplitWorkspace(centerHost);
    splitWorkspace->setObjectName(QStringLiteral("zzWorkspaceSplitWorkspace"));
    bottomPane = new ZzFluentUI::ZzBottomPane(centerHost);
    bottomPane->setObjectName(QStringLiteral("zzWorkspaceBottomPane"));
    bottomPane->setCollapsed(true);
    rightSidePane = new ZzFluentUI::ZzSidePane(
        ZzFluentUI::ZzSidePaneEdge::Right, workspaceRoot);
    rightSidePane->setMode(ZzFluentUI::ZzSidePaneMode::Stacked);
    rightActivityBar = new ZzFluentUI::ZzActivityBar(
        ZzFluentUI::ZzSidePaneEdge::Right, workspaceRoot);
    palette = new ZzFluentUI::ZzCommandPalette(workspaceRoot);
    activityModel = new ZzWorkspaceActivityModel(workspaceRoot);
    leftActivityBar->setModel(activityModel);
    rightActivityBar->setModel(activityModel);

    auto *centerLayout = new QVBoxLayout(centerHost);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);
    centerLayout->addWidget(splitWorkspace, 1);
    centerLayout->addWidget(bottomPane);

    auto *layout = new QHBoxLayout(workspaceRoot);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(leftActivityBar);
    layout->addWidget(leftSidePane);
    layout->addWidget(centerHost, 1);
    layout->addWidget(rightSidePane);
    layout->addWidget(rightActivityBar);
    syncSideEdgeVisibility();

    QObject::connect(
        leftActivityBar, &ZzFluentUI::ZzActivityBar::activationRequested,
        q_ptr, [this](const QModelIndex &index) {
            activateSidePanel(index, false);
        });
    QObject::connect(
        rightActivityBar, &ZzFluentUI::ZzActivityBar::activationRequested,
        q_ptr, [this](const QModelIndex &index) {
            activateSidePanel(index, false);
        });
    QObject::connect(
        leftActivityBar, &ZzFluentUI::ZzActivityBar::collapseRequested,
        q_ptr, [this](const QModelIndex &index) {
            activateSidePanel(index, true);
        });
    QObject::connect(
        rightActivityBar, &ZzFluentUI::ZzActivityBar::collapseRequested,
        q_ptr, [this](const QModelIndex &index) {
            activateSidePanel(index, true);
        });
    QObject::connect(
        splitWorkspace, &ZzFluentUI::ZzSplitWorkspace::activeGroupChanged,
        q_ptr, [this] { refreshActiveTabConnections(); });
    if (titleBar != nullptr) {
        QObject::connect(
            titleBar, &ZzFluentUI::ZzFluentTitleBar::alwaysOnTopRequested,
            q_ptr, [this](bool requested) {
                static_cast<void>(setAlwaysOnTop(requested));
            });
    }
    refreshActiveTabConnections();
}

ZzWorkspaceShellPrivate::~ZzWorkspaceShellPrivate()
{
    QObject::disconnect(activeTabChangedConnection);
    QObject::disconnect(activeTabPresentationConnection);
    QObject::disconnect(currentTabTitleConnection);
    for (ZzPanelRecord &record : panels) {
        QObject::disconnect(record.contentDestroyedConnection);
    }
    for (const ZzPanelRecord &record : std::as_const(panels)) {
        switch (record.kind) {
        case ZzPanelKind::Side:
        case ZzPanelKind::Bottom:
            break;
        case ZzPanelKind::Dock: {
            ZzFluentUI::ZzDockPanel *const dock = record.dock.data();
            if (record.removalInProgress && dock != nullptr) {
                if (!cleanupDockPanel(dock)) {
                    auto *const dockHost = qobject_cast<QMainWindow *>(
                        dock->parentWidget());
                    if (dockHost != nullptr
                        && dockHost->layout() != nullptr) {
                        dockHost->removeDockWidget(dock);
                    }
                    dock->hide();
                    dock->setParent(nullptr);
                }
                break;
            }
            auto *const dockHost = dock != nullptr
                ? qobject_cast<QMainWindow *>(dock->parentWidget()) : nullptr;
            if (dockHost != nullptr && dockHost->layout() != nullptr) {
                dockHost->removeDockWidget(dock);
                delete dock;
            }
            break;
        }
        }
    }
    panels.clear();
    if (workspaceRoot != nullptr) {
        delete workspaceRoot;
    }
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::registerSidePanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    QWidget *content)
{
    const QString normalizedTitle = title.trimmed();
    if (host == nullptr || workspaceRoot == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    if (!id.isValid() || normalizedTitle.isEmpty() || content == nullptr
        || !zzIsSideArea(area)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Invalid side panel registration"), id.value());
    }
    if (!zzIsCurrentThread(content) || content->parent() != nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel content must be unparented on the GUI thread"),
            id.value());
    }
    if (indexOf(id) >= 0) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace panel id is already registered"),
            id.value());
    }

    ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(area)
        ? leftSidePane.data() : rightSidePane.data();
    if (pane == nullptr || activityModel == nullptr
        || leftActivityBar == nullptr || rightActivityBar == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel rejected content"), id.value());
    }

    ZzPanelRecord record;
    record.id = id;
    record.title = normalizedTitle;
    record.icon = icon;
    record.kind = ZzPanelKind::Side;
    record.activityArea = area;
    record.content = content;
    record.contentIdentity = content;
    record.registrationGeneration = ++nextPanelRegistrationGeneration;
    record.registrationInProgress = true;
    panels.append(std::move(record));
    connectPanelContentDestroyed(id, content);

    const bool accepted = pane->addWidget(content, normalizedTitle)
        && pane->setCurrentWidget(content);
    int panelIndex = indexOf(id);
    if (!accepted || panelIndex < 0
        || panels.at(panelIndex).contentIdentity != content
        || panels.at(panelIndex).content != content) {
        rollbackPanelRegistration(id, content);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel rejected content"), id.value());
    }
    zzActivityModel(activityModel)->append(
        ZzActivityRow{id, normalizedTitle, std::move(icon), area, 0});
    const QModelIndex sourceIndex =
        zzActivityModel(activityModel)->indexFor(id);
    leftActivityBar->setCurrentSourceIndex(sourceIndex);
    rightActivityBar->setCurrentSourceIndex(sourceIndex);
    pane->setCollapsed(false);
    panelIndex = indexOf(id);
    if (panelIndex < 0
        || panels.at(panelIndex).contentIdentity != content
        || panels.at(panelIndex).content != content) {
        rollbackPanelRegistration(id, content);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel registration was interrupted"),
            id.value());
    }
    panels[panelIndex].registrationInProgress = false;
    syncSideEdgeVisibility();
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::registerBottomPanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    QWidget *content)
{
    const QString normalizedTitle = title.trimmed();
    if (host == nullptr || bottomPane == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    if (!id.isValid() || normalizedTitle.isEmpty() || content == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Invalid bottom panel registration"), id.value());
    }
    if (!zzIsCurrentThread(content) || content->parent() != nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Bottom panel content must be unparented on the GUI thread"),
            id.value());
    }
    if (indexOf(id) >= 0) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace panel id is already registered"),
            id.value());
    }

    ZzPanelRecord record;
    record.id = id;
    record.title = normalizedTitle;
    record.icon = icon;
    record.kind = ZzPanelKind::Bottom;
    record.content = content;
    record.contentIdentity = content;
    record.registrationGeneration = ++nextPanelRegistrationGeneration;
    record.registrationInProgress = true;
    panels.append(std::move(record));
    connectPanelContentDestroyed(id, content);

    const QPointer<ZzFluentUI::ZzBottomPane> paneGuard(bottomPane);
    const QPointer<QWidget> contentGuard(content);
    const bool accepted = paneGuard != nullptr
        && paneGuard->addWidget(content, normalizedTitle, icon);
    const int panelIndex = indexOf(id);
    if (!accepted || paneGuard == nullptr || contentGuard == nullptr
        || panelIndex < 0
        || panels.at(panelIndex).contentIdentity != content
        || panels.at(panelIndex).content != contentGuard) {
        rollbackPanelRegistration(id, content);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Bottom panel registration was interrupted"),
            id.value());
    }
    panels[panelIndex].registrationInProgress = false;
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::registerDockPanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    Qt::DockWidgetArea area,
    QWidget *content)
{
    const QString normalizedTitle = title.trimmed();
    if (host == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    if (!id.isValid() || normalizedTitle.isEmpty() || content == nullptr
        || !zzIsDockArea(area)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Invalid dock panel registration"), id.value());
    }
    if (!zzIsCurrentThread(content) || content->parent() != nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Dock content must be unparented on the GUI thread"),
            id.value());
    }
    if (indexOf(id) >= 0) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace panel id is already registered"),
            id.value());
    }

    auto *dock = new ZzFluentUI::ZzDockPanel(normalizedTitle);
    dock->setObjectName(
        QStringLiteral("zzWorkspaceDock:") + id.value());
    dock->setIconDescriptor(icon);

    ZzPanelRecord record;
    record.id = id;
    record.title = normalizedTitle;
    record.icon = std::move(icon);
    record.kind = ZzPanelKind::Dock;
    record.dockArea = area;
    record.content = content;
    record.contentIdentity = content;
    record.registrationGeneration = ++nextPanelRegistrationGeneration;
    record.dock = dock;
    record.dockIdentity = dock;
    record.registrationInProgress = true;
    panels.append(std::move(record));
    connectPanelContentDestroyed(id, content);

    QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
    dock->setWidget(content);
    int panelIndex = indexOf(id);
    if (dockGuard == nullptr || host == nullptr || panelIndex < 0
        || panels.at(panelIndex).contentIdentity != content
        || panels.at(panelIndex).content != content
        || panels.at(panelIndex).dock != dockGuard
        || dockGuard->widget() != content) {
        rollbackPanelRegistration(id, content);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Dock panel registration was interrupted"),
            id.value());
    }
    host->addDockWidget(area, dockGuard);
    panelIndex = indexOf(id);
    if (dockGuard == nullptr || host == nullptr || panelIndex < 0
        || panels.at(panelIndex).contentIdentity != content
        || panels.at(panelIndex).content != content
        || panels.at(panelIndex).dock != dockGuard
        || dockGuard->widget() != content) {
        rollbackPanelRegistration(id, content);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Dock panel registration was interrupted"),
            id.value());
    }
    panels[panelIndex].registrationInProgress = false;
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<QWidget *> ZzWorkspaceShellPrivate::takePanel(
    const ZzWorkspacePanelId &id)
{
    const int panelIndex = indexOf(id);
    if (panelIndex < 0) {
        return zzWorkspaceFailure<QWidget *>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("Workspace panel is not registered"), id.value());
    }
    ZzPanelRecord record = panels.at(panelIndex);
    if (record.registrationInProgress || record.removalInProgress) {
        return zzWorkspaceFailure<QWidget *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
    QPointer<QWidget> contentGuard(record.content);
    QWidget *content = nullptr;
    switch (record.kind) {
    case ZzPanelKind::Side: {
        ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(record.activityArea)
            ? leftSidePane.data() : rightSidePane.data();
        if (pane == nullptr || contentGuard == nullptr) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        panels[panelIndex].removalInProgress = true;
        QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
        content = pane->takeWidget(contentGuard);
        if (content == nullptr || contentGuard == nullptr) {
            const int currentIndex = indexOf(id);
            if (currentIndex >= 0
                && panels.at(currentIndex).contentIdentity
                    == record.contentIdentity) {
                if (contentGuard != nullptr) {
                    panels[currentIndex].removalInProgress = false;
                    connectPanelContentDestroyed(id, contentGuard);
                } else {
                    cleanupInterruptedPanelRemoval(
                        id, record.contentIdentity,
                        record.registrationGeneration);
                }
            }
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        if (content != contentGuard || contentGuard->parent() != nullptr
            || activityModel == nullptr
            || !zzActivityModel(activityModel)->remove(id)) {
            cleanupInterruptedPanelRemoval(
                id, record.contentIdentity, record.registrationGeneration);
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Side panel removal was interrupted"),
                id.value());
        }
        break;
    }
    case ZzPanelKind::Bottom: {
        const QPointer<ZzFluentUI::ZzBottomPane> paneGuard(bottomPane);
        if (paneGuard == nullptr || contentGuard == nullptr) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        panels[panelIndex].removalInProgress = true;
        QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
        content = paneGuard->takeWidget(contentGuard);
        if (content == nullptr || contentGuard == nullptr) {
            const int currentIndex = indexOf(id);
            if (currentIndex >= 0
                && panels.at(currentIndex).contentIdentity
                    == record.contentIdentity) {
                if (contentGuard != nullptr) {
                    panels[currentIndex].removalInProgress = false;
                    connectPanelContentDestroyed(id, contentGuard);
                } else {
                    cleanupInterruptedPanelRemoval(
                        id, record.contentIdentity,
                        record.registrationGeneration);
                }
            }
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        break;
    }
    case ZzPanelKind::Dock: {
        if (record.dock == nullptr || contentGuard == nullptr
            || record.dock->widget() != contentGuard) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        panels[panelIndex].removalInProgress = true;
        QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
        content = record.dock->takeContentWidget();
        if (content == nullptr || contentGuard == nullptr) {
            const int currentIndex = indexOf(id);
            if (currentIndex >= 0
                && panels.at(currentIndex).contentIdentity
                    == record.contentIdentity) {
                if (contentGuard != nullptr) {
                    panels[currentIndex].removalInProgress = false;
                    connectPanelContentDestroyed(id, contentGuard);
                } else {
                    cleanupInterruptedPanelRemoval(
                        id, record.contentIdentity,
                        record.registrationGeneration);
                }
            }
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        if (content != contentGuard || contentGuard->parent() != nullptr
            || record.dock == nullptr
            || record.dock.data() != record.dockIdentity
            || record.dock->widget() != nullptr) {
            cleanupInterruptedPanelRemoval(
                id, record.contentIdentity, record.registrationGeneration);
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Dock panel removal was interrupted"),
                id.value());
        }
        if (!cleanupDockPanel(record.dockIdentity)) {
            cleanupInterruptedPanelRemoval(
                id, record.contentIdentity, record.registrationGeneration);
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Dock panel removal was interrupted"),
                id.value());
        }
        break;
    }
    }
    const int currentIndex = stablePanelIndex(record);
    if (contentGuard == nullptr || content != contentGuard
        || contentGuard->parent() != nullptr || currentIndex < 0
        || !panels.at(currentIndex).removalInProgress) {
        cleanupInterruptedPanelRemoval(
            id, record.contentIdentity, record.registrationGeneration);
        return zzWorkspaceFailure<QWidget *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel removal was interrupted"),
            id.value());
    }
    panels.removeAt(currentIndex);
    if (record.kind == ZzPanelKind::Side) {
        syncSideEdgeVisibility();
    }
    if (contentGuard == nullptr || content != contentGuard
        || contentGuard->parent() != nullptr) {
        return zzWorkspaceFailure<QWidget *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel ownership changed during removal"),
            id.value());
    }
    return ZzCore::ZzResult<QWidget *>::success(content);
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::showPanel(
    const ZzWorkspacePanelId &id,
    bool visible)
{
    const int panelIndex = indexOf(id);
    if (panelIndex < 0) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("Workspace panel is not registered"), id.value());
    }
    const ZzPanelRecord record = panels.at(panelIndex);
    if (record.registrationInProgress || record.removalInProgress) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
    switch (record.kind) {
    case ZzPanelKind::Dock:
        if (record.dock == nullptr) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Dock panel has been destroyed"), id.value());
        }
        record.dock->setVisible(visible);
        return ZzCore::ZzResult<void>::success();
    case ZzPanelKind::Bottom: {
        const QPointer<ZzFluentUI::ZzBottomPane> paneGuard(bottomPane);
        const QPointer<QWidget> contentGuard(record.content);
        if (paneGuard == nullptr || contentGuard == nullptr) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Bottom panel has been destroyed"), id.value());
        }
        if (visible && !paneGuard->setCurrentWidget(contentGuard)) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Bottom panel content is no longer registered"),
                id.value());
        }
        const int currentIndex = stablePanelIndex(record);
        if (paneGuard == nullptr || contentGuard == nullptr
            || currentIndex < 0
            || panels.at(currentIndex).registrationInProgress
            || panels.at(currentIndex).removalInProgress
            || !paneGuard->isAncestorOf(contentGuard)
            || (visible && paneGuard->currentWidget() != contentGuard)) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Bottom panel state changed during activation"),
                id.value());
        }
        paneGuard->setCollapsed(!visible);
        const int finalIndex = stablePanelIndex(record);
        if (paneGuard == nullptr || contentGuard == nullptr
            || finalIndex < 0
            || panels.at(finalIndex).registrationInProgress
            || panels.at(finalIndex).removalInProgress
            || !paneGuard->isAncestorOf(contentGuard)
            || paneGuard->isCollapsed() != !visible
            || (visible && paneGuard->currentWidget() != contentGuard)) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Bottom panel state changed during visibility update"),
                id.value());
        }
        return ZzCore::ZzResult<void>::success();
    }
    case ZzPanelKind::Side:
        break;
    }
    ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(record.activityArea)
        ? leftSidePane.data() : rightSidePane.data();
    if (pane == nullptr || record.content == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel has been destroyed"), id.value());
    }
    if (visible) {
        if (!pane->setCurrentWidget(record.content)) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Side panel content is no longer registered"),
                id.value());
        }
        pane->setCollapsed(false);
        const QModelIndex sourceIndex =
            zzActivityModel(activityModel)->indexFor(id);
        leftActivityBar->setCurrentSourceIndex(sourceIndex);
        rightActivityBar->setCurrentSourceIndex(sourceIndex);
    } else {
        pane->setCollapsed(true);
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::setPanelBadge(
    const ZzWorkspacePanelId &id,
    int value)
{
    const int panelIndex = indexOf(id);
    if (panelIndex < 0) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("Workspace panel is not registered"), id.value());
    }
    if (value < 0 || panels.at(panelIndex).kind != ZzPanelKind::Side) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Badge requires a side panel and non-negative value"),
            id.value());
    }
    if (!zzActivityModel(activityModel)->setBadge(id, value)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Activity model lost the panel"), id.value());
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::setAlwaysOnTop(
    bool alwaysOnTop)
{
    if (host == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    const bool wasVisible = host->isVisible();
    const Qt::WindowStates previousState = host->windowState();
    host->setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
    host->setWindowState(previousState);
    if (wasVisible) {
        host->show();
    } else {
        host->hide();
    }
    const bool applied = host->windowFlags().testFlag(
        Qt::WindowStaysOnTopHint);
    if (titleBar != nullptr) {
        titleBar->setAlwaysOnTop(applied);
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<QByteArray> ZzWorkspaceShellPrivate::saveLayout() const
{
    if (host == nullptr || leftSidePane == nullptr
        || rightSidePane == nullptr || splitWorkspace == nullptr
        || activeTabs == nullptr) {
        return zzWorkspaceFailure<QByteArray>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    QByteArray payload;
    if (!zzWritePayload(captureLayoutState(), &payload)) {
        return zzWorkspaceFailure<QByteArray>(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("Failed to serialize workspace payload"));
    }
    const qsizetype totalSize = zzLayoutHeaderSize
        + payload.size() + zzLayoutDigestSize;
    if (totalSize > zzMaximumLayoutSize) {
        return zzWorkspaceFailure<QByteArray>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace layout exceeds 1 MiB"));
    }
    QByteArray encoded;
    encoded.reserve(totalSize);
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setVersion(zzLayoutStreamVersion);
    if (stream.writeRawData("ZZWS", 4) != 4) {
        return zzWorkspaceFailure<QByteArray>(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("Failed to write workspace magic"));
    }
    stream << zzLayoutSchemaVersion
           << static_cast<quint16>(zzLayoutStreamVersion)
           << static_cast<quint32>(payload.size());
    if (stream.writeRawData(payload.constData(), payload.size())
            != payload.size()
        || stream.status() != QDataStream::Ok) {
        return zzWorkspaceFailure<QByteArray>(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("Failed to write workspace payload"));
    }
    encoded.append(QCryptographicHash::hash(
        payload, QCryptographicHash::Sha256));
    return ZzCore::ZzResult<QByteArray>::success(std::move(encoded));
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::restoreLayout(
    const QByteArray &state)
{
    if (host == nullptr || leftSidePane == nullptr
        || rightSidePane == nullptr || splitWorkspace == nullptr
        || activeTabs == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    ZzLayoutState requested;
    if (!zzDecodeLayout(state, &requested)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace layout envelope is invalid"));
    }
    const ZzLayoutState snapshot = captureLayoutState();
    const bool restoredQt = host->restoreState(
        requested.qtState, zzLayoutSchemaVersion);
    const bool appliedShell = restoredQt && applyShellLayout(requested);
    if (!appliedShell) {
        const bool restoredShellSnapshot = applyShellLayout(snapshot);
        const bool restoredQtSnapshot = host->restoreState(
            snapshot.qtState, zzLayoutSchemaVersion);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            restoredShellSnapshot && restoredQtSnapshot
                ? QStringLiteral("Workspace layout restore failed and was rolled back")
                : QStringLiteral("Workspace layout restore failed and rollback failed"));
    }
    return ZzCore::ZzResult<void>::success();
}

void ZzWorkspaceShellPrivate::refreshTitle()
{
    const std::uint64_t refreshGeneration = ++titleRefreshGeneration;
    QString pageTitle;
    const QPointer<ZzFluentUI::ZzTabWidget> tabsGuard(activeTabs);
    const QPointer<QWidget> pageGuard = tabsGuard != nullptr
        ? tabsGuard->currentWidget() : nullptr;
    if (tabsGuard != nullptr && pageGuard != nullptr) {
        pageTitle = pageGuard->windowTitle();
        if (pageTitle.isEmpty()) {
            pageTitle = tabsGuard->tabText(tabsGuard->currentIndex());
        }
    }
    QString effectiveTitle;
    switch (titleMode) {
    case ZzWorkspaceTitleMode::Application:
        effectiveTitle = applicationTitle;
        break;
    case ZzWorkspaceTitleMode::CurrentTab:
        effectiveTitle = pageTitle.isEmpty() ? applicationTitle : pageTitle;
        break;
    case ZzWorkspaceTitleMode::CurrentTabAndApplication:
        if (pageTitle.isEmpty()) {
            effectiveTitle = applicationTitle;
        } else if (applicationTitle.isEmpty()) {
            effectiveTitle = pageTitle;
        } else {
            effectiveTitle = pageTitle + QStringLiteral(" - ")
                + applicationTitle;
        }
        break;
    case ZzWorkspaceTitleMode::Custom:
        effectiveTitle = customTitle.isEmpty()
            ? applicationTitle : customTitle;
        break;
    }
    if (host != nullptr) {
        host->setWindowTitle(effectiveTitle);
    }
    if (refreshGeneration != titleRefreshGeneration) {
        return;
    }
    if (titleBar != nullptr) {
        titleBar->setTitle(effectiveTitle);
    }
}

void ZzWorkspaceShellPrivate::refreshActiveTabConnections()
{
    QObject::disconnect(activeTabChangedConnection);
    QObject::disconnect(activeTabPresentationConnection);
    activeTabChangedConnection = {};
    activeTabPresentationConnection = {};
    activeTabs = splitWorkspace != nullptr
        ? splitWorkspace->tabWidget(splitWorkspace->activeGroupId()) : nullptr;
    if (activeTabs != nullptr) {
        activeTabChangedConnection = QObject::connect(
            activeTabs, &QTabWidget::currentChanged,
            q_ptr, [this] { refreshCurrentTabConnection(); });
        activeTabPresentationConnection = QObject::connect(
            activeTabs,
            &ZzFluentUI::ZzTabWidget::pagePresentationChanged,
            q_ptr, [this](QWidget *page) {
                const QPointer<ZzFluentUI::ZzTabWidget> tabsGuard(activeTabs);
                if (tabsGuard != nullptr
                    && tabsGuard->currentWidget() == page) {
                    refreshTitle();
                }
            });
    }
    refreshCurrentTabConnection();
}

void ZzWorkspaceShellPrivate::refreshCurrentTabConnection()
{
    QObject::disconnect(currentTabTitleConnection);
    currentTabTitleConnection = {};
    const QPointer<ZzFluentUI::ZzTabWidget> tabsGuard(activeTabs);
    const QPointer<QWidget> pageGuard = tabsGuard != nullptr
        ? tabsGuard->currentWidget() : nullptr;
    if (pageGuard != nullptr) {
        currentTabTitleConnection = QObject::connect(
            pageGuard, &QWidget::windowTitleChanged,
            q_ptr, [this] { refreshTitle(); });
    }
    refreshTitle();
}

void ZzWorkspaceShellPrivate::connectPanelContentDestroyed(
    const ZzWorkspacePanelId &id,
    QWidget *content)
{
    const int panelIndex = indexOf(id);
    if (panelIndex < 0 || content == nullptr
        || panels.at(panelIndex).contentIdentity != content) {
        return;
    }
    QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
    panels[panelIndex].contentDestroyedConnection = QObject::connect(
        content, &QObject::destroyed, q_ptr,
        [this, id, content] {
            handlePanelContentDestroyed(id, content);
        });
}

void ZzWorkspaceShellPrivate::handlePanelContentDestroyed(
    const ZzWorkspacePanelId &id,
    QWidget *contentIdentity)
{
    int panelIndex = indexOf(id);
    if (panelIndex < 0
        || panels.at(panelIndex).contentIdentity != contentIdentity
        || panels.at(panelIndex).registrationInProgress
        || panels.at(panelIndex).removalInProgress) {
        return;
    }

    panels[panelIndex].removalInProgress = true;
    QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
    const ZzPanelRecord record = panels.at(panelIndex);
    switch (record.kind) {
    case ZzPanelKind::Side:
        if (activityModel != nullptr) {
            static_cast<void>(zzActivityModel(activityModel)->remove(id));
        }
        break;
    case ZzPanelKind::Bottom:
        break;
    case ZzPanelKind::Dock: {
        auto *const dock = qobject_cast<ZzFluentUI::ZzDockPanel *>(
            record.dockIdentity);
        auto *const dockHost = dock != nullptr
            ? qobject_cast<QMainWindow *>(dock->parentWidget()) : nullptr;
        if (dockHost != nullptr && dockHost->layout() != nullptr) {
            dockHost->removeDockWidget(dock);
            dock->deleteLater();
        }
        break;
    }
    }

    panelIndex = stablePanelIndex(record);
    if (panelIndex >= 0) {
        panels.removeAt(panelIndex);
    }
    if (record.kind == ZzPanelKind::Side) {
        syncSideEdgeVisibility();
    }
}

void ZzWorkspaceShellPrivate::rollbackPanelRegistration(
    const ZzWorkspacePanelId &id,
    QWidget *contentIdentity)
{
    int panelIndex = indexOf(id);
    if (panelIndex < 0
        || panels.at(panelIndex).contentIdentity != contentIdentity
        || !panels.at(panelIndex).registrationInProgress) {
        return;
    }

    panels[panelIndex].removalInProgress = true;
    QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
    const ZzPanelRecord record = panels.at(panelIndex);
    switch (record.kind) {
    case ZzPanelKind::Side: {
        ZzFluentUI::ZzSidePane *const pane =
            zzIsLeftArea(record.activityArea)
            ? leftSidePane.data() : rightSidePane.data();
        if (pane != nullptr && record.content != nullptr) {
            static_cast<void>(pane->takeWidget(record.content));
        }
        if (activityModel != nullptr) {
            static_cast<void>(zzActivityModel(activityModel)->remove(id));
        }
        break;
    }
    case ZzPanelKind::Bottom:
        if (bottomPane != nullptr && record.content != nullptr) {
            static_cast<void>(bottomPane->takeWidget(record.content));
        }
        break;
    case ZzPanelKind::Dock:
        if (record.dock == nullptr) {
            break;
        }
        if (!cleanupDockPanel(record.dockIdentity)) {
            scheduleInterruptedPanelRemovalCleanup(
                id, contentIdentity, record.registrationGeneration);
            return;
        }
        break;
    }

    panelIndex = stablePanelIndex(record);
    if (panelIndex >= 0) {
        panels.removeAt(panelIndex);
    }
    if (record.kind == ZzPanelKind::Side) {
        syncSideEdgeVisibility();
    }
}

void ZzWorkspaceShellPrivate::cleanupInterruptedPanelRemoval(
    const ZzWorkspacePanelId &id,
    QWidget *contentIdentity,
    std::uint64_t registrationGeneration)
{
    int panelIndex = indexOf(id);
    if (panelIndex < 0
        || panels.at(panelIndex).contentIdentity != contentIdentity
        || panels.at(panelIndex).registrationGeneration
            != registrationGeneration) {
        return;
    }

    QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
    const ZzPanelRecord record = panels.at(panelIndex);
    switch (record.kind) {
    case ZzPanelKind::Side:
        if (activityModel != nullptr) {
            static_cast<void>(zzActivityModel(activityModel)->remove(id));
        }
        break;
    case ZzPanelKind::Bottom:
        break;
    case ZzPanelKind::Dock:
        if (record.dock != nullptr
            && !cleanupDockPanel(record.dockIdentity)) {
            scheduleInterruptedPanelRemovalCleanup(
                id, contentIdentity, registrationGeneration);
            return;
        }
        break;
    }

    panelIndex = stablePanelIndex(record);
    if (panelIndex >= 0) {
        panels.removeAt(panelIndex);
    }
    if (record.kind == ZzPanelKind::Side) {
        syncSideEdgeVisibility();
    }
}

bool ZzWorkspaceShellPrivate::cleanupDockPanel(
    ZzFluentUI::ZzDockPanel *dockIdentity)
{
    QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dockIdentity);
    const auto preserveForRetry = [&dockGuard] {
        if (dockGuard == nullptr) {
            return;
        }
        auto *const dockHost = qobject_cast<QMainWindow *>(
            dockGuard->parentWidget());
        if (dockHost != nullptr && dockHost->layout() != nullptr) {
            dockHost->removeDockWidget(dockGuard);
        }
        if (dockGuard != nullptr) {
            dockGuard->hide();
            dockGuard->setParent(nullptr);
        }
    };
    if (dockGuard == nullptr) {
        return true;
    }
    if (dockGuard->widget() != nullptr) {
        static_cast<void>(dockGuard->takeContentWidget());
    }
    if (dockGuard == nullptr || dockGuard->widget() != nullptr) {
        if (dockGuard != nullptr) {
            preserveForRetry();
            return false;
        }
        return true;
    }
    const QPointer<QMainWindow> dockHost(qobject_cast<QMainWindow *>(
        dockGuard->parentWidget()));
    if (dockHost != nullptr && dockHost->layout() != nullptr) {
        dockHost->removeDockWidget(dockGuard);
    }
    if (dockGuard == nullptr || dockGuard->widget() != nullptr) {
        if (dockGuard != nullptr) {
            preserveForRetry();
            return false;
        }
        return true;
    }
    if (dockHost == nullptr || dockHost->layout() != nullptr) {
        delete dockGuard;
    }
    return true;
}

void ZzWorkspaceShellPrivate::scheduleInterruptedPanelRemovalCleanup(
    const ZzWorkspacePanelId &id,
    QWidget *contentIdentity,
    std::uint64_t registrationGeneration)
{
    static_cast<void>(QMetaObject::invokeMethod(
        q_ptr,
        [this, id, contentIdentity, registrationGeneration] {
            cleanupInterruptedPanelRemoval(
                id, contentIdentity, registrationGeneration);
        },
        Qt::QueuedConnection));
}

void ZzWorkspaceShellPrivate::activateSidePanel(
    const QModelIndex &sourceIndex,
    bool collapse)
{
    if (!sourceIndex.isValid() || sourceIndex.model() != activityModel) {
        return;
    }
    const ZzWorkspacePanelId id =
        zzActivityModel(activityModel)->idAt(sourceIndex.row());
    const int panelIndex = indexOf(id);
    if (panelIndex < 0 || panels.at(panelIndex).kind != ZzPanelKind::Side) {
        return;
    }
    const ZzPanelRecord &record = panels.at(panelIndex);
    ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(record.activityArea)
        ? leftSidePane.data() : rightSidePane.data();
    if (collapse && pane != nullptr
        && pane->currentWidget() == record.content
        && !pane->isCollapsed()) {
        pane->setCollapsed(true);
        return;
    }
    static_cast<void>(showPanel(id, true));
}

void ZzWorkspaceShellPrivate::syncSideEdgeVisibility()
{
    const auto syncEdge = [this](
                              ZzFluentUI::ZzSidePane *pane,
                              ZzFluentUI::ZzActivityBar *bar,
                              bool left) {
        if (pane == nullptr || bar == nullptr) {
            return;
        }
        const bool hasPanel = std::any_of(
            panels.cbegin(), panels.cend(), [left](const ZzPanelRecord &record) {
                return record.kind == ZzPanelKind::Side
                    && record.content != nullptr
                    && zzIsLeftArea(record.activityArea) == left;
            });
        bar->setVisible(hasPanel);
        if (!hasPanel) {
            bar->setCurrentSourceIndex({});
            pane->setCollapsed(true);
        }
    };
    syncEdge(leftSidePane, leftActivityBar, true);
    syncEdge(rightSidePane, rightActivityBar, false);
}

int ZzWorkspaceShellPrivate::indexOf(
    const ZzWorkspacePanelId &id) const noexcept
{
    for (qsizetype index = 0; index < panels.size(); ++index) {
        if (panels.at(index).id == id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int ZzWorkspaceShellPrivate::stablePanelIndex(
    const ZzPanelRecord &expected) const noexcept
{
    const int panelIndex = indexOf(expected.id);
    if (panelIndex < 0) {
        return -1;
    }
    const ZzPanelRecord &current = panels.at(panelIndex);
    return current.kind == expected.kind
            && current.registrationGeneration
                == expected.registrationGeneration
            && current.contentIdentity == expected.contentIdentity
            && current.content == expected.content
            && current.dockIdentity == expected.dockIdentity
            && current.dock == expected.dock
        ? panelIndex : -1;
}

ZzWorkspacePanelId ZzWorkspaceShellPrivate::currentSideId(
    ZzFluentUI::ZzSidePane *pane) const
{
    if (pane == nullptr || pane->currentWidget() == nullptr) {
        return {};
    }
    for (const ZzPanelRecord &record : panels) {
        if (record.kind == ZzPanelKind::Side
            && record.content == pane->currentWidget()) {
            return record.id;
        }
    }
    return {};
}

ZzWorkspaceShellPrivate::ZzLayoutState
ZzWorkspaceShellPrivate::captureLayoutState() const
{
    ZzLayoutState state;
    state.qtState = host != nullptr
        ? host->saveState(zzLayoutSchemaVersion) : QByteArray{};
    if (leftSidePane != nullptr) {
        state.leftCollapsed = leftSidePane->isCollapsed();
        state.leftWidth = leftSidePane->lastExpandedWidth();
        state.leftCurrent = currentSideId(leftSidePane);
    }
    if (rightSidePane != nullptr) {
        state.rightCollapsed = rightSidePane->isCollapsed();
        state.rightWidth = rightSidePane->lastExpandedWidth();
        state.rightCurrent = currentSideId(rightSidePane);
    }
    std::array<int, 4> orders{};
    const auto *model = zzActivityModel(activityModel);
    for (int row = 0; row < model->rowCount(); ++row) {
        const int panelIndex = indexOf(model->idAt(row));
        if (panelIndex < 0
            || panels.at(panelIndex).kind != ZzPanelKind::Side) {
            continue;
        }
        const ZzPanelRecord &record = panels.at(panelIndex);
        const auto areaIndex = static_cast<std::size_t>(record.activityArea);
        state.sideEntries.append(ZzSideLayoutEntry{
            record.id, record.activityArea, orders.at(areaIndex)++});
    }
    state.currentTabIndex = activeTabs != nullptr
        ? activeTabs->currentIndex() : -1;
    state.titleMode = titleMode;
    return state;
}

bool ZzWorkspaceShellPrivate::applyShellLayout(const ZzLayoutState &state)
{
    if (leftSidePane == nullptr || rightSidePane == nullptr
        || splitWorkspace == nullptr || activeTabs == nullptr
        || activityModel == nullptr) {
        return false;
    }
    const ZzWorkspacePanelId oldLeftCurrent = currentSideId(leftSidePane);
    const ZzWorkspacePanelId oldRightCurrent = currentSideId(rightSidePane);
    QVector<ZzWorkspacePanelId> requestedOrder;
    requestedOrder.reserve(state.sideEntries.size());
    for (const ZzSideLayoutEntry &entry : state.sideEntries) {
        const int panelIndex = indexOf(entry.id);
        if (panelIndex < 0) {
            continue;
        }
        ZzPanelRecord &record = panels[panelIndex];
        if (record.kind != ZzPanelKind::Side) {
            continue;
        }
        record.activityArea = entry.area;
        zzActivityModel(activityModel)->setArea(entry.id, entry.area);
        requestedOrder.append(entry.id);
    }
    zzActivityModel(activityModel)->reorder(requestedOrder);

    for (const ZzPanelRecord &record : std::as_const(panels)) {
        if (record.kind != ZzPanelKind::Side || record.content == nullptr) {
            continue;
        }
        if (leftSidePane->takeWidget(record.content) == nullptr) {
            static_cast<void>(rightSidePane->takeWidget(record.content));
        }
    }

    auto addArea = [this, &state](ZzFluentUI::ZzActivityArea area) {
        QVector<const ZzSideLayoutEntry *> requested;
        for (const ZzSideLayoutEntry &entry : state.sideEntries) {
            if (entry.area == area && indexOf(entry.id) >= 0) {
                requested.append(&entry);
            }
        }
        std::sort(
            requested.begin(), requested.end(),
            [](const ZzSideLayoutEntry *left, const ZzSideLayoutEntry *right) {
                return left->order < right->order;
            });
        QVector<ZzWorkspacePanelId> added;
        auto addRecord = [this, area, &added](ZzPanelRecord &record) {
            if (record.kind != ZzPanelKind::Side
                || record.activityArea != area
                || record.content == nullptr
                || added.contains(record.id)) {
                return true;
            }
            ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(area)
                ? leftSidePane.data() : rightSidePane.data();
            if (pane == nullptr
                || !pane->addWidget(record.content, record.title)) {
                return false;
            }
            added.append(record.id);
            return true;
        };
        for (const ZzSideLayoutEntry *entry : requested) {
            ZzPanelRecord &record = panels[indexOf(entry->id)];
            if (!addRecord(record)) {
                return false;
            }
        }
        for (ZzPanelRecord &record : panels) {
            if (!addRecord(record)) {
                return false;
            }
        }
        return true;
    };
    for (const auto area : {
             ZzFluentUI::ZzActivityArea::LeftPrimary,
             ZzFluentUI::ZzActivityArea::LeftSecondary,
             ZzFluentUI::ZzActivityArea::RightPrimary,
             ZzFluentUI::ZzActivityArea::RightSecondary}) {
        if (!addArea(area)) {
            return false;
        }
    }

    leftSidePane->setPaneWidth(state.leftWidth);
    rightSidePane->setPaneWidth(state.rightWidth);
    if (leftSidePane->paneWidth() != state.leftWidth
        || rightSidePane->paneWidth() != state.rightWidth) {
        return false;
    }
    const auto restoreCurrent = [this](
                                    ZzFluentUI::ZzSidePane *pane,
                                    const ZzWorkspacePanelId &requested,
                                    const ZzWorkspacePanelId &fallback) {
        for (const ZzWorkspacePanelId &candidate : {requested, fallback}) {
            const int panelIndex = indexOf(candidate);
            if (panelIndex >= 0
                && panels.at(panelIndex).kind == ZzPanelKind::Side
                && panels.at(panelIndex).content != nullptr
                && pane->setCurrentWidget(panels.at(panelIndex).content)) {
                return;
            }
        }
    };
    restoreCurrent(leftSidePane, state.leftCurrent, oldLeftCurrent);
    restoreCurrent(rightSidePane, state.rightCurrent, oldRightCurrent);
    leftActivityBar->setCurrentSourceIndex(
        zzActivityModel(activityModel)->indexFor(
            currentSideId(leftSidePane)));
    rightActivityBar->setCurrentSourceIndex(
        zzActivityModel(activityModel)->indexFor(
            currentSideId(rightSidePane)));
    leftSidePane->setCollapsed(state.leftCollapsed);
    rightSidePane->setCollapsed(state.rightCollapsed);
    syncSideEdgeVisibility();
    const QPointer<ZzFluentUI::ZzTabWidget> tabsGuard(activeTabs);
    if (tabsGuard != nullptr && state.currentTabIndex >= 0
        && state.currentTabIndex < tabsGuard->count()) {
        tabsGuard->setCurrentIndex(state.currentTabIndex);
    }
    titleMode = state.titleMode;
    refreshCurrentTabConnection();
    return true;
}

} // namespace ZzPureTools
