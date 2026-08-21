#include "ZzWorkspaceShellPrivate.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QThread>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzWorkspaceShell.h>

namespace ZzPureTools {

namespace {

constexpr qsizetype zzMaximumLayoutSize = 1024 * 1024;
constexpr quint16 zzLayoutSchemaVersion = 1;
constexpr auto zzLayoutStreamVersion = QDataStream::Qt_6_8;
constexpr int zzLayoutDigestSize = 32;
constexpr int zzLayoutHeaderSize = 12;

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
    const ZzWorkspaceShellPrivate::LayoutState &state,
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
    ZzWorkspaceShellPrivate::LayoutState *state)
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
        || sideCount > static_cast<quint32>(zzMaximumLayoutSize / 8)
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
    for (quint32 index = 0; index < sideCount; ++index) {
        QString idValue;
        quint8 areaValue = 0;
        qint32 order = 0;
        stream >> idValue >> areaValue >> order;
        ZzWorkspaceShellPrivate::SideLayoutEntry entry{
            ZzWorkspacePanelId(std::move(idValue)),
            static_cast<ZzFluentUI::ZzActivityArea>(areaValue),
            order};
        if (stream.status() != QDataStream::Ok
            || !entry.id.isValid()
            || !zzIsSideArea(entry.area)
            || entry.order < 0) {
            return false;
        }
        for (const auto &existing : state->sideEntries) {
            if (existing.id == entry.id) {
                return false;
            }
        }
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
    ZzWorkspaceShellPrivate::LayoutState *state)
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
    tabs = new ZzFluentUI::ZzTabWidget(workspaceRoot);
    rightSidePane = new ZzFluentUI::ZzSidePane(
        ZzFluentUI::ZzSidePaneEdge::Right, workspaceRoot);
    rightActivityBar = new ZzFluentUI::ZzActivityBar(
        ZzFluentUI::ZzSidePaneEdge::Right, workspaceRoot);
    palette = new ZzFluentUI::ZzCommandPalette(workspaceRoot);
    activityModel = new ZzWorkspaceActivityModel(workspaceRoot);
    leftActivityBar->setModel(activityModel);
    rightActivityBar->setModel(activityModel);

    auto *layout = new QHBoxLayout(workspaceRoot);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(leftActivityBar);
    layout->addWidget(leftSidePane);
    layout->addWidget(tabs, 1);
    layout->addWidget(rightSidePane);
    layout->addWidget(rightActivityBar);

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
        tabs, &QTabWidget::currentChanged,
        q_ptr, [this] { refreshCurrentTabConnection(); });
    QObject::connect(
        tabs, &ZzFluentUI::ZzTabWidget::pagePresentationChanged,
        q_ptr, [this](QWidget *page) {
            if (tabs != nullptr && tabs->currentWidget() == page) {
                refreshTitle();
            }
        });
    if (titleBar != nullptr) {
        QObject::connect(
            titleBar, &ZzFluentUI::ZzFluentTitleBar::alwaysOnTopRequested,
            q_ptr, [this](bool requested) {
                static_cast<void>(setAlwaysOnTop(requested));
            });
    }
    refreshCurrentTabConnection();
}

