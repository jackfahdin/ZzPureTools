#include "ZzWorkspaceShellPrivate.h"

#include <algorithm>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QEvent>
#include <QtCore/QHash>
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
#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneMode.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzWorkspaceShell.h>

#include "ZzWorkspaceActivityMoveTransactionPrivate.h"
#include "ZzWorkspaceLayoutTransactionPrivate.h"

namespace ZzPureTools {

namespace {

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

/** @brief 判断 Side Area 是否属于物理栈中应优先排列的 Primary 行。 */
[[nodiscard]] bool zzIsPrimaryArea(
    ZzFluentUI::ZzActivityArea area) noexcept
{
    return area == ZzFluentUI::ZzActivityArea::LeftPrimary
        || area == ZzFluentUI::ZzActivityArea::RightPrimary;
}

/** @brief 按注册表快照计算新 Side 内容在所属物理栈中的固定位置。 */
[[nodiscard]] int zzSideRegistrationTargetIndex(
    const QVector<ZzWorkspaceShellPrivate::ZzPanelRecord> &panels,
    ZzFluentUI::ZzActivityArea area) noexcept
{
    const bool left = zzIsLeftArea(area);
    int primaryCount = 0;
    int sideCount = 0;
    for (const ZzWorkspaceShellPrivate::ZzPanelRecord &record : panels) {
        if (record.kind != ZzWorkspaceShellPrivate::ZzPanelKind::Side
            || record.contentIdentity == nullptr || record.content == nullptr
            || record.content.data() != record.contentIdentity
            || record.removalInProgress
            || zzIsLeftArea(record.activityArea) != left) {
            continue;
        }
        ++sideCount;
        if (zzIsPrimaryArea(record.activityArea)) {
            ++primaryCount;
        }
    }
    return zzIsPrimaryArea(area) ? primaryCount : sideCount;
}

/** @brief 验证内容仍由注册时捕获的 PanelStack 内部框架直接持有。 */
[[nodiscard]] bool zzHasExpectedPanelStackContentOwner(
    const QPointer<QWidget> &content,
    const QPointer<QWidget> &owner,
    QWidget *ownerIdentity,
    const QPointer<ZzFluentUI::ZzPanelStack> &stack) noexcept
{
    return content != nullptr && owner != nullptr
        && owner.data() == ownerIdentity
        && content->parentWidget() == owner
        && stack != nullptr && stack->isAncestorOf(owner);
}

/** @brief 在一次 Side 注册期间固定首次合法框架 owner，并记录后续换父污染。 */
class ZzPanelOwnerObserver final : public QObject
{
public:
    ZzPanelOwnerObserver(
        QWidget *content,
        ZzFluentUI::ZzPanelStack *stack)
        : content_(content)
        , stack_(stack)
    {
        if (content_ != nullptr) {
            content_->installEventFilter(this);
        }
    }

    ~ZzPanelOwnerObserver() override
    {
        if (content_ != nullptr) {
            content_->removeEventFilter(this);
        }
    }

    [[nodiscard]] QPointer<QWidget> owner() const noexcept
    {
        return owner_;
    }

    [[nodiscard]] QWidget *ownerIdentity() const noexcept
    {
        return ownerIdentity_;
    }

    [[nodiscard]] bool hasCapturedOwner() const noexcept
    {
        return ownerIdentity_ != nullptr;
    }