ZzWorkspaceShellPrivate::~ZzWorkspaceShellPrivate()
{
    QObject::disconnect(currentTabTitleConnection);
    for (PanelRecord &record : panels) {
        QObject::disconnect(record.contentDestroyedConnection);
    }
    if (host != nullptr) {
        for (const PanelRecord &record : std::as_const(panels)) {
            if (record.dock != nullptr) {
                host->removeDockWidget(record.dock);
                delete record.dock;
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
    QString title,
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

    PanelRecord record;
    record.id = id;
    record.title = normalizedTitle;
    record.icon = icon;
    record.kind = PanelKind::Side;
    record.activityArea = area;
    record.content = content;
    record.contentIdentity = content;
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
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::registerDockPanel(
    const ZzWorkspacePanelId &id,
    QString title,
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

    PanelRecord record;
    record.id = id;
    record.title = normalizedTitle;
    record.icon = std::move(icon);
    record.kind = PanelKind::Dock;
    record.dockArea = area;
    record.content = content;
    record.contentIdentity = content;
    record.dock = dock;
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
    PanelRecord record = panels.at(panelIndex);
    if (record.registrationInProgress || record.removalInProgress) {
        return zzWorkspaceFailure<QWidget *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
    QWidget *content = nullptr;
    if (record.kind == PanelKind::Side) {
        ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(record.activityArea)
            ? leftSidePane.data() : rightSidePane.data();
        if (pane == nullptr || record.content == nullptr) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        panels[panelIndex].removalInProgress = true;
        QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
        content = pane->takeWidget(record.content);
        if (content == nullptr) {
            const int currentIndex = indexOf(id);
            if (currentIndex >= 0
                && panels.at(currentIndex).contentIdentity
                    == record.contentIdentity) {
                panels[currentIndex].removalInProgress = false;
                connectPanelContentDestroyed(id, record.content);
            }
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        static_cast<void>(zzActivityModel(activityModel)->remove(id));
    } else {
        if (record.dock == nullptr || record.content == nullptr
            || record.dock->widget() != record.content) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        panels[panelIndex].removalInProgress = true;
        QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
        content = record.dock->takeContentWidget();
        if (content == nullptr) {
            const int currentIndex = indexOf(id);
            if (currentIndex >= 0
                && panels.at(currentIndex).contentIdentity
                    == record.contentIdentity) {
                panels[currentIndex].removalInProgress = false;
                connectPanelContentDestroyed(id, record.content);
            }
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        if (host != nullptr) {
            host->removeDockWidget(record.dock);
        }
        delete record.dock;
    }
    const int currentIndex = indexOf(id);
    if (currentIndex >= 0
        && panels.at(currentIndex).contentIdentity == record.contentIdentity) {
        panels.removeAt(currentIndex);
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
    PanelRecord &record = panels[panelIndex];
    if (record.kind == PanelKind::Dock) {
        if (record.dock == nullptr) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Dock panel has been destroyed"), id.value());
        }
        record.dock->setVisible(visible);
        return ZzCore::ZzResult<void>::success();
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
    if (value < 0 || panels.at(panelIndex).kind != PanelKind::Side) {
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
        || rightSidePane == nullptr || tabs == nullptr) {
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
        || rightSidePane == nullptr || tabs == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"));
    }
    LayoutState requested;
    if (!zzDecodeLayout(state, &requested)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace layout envelope is invalid"));
    }
    const LayoutState snapshot = captureLayoutState();
    if (!host->restoreState(requested.qtState, zzLayoutSchemaVersion)
        || !applyShellLayout(requested)) {
        static_cast<void>(applyShellLayout(snapshot));
        static_cast<void>(host->restoreState(
            snapshot.qtState, zzLayoutSchemaVersion));
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace layout restore failed and was rolled back"));
    }
    return ZzCore::ZzResult<void>::success();
}

void ZzWorkspaceShellPrivate::refreshTitle()
{
    QString pageTitle;
    if (tabs != nullptr && tabs->currentWidget() != nullptr) {
        pageTitle = tabs->currentWidget()->windowTitle();
        if (pageTitle.isEmpty()) {
            pageTitle = tabs->tabText(tabs->currentIndex());
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
    if (titleBar != nullptr) {
        titleBar->setTitle(effectiveTitle);
    }
}

void ZzWorkspaceShellPrivate::refreshCurrentTabConnection()
{
    QObject::disconnect(currentTabTitleConnection);
    currentTabTitleConnection = {};
    if (tabs != nullptr && tabs->currentWidget() != nullptr) {
        currentTabTitleConnection = QObject::connect(
            tabs->currentWidget(), &QWidget::windowTitleChanged,
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
    const PanelRecord record = panels.at(panelIndex);
    if (record.kind == PanelKind::Side) {
        if (activityModel != nullptr) {
            static_cast<void>(zzActivityModel(activityModel)->remove(id));
        }
    } else if (record.dock != nullptr) {
        const bool hostCanManageDock = host != nullptr
            && host->layout() != nullptr;
        if (hostCanManageDock) {
            host->removeDockWidget(record.dock);
            record.dock->deleteLater();
        }
    }

    panelIndex = indexOf(id);
    if (panelIndex >= 0
        && panels.at(panelIndex).contentIdentity == contentIdentity) {
        panels.removeAt(panelIndex);
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
    const PanelRecord record = panels.at(panelIndex);
    if (record.kind == PanelKind::Side) {
        ZzFluentUI::ZzSidePane *const pane =
            zzIsLeftArea(record.activityArea)
            ? leftSidePane.data() : rightSidePane.data();
        if (pane != nullptr && record.content != nullptr) {
            static_cast<void>(pane->takeWidget(record.content));
        }
        if (activityModel != nullptr) {
            static_cast<void>(zzActivityModel(activityModel)->remove(id));
        }
    } else if (record.dock != nullptr) {
        if (record.content != nullptr
            && record.dock->widget() == record.content) {
            static_cast<void>(record.dock->takeContentWidget());
        }
        if (host != nullptr) {
            host->removeDockWidget(record.dock);
        }
        delete record.dock;
    }

    panelIndex = indexOf(id);
    if (panelIndex >= 0
        && panels.at(panelIndex).contentIdentity == contentIdentity) {
        panels.removeAt(panelIndex);
    }
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
    if (panelIndex < 0 || panels.at(panelIndex).kind != PanelKind::Side) {
        return;
    }
    const PanelRecord &record = panels.at(panelIndex);
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

ZzWorkspacePanelId ZzWorkspaceShellPrivate::currentSideId(
    ZzFluentUI::ZzSidePane *pane) const
{
    if (pane == nullptr || pane->currentWidget() == nullptr) {
        return {};
    }
    for (const PanelRecord &record : panels) {
        if (record.kind == PanelKind::Side
            && record.content == pane->currentWidget()) {
            return record.id;
        }
    }
    return {};
}

ZzWorkspaceShellPrivate::LayoutState
ZzWorkspaceShellPrivate::captureLayoutState() const
{
    LayoutState state;
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
            || panels.at(panelIndex).kind != PanelKind::Side) {
            continue;
        }
        const PanelRecord &record = panels.at(panelIndex);
        const auto areaIndex = static_cast<std::size_t>(record.activityArea);
        state.sideEntries.append(SideLayoutEntry{
            record.id, record.activityArea, orders.at(areaIndex)++});
    }
    state.currentTabIndex = tabs != nullptr ? tabs->currentIndex() : -1;
    state.titleMode = titleMode;
    return state;
}

bool ZzWorkspaceShellPrivate::applyShellLayout(const LayoutState &state)
{
    if (leftSidePane == nullptr || rightSidePane == nullptr
        || tabs == nullptr || activityModel == nullptr) {
        return false;
    }
    const ZzWorkspacePanelId oldLeftCurrent = currentSideId(leftSidePane);
    const ZzWorkspacePanelId oldRightCurrent = currentSideId(rightSidePane);
    QVector<ZzWorkspacePanelId> requestedOrder;
    requestedOrder.reserve(state.sideEntries.size());
    for (const SideLayoutEntry &entry : state.sideEntries) {
        const int panelIndex = indexOf(entry.id);
        if (panelIndex < 0) {
            continue;
        }
        PanelRecord &record = panels[panelIndex];
        if (record.kind != PanelKind::Side) {
            continue;
        }
        record.activityArea = entry.area;
        zzActivityModel(activityModel)->setArea(entry.id, entry.area);
        requestedOrder.append(entry.id);
    }
    zzActivityModel(activityModel)->reorder(requestedOrder);

    for (const PanelRecord &record : std::as_const(panels)) {
        if (record.kind != PanelKind::Side || record.content == nullptr) {
            continue;
        }
        if (leftSidePane->takeWidget(record.content) == nullptr) {
            static_cast<void>(rightSidePane->takeWidget(record.content));
        }
    }

    auto addArea = [this, &state](ZzFluentUI::ZzActivityArea area) {
        QVector<const SideLayoutEntry *> requested;
        for (const SideLayoutEntry &entry : state.sideEntries) {
            if (entry.area == area && indexOf(entry.id) >= 0) {
                requested.append(&entry);
            }
        }
        std::sort(
            requested.begin(), requested.end(),
            [](const SideLayoutEntry *left, const SideLayoutEntry *right) {
                return left->order < right->order;
            });
        QVector<ZzWorkspacePanelId> added;
        auto addRecord = [this, area, &added](PanelRecord &record) {
            if (record.kind != PanelKind::Side
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
        for (const SideLayoutEntry *entry : requested) {
            PanelRecord &record = panels[indexOf(entry->id)];
            if (!addRecord(record)) {
                return false;
            }
        }
        for (PanelRecord &record : panels) {
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
                && panels.at(panelIndex).kind == PanelKind::Side
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
    if (state.currentTabIndex >= 0
        && state.currentTabIndex < tabs->count()) {
        tabs->setCurrentIndex(state.currentTabIndex);
    }
    titleMode = state.titleMode;
    refreshCurrentTabConnection();
    return true;
}

} // namespace ZzPureTools