    [[nodiscard]] bool isPolluted() const noexcept
    {
        return polluted_;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == content_.data() && event != nullptr
            && event->type() == QEvent::ParentChange) {
            QWidget *const currentOwner = content_ != nullptr
                ? content_->parentWidget() : nullptr;
            if (ownerIdentity_ == nullptr) {
                if (currentOwner != nullptr && stack_ != nullptr
                    && stack_->isAncestorOf(currentOwner)) {
                    owner_ = currentOwner;
                    ownerIdentity_ = currentOwner;
                } else {
                    polluted_ = true;
                }
            } else {
                polluted_ = true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QWidget> content_;
    QPointer<ZzFluentUI::ZzPanelStack> stack_;
    QPointer<QWidget> owner_;
    QWidget *ownerIdentity_ = nullptr;
    bool polluted_ = false;
};

[[nodiscard]] bool zzIsDockArea(Qt::DockWidgetArea area) noexcept
{
    return area == Qt::LeftDockWidgetArea
        || area == Qt::RightDockWidgetArea
        || area == Qt::TopDockWidgetArea
        || area == Qt::BottomDockWidgetArea;
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

    [[nodiscard]] QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry>
    placements() const
    {
        QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> result;
        result.reserve(rows_.size());
        for (qsizetype index = 0; index < rows_.size(); ++index) {
            const ZzActivityRow &row = rows_.at(index);
            result.append({row.id, row.area, static_cast<int>(index)});
        }
        return result;
    }

    [[nodiscard]] bool replaceRows(
        const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> &rows)
    {
        if (rows.size() != rows_.size()) {
            return false;
        }
        QHash<ZzWorkspacePanelId, int> sourceRows;
        sourceRows.reserve(rows_.size());
        for (qsizetype index = 0; index < rows_.size(); ++index) {
            if (sourceRows.contains(rows_.at(index).id)) {
                return false;
            }
            sourceRows.insert(
                rows_.at(index).id, static_cast<int>(index));
        }
        QSet<ZzWorkspacePanelId> seen;
        seen.reserve(rows.size());
        QVector<ZzActivityRow> replacement;
        replacement.reserve(rows_.size());
        for (const auto &placement : rows) {
            const auto source = sourceRows.constFind(placement.id);
            if (source == sourceRows.cend()
                || seen.contains(placement.id)) {
                return false;
            }
            seen.insert(placement.id);
            ZzActivityRow item = rows_.at(source.value());
            item.area = placement.area;
            replacement.append(std::move(item));
        }
        beginResetModel();
        rows_ = std::move(replacement);
        endResetModel();
        return true;
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
    leftActivityBar->setMultiActiveEnabled(true);
    rightActivityBar->setMultiActiveEnabled(true);
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
        leftActivityBar, &ZzFluentUI::ZzActivityBar::moveRequested,
        q_ptr, [this](const QModelIndex &index,
                   ZzFluentUI::ZzActivityArea area, int row) {
            moveSidePanel(index, area, row);
        });
    QObject::connect(
        rightActivityBar, &ZzFluentUI::ZzActivityBar::moveRequested,
        q_ptr, [this](const QModelIndex &index,
                   ZzFluentUI::ZzActivityArea area, int row) {
            moveSidePanel(index, area, row);
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
            if (record.removalInProgress) {
                cleanupPendingDockPanelForDestruction(record);
                break;
            }
            ZzFluentUI::ZzDockPanel *const dock = record.dock.data();
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
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
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

    const QVector<ZzSideLayoutEntry> rowsBefore = activityRows();
    const int targetStackIndex = zzSideRegistrationTargetIndex(panels, area);
    const QPointer<QMainWindow> hostGuard(host);
    const QPointer<QWidget> rootGuard(workspaceRoot);
    const QPointer<ZzFluentUI::ZzSidePane> paneGuard(pane);
    const QPointer<ZzFluentUI::ZzPanelStack> stackGuard(pane->panelStack());
    const QPointer<QWidget> contentGuard(content);
    if (stackGuard == nullptr || targetStackIndex < 0
        || targetStackIndex > stackGuard->panels().size()) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel rejected content"), id.value());
    }
    QList<QPointer<QWidget>> appendOrder;
    const QList<QWidget *> panelsBefore = stackGuard->panels();
    appendOrder.reserve(panelsBefore.size() + 1);
    for (QWidget *const panel : panelsBefore) {
        appendOrder.append(panel);
    }
    appendOrder.append(contentGuard);
    QList<QPointer<QWidget>> canonicalOrder = appendOrder;
    canonicalOrder.removeLast();
    canonicalOrder.insert(targetStackIndex, contentGuard);

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
    const ZzPanelRecord expectedRecord = panels.constLast();
    ZzPanelOwnerObserver ownerObserver(contentGuard, stackGuard);
    QPointer<QWidget> contentOwnerGuard;
    QWidget *contentOwnerIdentity = nullptr;
    const auto sameRows = [](const QVector<ZzSideLayoutEntry> &actual,
                             const QVector<ZzSideLayoutEntry> &expected) {
        if (actual.size() != expected.size()) {
            return false;
        }
        for (qsizetype index = 0; index < actual.size(); ++index) {
            if (actual.at(index).id != expected.at(index).id
                || actual.at(index).area != expected.at(index).area
                || actual.at(index).order != expected.at(index).order) {
                return false;
            }
        }
        return true;
    };
    const auto audit = [this, &expectedRecord, &hostGuard, &rootGuard,
                        &paneGuard, &stackGuard, &contentGuard,
                        &contentOwnerGuard, &contentOwnerIdentity,
                        &ownerObserver, &sameRows](
                           const QList<QPointer<QWidget>> &expectedOrder,
                           const QVector<ZzSideLayoutEntry> &expectedRows,
                           bool registrationInProgress) {
        const int panelIndex = stablePanelIndex(expectedRecord);
        if (hostGuard == nullptr || hostGuard != host
            || rootGuard == nullptr || rootGuard != workspaceRoot
            || paneGuard == nullptr
            || paneGuard != (zzIsLeftArea(expectedRecord.activityArea)
                    ? leftSidePane : rightSidePane)
            || stackGuard == nullptr || stackGuard != paneGuard->panelStack()
            || contentGuard == nullptr
            || !ownerObserver.hasCapturedOwner()
            || ownerObserver.isPolluted()
            || panelIndex < 0
            || panels.at(panelIndex).contentIdentity != contentGuard
            || panels.at(panelIndex).content != contentGuard
            || panels.at(panelIndex).contentOwner != contentOwnerGuard
            || panels.at(panelIndex).contentOwnerIdentity
                != contentOwnerIdentity
            || panels.at(panelIndex).registrationGeneration
                != expectedRecord.registrationGeneration
            || panels.at(panelIndex).registrationInProgress
                != registrationInProgress
            || panels.at(panelIndex).removalInProgress) {
            return false;
        }
        const QList<QWidget *> actualOrder = stackGuard->panels();
        if (actualOrder.size() != expectedOrder.size()) {
            return false;
        }
        for (qsizetype index = 0; index < actualOrder.size(); ++index) {
            if (actualOrder.at(index) != expectedOrder.at(index)) {
                return false;
            }
        }
        return zzHasExpectedPanelStackContentOwner(
                   contentGuard, contentOwnerGuard,
                   contentOwnerIdentity, stackGuard)
            && paneGuard->isAncestorOf(contentGuard)
            && stackGuard->isAncestorOf(contentGuard)
            && sameRows(activityRows(), expectedRows);
    };
    const auto reject = [this, &id, content] {
        rollbackPanelRegistration(id, content);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel registration was interrupted"), id.value());
    };

    const bool added = paneGuard->addWidget(contentGuard, normalizedTitle);
    const int panelIndexAfterAdd = stablePanelIndex(expectedRecord);
    contentOwnerGuard = ownerObserver.owner();
    contentOwnerIdentity = ownerObserver.ownerIdentity();
    if (panelIndexAfterAdd >= 0) {
        panels[panelIndexAfterAdd].contentOwner = contentOwnerGuard;
        panels[panelIndexAfterAdd].contentOwnerIdentity = contentOwnerIdentity;
    }
    if (!added
        || !audit(appendOrder, rowsBefore, true)) {
        return reject();
    }
    if (!paneGuard->setCurrentWidget(contentGuard)
        || !audit(appendOrder, rowsBefore, true)) {
        return reject();
    }
    if (targetStackIndex != appendOrder.size() - 1
        && !stackGuard->movePanel(contentGuard, targetStackIndex)) {
        return reject();
    }
    if (!audit(canonicalOrder, rowsBefore, true)) {
        return reject();
    }
    zzActivityModel(activityModel)->append(
        ZzActivityRow{id, normalizedTitle, std::move(icon), area, 0});
    QVector<ZzSideLayoutEntry> rowsAfter = rowsBefore;
    rowsAfter.append({id, area, static_cast<int>(rowsAfter.size())});
    if (!audit(canonicalOrder, rowsAfter, true)) {
        return reject();
    }
    const QModelIndex sourceIndex =
        zzActivityModel(activityModel)->indexFor(id);
    if (!sourceIndex.isValid() || sourceIndex.model() != activityModel) {
        return reject();
    }
    leftActivityBar->setCurrentSourceIndex(sourceIndex);
    if (!audit(canonicalOrder, rowsAfter, true)) {
        return reject();
    }
    rightActivityBar->setCurrentSourceIndex(sourceIndex);
    if (!audit(canonicalOrder, rowsAfter, true)) {
        return reject();
    }
    ZzFluentUI::ZzActivityBar *const owningBar = zzIsLeftArea(area)
        ? leftActivityBar.data() : rightActivityBar.data();
    QList<QModelIndex> activeIndexes = owningBar->activeSourceIndexes();
    activeIndexes.append(sourceIndex);
    owningBar->setActiveSourceIndexes(activeIndexes);
    if (!audit(canonicalOrder, rowsAfter, true)) {
        return reject();
    }
    paneGuard->setCollapsed(false);
    if (!audit(canonicalOrder, rowsAfter, true)) {
        return reject();
    }
    syncSideEdgeVisibility();
    if (!audit(canonicalOrder, rowsAfter, true)) {
        return reject();
    }
    const int panelIndex = stablePanelIndex(expectedRecord);
    if (panelIndex < 0) {
        return reject();
    }
    panels[panelIndex].registrationInProgress = false;
    if (!audit(canonicalOrder, rowsAfter, false)) {
        return reject();
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::registerBottomPanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    QWidget *content)
{
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
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
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
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
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<QWidget *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
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
    if (record.kind == ZzPanelKind::Side
        && transactionKind == ZzTransactionKind::None) {
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
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
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
    const QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(leftSidePane);
    const QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(rightSidePane);
    const QPointer<ZzFluentUI::ZzPanelStack> leftStackGuard =
        leftPaneGuard != nullptr ? leftPaneGuard->panelStack() : nullptr;
    const QPointer<ZzFluentUI::ZzPanelStack> rightStackGuard =
        rightPaneGuard != nullptr ? rightPaneGuard->panelStack() : nullptr;
    const QPointer<ZzFluentUI::ZzActivityBar> leftBarGuard(leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBarGuard(rightActivityBar);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    const QPointer<QWidget> contentGuard(record.content);
    const QVector<ZzPanelRecord> expectedPanels = panels;
    ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(record.activityArea)
        ? leftPaneGuard.data() : rightPaneGuard.data();
    ZzFluentUI::ZzPanelStack *const stack = zzIsLeftArea(record.activityArea)
        ? leftStackGuard.data() : rightStackGuard.data();
    if (pane == nullptr || stack == nullptr || contentGuard == nullptr
        || leftPaneGuard == nullptr || rightPaneGuard == nullptr
        || leftStackGuard == nullptr || rightStackGuard == nullptr
        || leftBarGuard == nullptr || rightBarGuard == nullptr
        || modelGuard == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel has been destroyed"), id.value());
    }

    const auto stable = [this, &expectedPanels, &leftPaneGuard,
                            &rightPaneGuard, &leftStackGuard,
                            &rightStackGuard, &leftBarGuard,
                            &rightBarGuard, &modelGuard] {
        if (leftPaneGuard == nullptr || rightPaneGuard == nullptr
            || leftStackGuard == nullptr || rightStackGuard == nullptr
            || leftBarGuard == nullptr || rightBarGuard == nullptr
            || modelGuard == nullptr
            || leftSidePane != leftPaneGuard
            || rightSidePane != rightPaneGuard
            || leftPaneGuard->panelStack() != leftStackGuard
            || rightPaneGuard->panelStack() != rightStackGuard
            || leftActivityBar != leftBarGuard
            || rightActivityBar != rightBarGuard
            || activityModel != modelGuard) {
            return false;
        }
        for (const ZzPanelRecord &expected : expectedPanels) {
            if (expected.kind != ZzPanelKind::Side) {
                continue;
            }
            if (stablePanelIndex(expected) < 0
                || expected.content == nullptr
                || expected.content.data() != expected.contentIdentity) {
                return false;
            }
            const bool left = zzIsLeftArea(expected.activityArea);
            ZzFluentUI::ZzSidePane *const expectedPane = left
                ? leftPaneGuard.data() : rightPaneGuard.data();
            ZzFluentUI::ZzPanelStack *const expectedStack = left
                ? leftStackGuard.data() : rightStackGuard.data();
            if (!expectedStack->panels().contains(expected.content)
                || !expectedStack->isAncestorOf(expected.content)
                || !expectedPane->isAncestorOf(expected.content)) {
                return false;
            }
        }
        return true;
    };
    if (!pane->setWidgetVisible(contentGuard, visible)
        || !stable()
        || stack->isPanelVisible(contentGuard) != visible) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel visibility update was interrupted"),
            id.value());
    }
    if (visible && (!pane->setCurrentWidget(contentGuard)
            || !stable() || pane->currentWidget() != contentGuard)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel activation was interrupted"), id.value());
    }
    pane->setCollapsed(pane->visibleWidgets().isEmpty());
    if (!stable()) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel collapse update was interrupted"),
            id.value());
    }

    const auto indexesFor = [this, &modelGuard](
                                ZzFluentUI::ZzSidePane *sidePane) {
        QList<QModelIndex> indexes;
        for (QWidget *const widget : sidePane->visibleWidgets()) {
            for (const ZzPanelRecord &candidate : std::as_const(panels)) {
                if (candidate.kind == ZzPanelKind::Side
                    && candidate.content == widget) {
                    indexes.append(
                        zzActivityModel(modelGuard)->indexFor(candidate.id));
                    break;
                }
            }
        }
        return indexes;
    };
    leftBarGuard->setCurrentSourceIndex(
        zzActivityModel(modelGuard)->indexFor(currentSideId(leftPaneGuard)));
    if (!stable()) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Activity current state was interrupted"), id.value());
    }
    rightBarGuard->setCurrentSourceIndex(
        zzActivityModel(modelGuard)->indexFor(currentSideId(rightPaneGuard)));
    if (!stable()) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Activity current state was interrupted"), id.value());
    }
    leftBarGuard->setActiveSourceIndexes(indexesFor(leftPaneGuard));
    if (!stable()) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Activity active state was interrupted"), id.value());
    }
    rightBarGuard->setActiveSourceIndexes(indexesFor(rightPaneGuard));
    if (!stable()) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Activity active state was interrupted"), id.value());
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::setPanelBadge(
    const ZzWorkspacePanelId &id,
    int value)
{
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
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
    ZzWorkspaceLayoutTransactionPrivate transaction(*const_cast<
        ZzWorkspaceShellPrivate *>(this));
    return transaction.save();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::restoreLayout(
    const QByteArray &state)
{
    ZzWorkspaceLayoutTransactionPrivate transaction(*this);
    return transaction.restore(state);
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
        ZzFluentUI::ZzDockPanel *const dock = record.dock.data();
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
        static_cast<void>(QMetaObject::invokeMethod(
            q_ptr,
            [this] { syncSideEdgeVisibility(); },
            Qt::QueuedConnection));
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
        const QPointer<ZzFluentUI::ZzPanelStack> stackGuard = pane != nullptr
            ? pane->panelStack() : nullptr;
        if (pane != nullptr && stackGuard != nullptr
            && record.content != nullptr
            && stackGuard->panels().contains(record.content.data())) {
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

void ZzWorkspaceShellPrivate::cleanupPendingDockPanelForDestruction(
    ZzPanelRecord expected)
{
    while (true) {
        const int panelIndex = stablePanelIndex(expected);
        if (panelIndex < 0 || !panels.at(panelIndex).removalInProgress) {
            return;
        }
        ZzFluentUI::ZzDockPanel *const dock =
            panels.at(panelIndex).dock.data();
        QWidget *const content = dock != nullptr ? dock->widget() : nullptr;
        if (dock == nullptr || dock != expected.dockIdentity
            || (content != nullptr && content->parentWidget() != dock)) {
            return;
        }
        if (cleanupDockPanel(dock)) {
            return;
        }
    }
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
    if (transactionKind != ZzTransactionKind::None
        || !sourceIndex.isValid() || sourceIndex.model() != activityModel) {
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

void ZzWorkspaceShellPrivate::moveSidePanel(
    const QModelIndex &sourceIndex,
    ZzFluentUI::ZzActivityArea targetArea,
    int targetRow)
{
    if (transactionKind != ZzTransactionKind::None) {
        return;
    }
    ZzWorkspaceActivityMoveTransactionPrivate transaction(*this);
    static_cast<void>(transaction.execute(sourceIndex, targetArea, targetRow));
}

QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry>
ZzWorkspaceShellPrivate::activityRows() const
{
    return activityModel != nullptr
        ? zzActivityModel(activityModel)->placements()
        : QVector<ZzSideLayoutEntry>{};
}

bool ZzWorkspaceShellPrivate::replaceActivityRows(
    const QVector<ZzSideLayoutEntry> &rows)
{
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    if (modelGuard == nullptr) {
        return false;
    }
    const bool replaced = zzActivityModel(modelGuard)->replaceRows(rows);
    return replaced && modelGuard != nullptr && activityModel == modelGuard;
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

} // namespace ZzPureTools
