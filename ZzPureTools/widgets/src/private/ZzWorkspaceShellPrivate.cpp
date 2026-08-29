#include "ZzWorkspaceShellPrivate.h"

#include <algorithm>
#include <exception>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtGui/QAction>
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
#include "ZzWorkspaceNavigationIntegrationTransactionPrivate.h"

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

/** @brief 按 Activity 逻辑顺序计算 Ready 内容在所属物理栈中的固定位置。 */
[[nodiscard]] int zzSideRegistrationTargetIndex(
    const QVector<ZzWorkspaceShellPrivate::ZzPanelRecord> &panels,
    const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> &rows,
    const ZzWorkspacePanelId &id,
    ZzFluentUI::ZzActivityArea area) noexcept
{
    const bool left = zzIsLeftArea(area);
    int readyPredecessors = 0;
    for (const ZzWorkspaceShellPrivate::ZzPanelRecord &record : panels) {
        const bool hasLogicalRow = std::any_of(
            rows.cbegin(), rows.cend(), [&record](const auto &row) {
                return row.id == record.id;
            });
        if (!hasLogicalRow
            && record.kind == ZzWorkspaceShellPrivate::ZzPanelKind::Side
            && record.materialization
                == ZzWorkspaceShellPrivate::ZzMaterializationState::Ready
            && record.contentIdentity != nullptr && record.content != nullptr
            && record.content.data() == record.contentIdentity
            && !record.removalInProgress
            && zzIsLeftArea(record.activityArea) == left
            && (!zzIsPrimaryArea(area)
                || zzIsPrimaryArea(record.activityArea))) {
            ++readyPredecessors;
        }
    }
    for (const bool primary : {true, false}) {
        for (const ZzWorkspaceShellPrivate::ZzSideLayoutEntry &row : rows) {
            if (zzIsLeftArea(row.area) != left
                || zzIsPrimaryArea(row.area) != primary) {
                continue;
            }
            if (row.id == id) {
                return readyPredecessors;
            }
            const auto record = std::find_if(
                panels.cbegin(), panels.cend(), [&row](const auto &candidate) {
                    return candidate.id == row.id
                        && candidate.kind
                            == ZzWorkspaceShellPrivate::ZzPanelKind::Side
                        && candidate.materialization
                            == ZzWorkspaceShellPrivate::
                                ZzMaterializationState::Ready
                        && candidate.contentIdentity != nullptr
                        && candidate.content != nullptr
                        && candidate.content.data()
                            == candidate.contentIdentity
                        && !candidate.removalInProgress;
                });
            if (record != panels.cend()) {
                ++readyPredecessors;
            }
        }
    }
    return -1;
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

/** @brief 仅在 Dock 派生对象仍完整存活且身份未变化时返回面板。 */
[[nodiscard]] ZzFluentUI::ZzDockPanel *zzLiveDockPanel(
    const ZzWorkspaceShellPrivate::ZzPanelRecord &record) noexcept
{
    auto *const dock = qobject_cast<ZzFluentUI::ZzDockPanel *>(
        record.dock.data());
    return dock == record.dockIdentity ? dock : nullptr;
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

/** @brief 判断图标描述是否包含可渲染的 SVG 标识或字体字形。 */
[[nodiscard]] bool zzHasIconDescriptor(
    const ZzFluentUI::ZzIconDescriptor &descriptor) noexcept
{
    switch (descriptor.source) {
    case ZzFluentUI::ZzIconSource::SvgResource:
        return !descriptor.resourceId.trimmed().isEmpty();
    case ZzFluentUI::ZzIconSource::FontGlyph:
        return descriptor.fontIcon != ZzFluentUI::ZzFontIcon::None;
    }
    return false;
}

struct ZzActivityRow final
{
    ZzWorkspacePanelId id;
    QString title;
    ZzFluentUI::ZzIconDescriptor icon;
    ZzFluentUI::ZzActivityArea area =
        ZzFluentUI::ZzActivityArea::LeftPrimary;
    int badge = 0;
    ZzWorkspaceShellPrivate::ZzActivityRowKind kind =
        ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel;
    ZzWorkspaceActivityId activityId;
    QPointer<QAction> action;
};

/** @brief 保存 Side Panel 与 FixedAction，并投影 Activity Bar 通用角色。 */
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
        if (!index.isValid() || index.row() < 0
            || index.row() >= static_cast<int>(rows_.size())) {
            return Qt::NoItemFlags;
        }
        const ZzActivityRow &row = rows_.at(index.row());
        if (row.kind == ZzWorkspaceShellPrivate::ZzActivityRowKind::FixedAction) {
            return row.action != nullptr && row.action->isEnabled()
                ? Qt::ItemFlags(Qt::ItemIsEnabled) : Qt::NoItemFlags;
        }
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable
            | Qt::ItemIsDragEnabled;
    }

    void append(ZzActivityRow row)
    {
        int rowIndex = static_cast<int>(rows_.size());
        if (row.kind == ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel) {
            const auto firstFixed = std::find_if(
                rows_.cbegin(), rows_.cend(), [](const ZzActivityRow &candidate) {
                    return candidate.kind
                        == ZzWorkspaceShellPrivate::ZzActivityRowKind::FixedAction;
                });
            rowIndex = static_cast<int>(std::distance(rows_.cbegin(), firstFixed));
        }
        beginStructuralChange();
        beginInsertRows({}, rowIndex, rowIndex);
        rows_.insert(rowIndex, std::move(row));
        endInsertRows();
        endStructuralChange();
    }

    [[nodiscard]] bool insert(int rowIndex, ZzActivityRow row)
    {
        if (indexOf(row.id) >= 0) {
            return false;
        }
        const auto firstFixed = std::find_if(
            rows_.cbegin(), rows_.cend(), [](const ZzActivityRow &candidate) {
                return candidate.kind
                    == ZzWorkspaceShellPrivate::ZzActivityRowKind::FixedAction;
            });
        const int sideCount = static_cast<int>(
            std::distance(rows_.cbegin(), firstFixed));
        rowIndex = row.kind
            == ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel
            ? std::clamp(rowIndex, 0, sideCount)
            : static_cast<int>(rows_.size());
        beginStructuralChange();
        beginInsertRows({}, rowIndex, rowIndex);
        rows_.insert(rowIndex, std::move(row));
        endInsertRows();
        endStructuralChange();
        return true;
    }

    [[nodiscard]] bool remove(const ZzWorkspacePanelId &id)
    {
        const int row = indexOf(id);
        if (row < 0) {
            return false;
        }
        beginStructuralChange();
        beginRemoveRows({}, row, row);
        rows_.removeAt(row);
        endRemoveRows();
        endStructuralChange();
        return true;
    }

    [[nodiscard]] bool remove(const ZzWorkspaceActivityId &id)
    {
        const int row = indexOf(id);
        if (row < 0) {
            return false;
        }
        beginStructuralChange();
        beginRemoveRows({}, row, row);
        rows_.removeAt(row);
        endRemoveRows();
        endStructuralChange();
        return true;
    }

    void notifyActionChanged(const ZzWorkspaceActivityId &id)
    {
        const int row = indexOf(id);
        if (row >= 0) {
            Q_EMIT dataChanged(index(row, 0), index(row, 0), {});
        }
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

    [[nodiscard]] bool setArea(
        const ZzWorkspacePanelId &id,
        ZzFluentUI::ZzActivityArea area)
    {
        const int row = indexOf(id);
        if (row < 0) {
            return false;
        }
        if (rows_[row].area == area) {
            return true;
        }
        rows_[row].area = area;
        Q_EMIT dataChanged(
            index(row, 0), index(row, 0),
            {static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area)});
        return true;
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
            if (row.kind
                != ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel) {
                continue;
            }
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
        for (const ZzActivityRow &row : std::as_const(rows_)) {
            if (row.kind
                == ZzWorkspaceShellPrivate::ZzActivityRowKind::FixedAction) {
                reordered.append(row);
            }
        }
        beginStructuralChange();
        beginResetModel();
        rows_ = std::move(reordered);
        endResetModel();
        endStructuralChange();
    }

    [[nodiscard]] QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry>
    placements() const
    {
        QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> result;
        result.reserve(rows_.size());
        int sideOrder = 0;
        for (qsizetype index = 0; index < rows_.size(); ++index) {
            const ZzActivityRow &row = rows_.at(index);
            if (row.kind
                == ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel) {
                result.append({row.id, row.area, sideOrder++});
            }
        }
        return result;
    }

    [[nodiscard]] bool replaceRows(
        const QVector<ZzWorkspaceShellPrivate::ZzSideLayoutEntry> &rows)
    {
        const qsizetype sideCount = std::count_if(
            rows_.cbegin(), rows_.cend(), [](const ZzActivityRow &row) {
                return row.kind
                    == ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel;
            });
        if (rows.size() != sideCount) {
            return false;
        }
        QHash<ZzWorkspacePanelId, int> sourceRows;
        sourceRows.reserve(sideCount);
        for (qsizetype index = 0; index < rows_.size(); ++index) {
            if (rows_.at(index).kind
                != ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel) {
                continue;
            }
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
        for (const ZzActivityRow &item : std::as_const(rows_)) {
            if (item.kind
                == ZzWorkspaceShellPrivate::ZzActivityRowKind::FixedAction) {
                replacement.append(item);
            }
        }
        beginStructuralChange();
        beginResetModel();
        rows_ = std::move(replacement);
        endResetModel();
        endStructuralChange();
        return true;
    }

    [[nodiscard]] ZzWorkspacePanelId idAt(int row) const
    {
        return row >= 0 && row < static_cast<int>(rows_.size())
            ? rows_.at(row).id : ZzWorkspacePanelId{};
    }

    [[nodiscard]] const ZzActivityRow *rowAt(int row) const noexcept
    {
        return row >= 0 && row < static_cast<int>(rows_.size())
            ? &rows_.at(row) : nullptr;
    }

    [[nodiscard]] QModelIndex indexFor(
        const ZzWorkspacePanelId &id) const
    {
        const int row = indexOf(id);
        return row >= 0 ? index(row, 0) : QModelIndex{};
    }

    [[nodiscard]] QModelIndex indexFor(
        const ZzWorkspaceActivityId &id) const
    {
        const int row = indexOf(id);
        return row >= 0 ? index(row, 0) : QModelIndex{};
    }

    [[nodiscard]] bool hasRowsForSide(bool left) const noexcept
    {
        return std::any_of(
            rows_.cbegin(), rows_.cend(), [left](const ZzActivityRow &row) {
                return zzIsLeftArea(row.area) == left;
            });
    }

    /** @brief 返回模型是否正在发送结构变更信号。 */
    [[nodiscard]] bool isStructuralChangeInProgress() const noexcept
    {
        return structuralChangeDepth_ > 0;
    }

    /** @brief 脱离工作区对象树，并在当前结构变更完整结束后销毁。 */
    void detachAndDeleteWhenStructurallyIdle()
    {
        setParent(nullptr);
        deleteWhenStructurallyIdle_ = true;
        if (!isStructuralChangeInProgress()) {
            deleteLater();
        }
    }

private:
    /** @brief 标记 begin/end 结构信号序列开始。 */
    void beginStructuralChange() noexcept
    {
        ++structuralChangeDepth_;
    }

    /** @brief 标记结构信号序列结束，并兑现析构期延迟销毁。 */
    void endStructuralChange()
    {
        Q_ASSERT(structuralChangeDepth_ > 0);
        --structuralChangeDepth_;
        if (structuralChangeDepth_ == 0 && deleteWhenStructurallyIdle_) {
            deleteLater();
        }
    }

    [[nodiscard]] int indexOf(const ZzWorkspacePanelId &id) const noexcept
    {
        for (qsizetype row = 0; row < rows_.size(); ++row) {
            if (rows_.at(row).kind
                    == ZzWorkspaceShellPrivate::ZzActivityRowKind::SidePanel
                && rows_.at(row).id == id) {
                return static_cast<int>(row);
            }
        }
        return -1;
    }

    [[nodiscard]] int indexOf(const ZzWorkspaceActivityId &id) const noexcept
    {
        for (qsizetype row = 0; row < rows_.size(); ++row) {
            if (rows_.at(row).kind
                    == ZzWorkspaceShellPrivate::ZzActivityRowKind::FixedAction
                && rows_.at(row).activityId == id) {
                return static_cast<int>(row);
            }
        }
        return -1;
    }

    QVector<ZzActivityRow> rows_;
    int structuralChangeDepth_ = 0;
    bool deleteWhenStructurallyIdle_ = false;
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
    , hostObject(hostWindow)
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
    leftSidePane->setMode(ZzFluentUI::ZzSidePaneMode::Single);
    centerHost = new QWidget(workspaceRoot);
    centerHost->setObjectName(QStringLiteral("zzWorkspaceCenterHost"));
    splitWorkspace = new ZzFluentUI::ZzSplitWorkspace(centerHost);
    splitWorkspace->setObjectName(QStringLiteral("zzWorkspaceSplitWorkspace"));
    bottomPane = new ZzFluentUI::ZzBottomPane(centerHost);
    bottomPane->setObjectName(QStringLiteral("zzWorkspaceBottomPane"));
    bottomPane->setCollapsed(true);
    rightSidePane = new ZzFluentUI::ZzSidePane(
        ZzFluentUI::ZzSidePaneEdge::Right, workspaceRoot);
    rightSidePane->setMode(ZzFluentUI::ZzSidePaneMode::Single);
    rightActivityBar = new ZzFluentUI::ZzActivityBar(
        ZzFluentUI::ZzSidePaneEdge::Right, workspaceRoot);
    leftActivityBar->setMultiActiveEnabled(false);
    rightActivityBar->setMultiActiveEnabled(false);
    palette = new ZzFluentUI::ZzCommandPalette(workspaceRoot);
    activityModel = new ZzWorkspaceActivityModel(q_ptr);
    leftActivityBar->setModel(activityModel);
    rightActivityBar->setModel(activityModel);

    QObject::connect(
        leftSidePane, &ZzFluentUI::ZzSidePane::collapsedChanged,
        q_ptr, [bar = leftActivityBar](bool collapsed) {
            if (bar != nullptr) {
                bar->setSelectionVisible(!collapsed);
            }
        });
    QObject::connect(
        rightSidePane, &ZzFluentUI::ZzSidePane::collapsedChanged,
        q_ptr, [bar = rightActivityBar](bool collapsed) {
            if (bar != nullptr) {
                bar->setSelectionVisible(!collapsed);
            }
        });

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
            activateActivity(index, false);
        });
    QObject::connect(
        rightActivityBar, &ZzFluentUI::ZzActivityBar::activationRequested,
        q_ptr, [this](const QModelIndex &index) {
            activateActivity(index, false);
        });
    QObject::connect(
        leftActivityBar, &ZzFluentUI::ZzActivityBar::collapseRequested,
        q_ptr, [this](const QModelIndex &index) {
            activateActivity(index, true);
        });
    QObject::connect(
        rightActivityBar, &ZzFluentUI::ZzActivityBar::collapseRequested,
        q_ptr, [this](const QModelIndex &index) {
            activateActivity(index, true);
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
    QObject::disconnect(navigationTabPinnedConnection);
    QObject::disconnect(navigationTabCloseConnection);
    for (ZzPanelRecord &record : panels) {
        QObject::disconnect(record.contentDestroyedConnection);
    }
    for (ZzFixedActivityRecord &record : fixedActivities) {
        QObject::disconnect(record.destroyedConnection);
        QObject::disconnect(record.changedConnection);
    }
    fixedActivities.clear();
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
            ZzFluentUI::ZzDockPanel *const dock = zzLiveDockPanel(record);
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
    if (activityModel != nullptr) {
        auto *const model = zzActivityModel(activityModel);
        if (model->isStructuralChangeInProgress()) {
            model->detachAndDeleteWhenStructurallyIdle();
        }
    }
    const bool hostOwnsWorkspaceRoot = applicationNavigationIntegrated
        && hostObject != nullptr
        && workspaceRoot != nullptr
        && workspaceRoot->parent() == hostObject;
    if (workspaceRoot != nullptr && !hostOwnsWorkspaceRoot) {
        if (sideEdgeVisibilitySyncDepth > 0) {
            workspaceRoot->deleteLater();
        } else {
            delete workspaceRoot;
        }
    }
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::registerSidePanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    QWidget *content,
    bool withinNavigationIntegration)
{
    if (transactionKind != ZzTransactionKind::None
        && !(withinNavigationIntegration
            && transactionKind == ZzTransactionKind::NavigationIntegration)) {
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
    if (hasRegisteredStableId(id.value())) {
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

    if (pane->panelStack() == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel rejected content"), id.value());
    }

    ZzPanelRecord record;
    record.id = id;
    record.title = normalizedTitle;
    record.icon = std::move(icon);
    record.kind = ZzPanelKind::Side;
    record.activityArea = area;
    record.materialization = ZzMaterializationState::Ready;
    record.registrationGeneration = ++nextPanelRegistrationGeneration;
    record.registrationInProgress = true;
    panels.append(std::move(record));
    const ZzPanelRecord expectedRecord = panels.constLast();
    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    auto adopted = adoptSidePanelContent(
        id, expectedRecord.registrationGeneration, content, true);
    if (shellGuard == nullptr) {
        return adopted ? zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace was destroyed during panel registration"),
            id.value()) : adopted;
    }
    if (!adopted) {
        const int panelIndex = stablePanelIndex(expectedRecord);
        if (panelIndex >= 0) {
            panels.removeAt(panelIndex);
        }
        if (activityModel != nullptr) {
            static_cast<void>(zzActivityModel(activityModel)->remove(id));
        }
        syncSideEdgeVisibility();
        return adopted;
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void>
ZzWorkspaceShellPrivate::registerSidePanelFactory(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    ZzWorkspacePanelFactory factory)
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
    if (!id.isValid() || normalizedTitle.isEmpty() || !factory
        || !zzIsSideArea(area)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Invalid deferred side panel registration"),
            id.value());
    }
    if (hasRegisteredStableId(id.value())) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace panel id is already registered"),
            id.value());
    }
    if (activityModel == nullptr || leftActivityBar == nullptr
        || rightActivityBar == nullptr || leftSidePane == nullptr
        || rightSidePane == nullptr || leftSidePane->panelStack() == nullptr
        || rightSidePane->panelStack() == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Deferred side panel registration is unavailable"),
            id.value());
    }

    const QPointer<QMainWindow> hostGuard(host);
    const QPointer<QWidget> rootGuard(workspaceRoot);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    const QPointer<ZzFluentUI::ZzActivityBar> leftBarGuard(leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBarGuard(rightActivityBar);
    const QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(leftSidePane);
    const QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(rightSidePane);
    const QModelIndex leftCurrentBefore =
        leftBarGuard->currentSourceIndex();
    const QModelIndex rightCurrentBefore =
        rightBarGuard->currentSourceIndex();
    const QList<QModelIndex> leftActiveBefore =
        leftBarGuard->activeSourceIndexes();
    const QList<QModelIndex> rightActiveBefore =
        rightBarGuard->activeSourceIndexes();
    const QList<QWidget *> leftPanelsBefore =
        leftPaneGuard->panelStack()->panels();
    const QList<QWidget *> rightPanelsBefore =
        rightPaneGuard->panelStack()->panels();
    const QList<QWidget *> leftVisibleBefore = leftPaneGuard->visibleWidgets();
    const QList<QWidget *> rightVisibleBefore = rightPaneGuard->visibleWidgets();
    const QWidget *const leftPaneCurrentBefore = leftPaneGuard->currentWidget();
    const QWidget *const rightPaneCurrentBefore = rightPaneGuard->currentWidget();
    const bool leftCollapsedBefore = leftPaneGuard->isCollapsed();
    const bool rightCollapsedBefore = rightPaneGuard->isCollapsed();
    const QVector<ZzSideLayoutEntry> rowsBefore = activityRows();
    const auto sameRows = [](const QVector<ZzSideLayoutEntry> &left,
                             const QVector<ZzSideLayoutEntry> &right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (qsizetype index = 0; index < left.size(); ++index) {
            if (left.at(index).id != right.at(index).id
                || left.at(index).area != right.at(index).area
                || left.at(index).order != right.at(index).order) {
                return false;
            }
        }
        return true;
    };

    ZzPanelRecord record;
    record.id = id;
    record.title = normalizedTitle;
    record.icon = icon;
    record.kind = ZzPanelKind::Side;
    record.activityArea = area;
    record.factory = std::move(factory);
    record.materialization = ZzMaterializationState::Pending;
    record.registrationGeneration = ++nextPanelRegistrationGeneration;
    record.registrationInProgress = true;
    panels.append(std::move(record));
    const ZzPanelRecord expectedRecord = panels.constLast();

    const auto stableInfrastructure = [this, &hostGuard, &rootGuard,
                                       &modelGuard, &leftBarGuard,
                                       &rightBarGuard, &leftPaneGuard,
                                       &rightPaneGuard] {
        return hostGuard != nullptr && hostGuard == host
            && rootGuard != nullptr && rootGuard == workspaceRoot
            && modelGuard != nullptr && modelGuard == activityModel
            && leftBarGuard != nullptr && leftBarGuard == leftActivityBar
            && rightBarGuard != nullptr && rightBarGuard == rightActivityBar
            && leftPaneGuard != nullptr && leftPaneGuard == leftSidePane
            && rightPaneGuard != nullptr && rightPaneGuard == rightSidePane
            && leftPaneGuard->panelStack() != nullptr
            && rightPaneGuard->panelStack() != nullptr;
    };
    const auto stateUnchanged = [&] {
        return stableInfrastructure()
            && leftBarGuard->currentSourceIndex() == leftCurrentBefore
            && rightBarGuard->currentSourceIndex() == rightCurrentBefore
            && leftBarGuard->activeSourceIndexes() == leftActiveBefore
            && rightBarGuard->activeSourceIndexes() == rightActiveBefore
            && leftPaneGuard->panelStack()->panels() == leftPanelsBefore
            && rightPaneGuard->panelStack()->panels() == rightPanelsBefore
            && leftPaneGuard->visibleWidgets() == leftVisibleBefore
            && rightPaneGuard->visibleWidgets() == rightVisibleBefore
            && leftPaneGuard->currentWidget() == leftPaneCurrentBefore
            && rightPaneGuard->currentWidget() == rightPaneCurrentBefore
            && leftPaneGuard->isCollapsed() == leftCollapsedBefore
            && rightPaneGuard->isCollapsed() == rightCollapsedBefore;
    };
    const auto reject = [this, &id, &modelGuard, &expectedRecord] {
        if (modelGuard != nullptr && modelGuard == activityModel) {
            static_cast<void>(zzActivityModel(modelGuard)->remove(id));
        }
        const int panelIndex = stablePanelIndex(expectedRecord);
        if (panelIndex >= 0) {
            panels.removeAt(panelIndex);
        }
        syncSideEdgeVisibility();
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Deferred side panel registration was interrupted"),
            id.value());
    };

    zzActivityModel(modelGuard)->append(
        ZzActivityRow{id, normalizedTitle, std::move(icon), area, 0,
            ZzActivityRowKind::SidePanel, {}, {}});
    QVector<ZzSideLayoutEntry> rowsAfter = rowsBefore;
    rowsAfter.append({id, area, static_cast<int>(rowsAfter.size())});
    const int panelIndex = stablePanelIndex(expectedRecord);
    if (!stateUnchanged() || panelIndex < 0
        || !sameRows(activityRows(), rowsAfter)
        || panels.at(panelIndex).materialization
            != ZzMaterializationState::Pending
        || !panels.at(panelIndex).factory) {
        return reject();
    }
    syncSideEdgeVisibility();
    const int synchronizedIndex = stablePanelIndex(expectedRecord);
    if (!stateUnchanged() || synchronizedIndex < 0
        || !panels.at(synchronizedIndex).registrationInProgress
        || !sameRows(activityRows(), rowsAfter)) {
        return reject();
    }
    panels[synchronizedIndex].registrationInProgress = false;
    const int finalIndex = stablePanelIndex(expectedRecord);
    if (!stateUnchanged() || finalIndex < 0
        || panels.at(finalIndex).registrationInProgress
        || !sameRows(activityRows(), rowsAfter)) {
        return reject();
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void>
ZzWorkspaceShellPrivate::registerFixedActivityAction(
    const ZzWorkspaceActivityId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    QAction *action)
{
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
    const QString normalizedTitle = title.trimmed();
    if (host == nullptr || workspaceRoot == nullptr || activityModel == nullptr
        || leftActivityBar == nullptr || rightActivityBar == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"), id.value());
    }
    if (!id.isValid() || normalizedTitle.isEmpty() || action == nullptr
        || !zzIsSideArea(area) || !zzHasIconDescriptor(icon)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Invalid fixed activity registration"), id.value());
    }
    if (!zzIsCurrentThread(action)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Fixed activity action must use the GUI thread"),
            id.value());
    }
    if (hasRegisteredStableId(id.value())) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Workspace activity id is already registered"),
            id.value());
    }

    ZzFixedActivityRecord record;
    record.id = id;
    record.action = action;
    record.actionIdentity = action;
    record.registrationInProgress = true;
    record.destroyedConnection = QObject::connect(
        action, &QObject::destroyed, q_ptr,
        [this, id, action](QObject *) {
            handleFixedActivityActionDestroyed(id, action);
        });
    record.changedConnection = QObject::connect(
        action, &QAction::changed, q_ptr,
        [this, id, action] {
            handleFixedActivityActionChanged(id, action);
        });
    fixedActivities.append(record);

    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    const QPointer<QMainWindow> hostGuard(host);
    const QPointer<QWidget> rootGuard(workspaceRoot);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    const QPointer<ZzFluentUI::ZzActivityBar> leftBarGuard(leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBarGuard(rightActivityBar);
    const QPointer<QAction> actionGuard(action);
    const auto interrupted = [&id] {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Fixed activity registration was interrupted"),
            id.value());
    };
    zzActivityModel(modelGuard)->append(ZzActivityRow{
        {}, normalizedTitle, std::move(icon), area, 0,
        ZzActivityRowKind::FixedAction, id, actionGuard});

    if (shellGuard == nullptr) {
        if (modelGuard != nullptr) {
            static_cast<void>(zzActivityModel(modelGuard)->remove(id));
        }
        return interrupted();
    }
    const auto infrastructureIntact = [&] {
        return hostGuard != nullptr && hostGuard == host
            && rootGuard != nullptr && rootGuard == workspaceRoot
            && modelGuard != nullptr && modelGuard == activityModel
            && leftBarGuard != nullptr && leftBarGuard == leftActivityBar
            && rightBarGuard != nullptr && rightBarGuard == rightActivityBar;
    };
    const auto registrationIntact = [&] {
        if (!infrastructureIntact() || actionGuard == nullptr) {
            return false;
        }
        const int currentRecord = fixedActivityIndex(id);
        return currentRecord >= 0
            && fixedActivities.at(currentRecord).actionIdentity == action
            && fixedActivities.at(currentRecord).action == actionGuard
            && zzActivityModel(modelGuard)->indexFor(id).isValid();
    };
    const auto rollback = [&] {
        const int staleRecord = fixedActivityIndex(id);
        if (staleRecord >= 0) {
            QObject::disconnect(fixedActivities[staleRecord].destroyedConnection);
            QObject::disconnect(fixedActivities[staleRecord].changedConnection);
            fixedActivities.removeAt(staleRecord);
        }
        if (modelGuard != nullptr && modelGuard == activityModel) {
            static_cast<void>(zzActivityModel(modelGuard)->remove(id));
        }
        if (shellGuard != nullptr) {
            syncSideEdgeVisibility();
        }
    };
    if (!registrationIntact()) {
        rollback();
        return interrupted();
    }

    const int recordIndex = fixedActivityIndex(id);
    const bool publishPendingActionChange =
        fixedActivities.at(recordIndex).actionChangePending;
    fixedActivities[recordIndex].registrationInProgress = false;
    fixedActivities[recordIndex].actionChangePending = false;
    if (publishPendingActionChange) {
        zzActivityModel(modelGuard)->notifyActionChanged(id);
    }
    if (shellGuard == nullptr) {
        return interrupted();
    }
    if (!registrationIntact()) {
        rollback();
        return interrupted();
    }

    syncSideEdgeVisibility();
    if (shellGuard == nullptr) {
        return interrupted();
    }
    if (!registrationIntact()) {
        rollback();
        return interrupted();
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::adoptSidePanelContent(
    const ZzWorkspacePanelId &id,
    std::uint64_t registrationGeneration,
    QWidget *content,
    bool activate)
{
    int panelIndex = indexOf(id);
    if (panelIndex < 0 || content == nullptr
        || !zzIsCurrentThread(content) || content->parent() != nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Deferred side panel returned invalid content"),
            id.value());
    }
    const ZzPanelRecord before = panels.at(panelIndex);
    if (before.kind != ZzPanelKind::Side
        || before.registrationGeneration != registrationGeneration
        || before.removalInProgress
        || (before.materialization != ZzMaterializationState::Materializing
            && !before.registrationInProgress)) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel materialization state changed"),
            id.value());
    }
    ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(before.activityArea)
        ? leftSidePane.data() : rightSidePane.data();
    if (host == nullptr || workspaceRoot == nullptr || pane == nullptr
        || pane->panelStack() == nullptr || activityModel == nullptr
        || leftActivityBar == nullptr || rightActivityBar == nullptr) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel container is unavailable"), id.value());
    }

    const QModelIndex existingSourceIndex =
        zzActivityModel(activityModel)->indexFor(id);
    if (existingSourceIndex.isValid() == activate) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel logical registration is inconsistent"),
            id.value());
    }

    const QPointer<QMainWindow> hostGuard(host);
    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    const QPointer<QWidget> rootGuard(workspaceRoot);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    const QPointer<ZzFluentUI::ZzActivityBar> leftBarGuard(leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBarGuard(rightActivityBar);
    const QPointer<ZzFluentUI::ZzSidePane> paneGuard(pane);
    const QPointer<ZzFluentUI::ZzPanelStack> stackGuard(pane->panelStack());
    const QPointer<QWidget> contentGuard(content);
    const QVector<ZzSideLayoutEntry> rowsBefore = activityRows();
    const QList<QWidget *> panelsBefore = stackGuard->panels();
    const QList<QWidget *> visibleBefore = paneGuard->visibleWidgets();
    const QList<int> sizesBefore = stackGuard->panelSizes();
    const QPointer<QWidget> currentBefore(paneGuard->currentWidget());
    const bool collapsedBefore = paneGuard->isCollapsed();
    const QModelIndex leftCurrentBefore =
        leftBarGuard->currentSourceIndex();
    const QModelIndex rightCurrentBefore =
        rightBarGuard->currentSourceIndex();
    const QList<QModelIndex> leftActiveBefore =
        leftBarGuard->activeSourceIndexes();
    const QList<QModelIndex> rightActiveBefore =
        rightBarGuard->activeSourceIndexes();
    const ZzWorkspacePanelId leftCurrentPanelBefore = leftCurrentPanel;
    const ZzWorkspacePanelId rightCurrentPanelBefore = rightCurrentPanel;
    const bool leftPaneExpandedBefore = leftPaneExpanded;
    const bool rightPaneExpandedBefore = rightPaneExpanded;
    const auto interrupted = [&] {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace was destroyed during side panel adoption"),
            id.value());
    };
    QList<QPointer<QWidget>> appendOrder;
    appendOrder.reserve(panelsBefore.size() + 1);
    for (QWidget *const panel : panelsBefore) {
        appendOrder.append(panel);
    }
    appendOrder.append(contentGuard);
    QList<QPointer<QWidget>> canonicalOrder = appendOrder;
    canonicalOrder.removeLast();
    QVector<ZzSideLayoutEntry> rowsAfter = rowsBefore;
    if (activate) {
        rowsAfter.append(
            {id, before.activityArea, static_cast<int>(rowsAfter.size())});
    }
    const int targetStackIndex = zzSideRegistrationTargetIndex(
        panels, rowsAfter, id, before.activityArea);
    if (targetStackIndex < 0 || targetStackIndex > panelsBefore.size()) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel insertion position is invalid"),
            id.value());
    }
    canonicalOrder.insert(targetStackIndex, contentGuard);

    panels[panelIndex].content = content;
    panels[panelIndex].contentIdentity = content;
    panels[panelIndex].registrationInProgress = true;
    connectPanelContentDestroyed(id, content);
    const ZzPanelRecord expected = panels.at(panelIndex);
    ZzPanelOwnerObserver ownerObserver(contentGuard, stackGuard);
    QPointer<QWidget> contentOwnerGuard;
    QWidget *contentOwnerIdentity = nullptr;
    bool activityAppended = false;
    const auto sameRows = [](const QVector<ZzSideLayoutEntry> &actual,
                             const QVector<ZzSideLayoutEntry> &expectedRows) {
        if (actual.size() != expectedRows.size()) {
            return false;
        }
        for (qsizetype index = 0; index < actual.size(); ++index) {
            if (actual.at(index).id != expectedRows.at(index).id
                || actual.at(index).area != expectedRows.at(index).area
                || actual.at(index).order != expectedRows.at(index).order) {
                return false;
            }
        }
        return true;
    };
    const auto audit = [&](const QList<QPointer<QWidget>> &expectedOrder,
                           const QVector<ZzSideLayoutEntry> &expectedRows,
                           bool registrationInProgress) {
        if (shellGuard == nullptr) {
            return false;
        }
        const int currentIndex = stablePanelIndex(expected);
        if (hostGuard == nullptr || hostGuard != host
            || rootGuard == nullptr || rootGuard != workspaceRoot
            || modelGuard == nullptr || modelGuard != activityModel
            || leftBarGuard == nullptr || leftBarGuard != leftActivityBar
            || rightBarGuard == nullptr || rightBarGuard != rightActivityBar
            || paneGuard == nullptr
            || paneGuard != (zzIsLeftArea(expected.activityArea)
                    ? leftSidePane : rightSidePane)
            || stackGuard == nullptr || stackGuard != paneGuard->panelStack()
            || contentGuard == nullptr || currentIndex < 0
            || !ownerObserver.hasCapturedOwner() || ownerObserver.isPolluted()
            || panels.at(currentIndex).contentIdentity != contentGuard
            || panels.at(currentIndex).content != contentGuard
            || panels.at(currentIndex).contentOwner != contentOwnerGuard
            || panels.at(currentIndex).contentOwnerIdentity
                != contentOwnerIdentity
            || panels.at(currentIndex).registrationInProgress
                != registrationInProgress
            || panels.at(currentIndex).removalInProgress) {
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
    const auto rollback = [&] {
        if (shellGuard == nullptr) {
            return;
        }
        if (paneGuard != nullptr && stackGuard != nullptr
            && contentGuard != nullptr
            && stackGuard->panels().contains(contentGuard)) {
            static_cast<void>(paneGuard->takeWidget(contentGuard));
            if (shellGuard == nullptr) {
                return;
            }
        }
        if (activityAppended && modelGuard != nullptr
            && modelGuard == activityModel) {
            static_cast<void>(zzActivityModel(modelGuard)->remove(id));
            if (shellGuard == nullptr) {
                return;
            }
        }
        if (leftBarGuard != nullptr && leftBarGuard == leftActivityBar) {
            leftBarGuard->setCurrentSourceIndex(leftCurrentBefore);
            if (shellGuard == nullptr) {
                return;
            }
            leftBarGuard->setActiveSourceIndexes(leftActiveBefore);
            if (shellGuard == nullptr) {
                return;
            }
        }
        if (rightBarGuard != nullptr && rightBarGuard == rightActivityBar) {
            rightBarGuard->setCurrentSourceIndex(rightCurrentBefore);
            if (shellGuard == nullptr) {
                return;
            }
            rightBarGuard->setActiveSourceIndexes(rightActiveBefore);
            if (shellGuard == nullptr) {
                return;
            }
        }
        leftCurrentPanel = leftCurrentPanelBefore;
        rightCurrentPanel = rightCurrentPanelBefore;
        leftPaneExpanded = leftPaneExpandedBefore;
        rightPaneExpanded = rightPaneExpandedBefore;
        if (paneGuard != nullptr && stackGuard != nullptr) {
            for (QWidget *const existing : panelsBefore) {
                if (existing != nullptr && stackGuard->panels().contains(existing)) {
                    static_cast<void>(paneGuard->setWidgetVisible(
                        existing, visibleBefore.contains(existing)));
                    if (shellGuard == nullptr) {
                        return;
                    }
                }
            }
            if (currentBefore != nullptr
                && stackGuard->panels().contains(currentBefore)) {
                static_cast<void>(paneGuard->setCurrentWidget(currentBefore));
                if (shellGuard == nullptr) {
                    return;
                }
            }
            if (!sizesBefore.isEmpty()) {
                static_cast<void>(stackGuard->setPanelSizes(sizesBefore));
                if (shellGuard == nullptr) {
                    return;
                }
            }
            paneGuard->setCollapsed(collapsedBefore);
            if (shellGuard == nullptr) {
                return;
            }
        }
        panelIndex = stablePanelIndex(expected);
        if (panelIndex >= 0) {
            QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
            panels[panelIndex].contentDestroyedConnection =
                before.contentDestroyedConnection;
            panels[panelIndex].content = before.content;
            panels[panelIndex].contentIdentity = before.contentIdentity;
            panels[panelIndex].contentOwner = before.contentOwner;
            panels[panelIndex].contentOwnerIdentity =
                before.contentOwnerIdentity;
            panels[panelIndex].registrationInProgress =
                before.registrationInProgress;
            panels[panelIndex].materialization = before.materialization;
        }
        syncSideEdgeVisibility();
    };
    const auto reject = [&](const QString &message) {
        if (shellGuard == nullptr) {
            return interrupted();
        }
        rollback();
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            message, id.value());
    };

    const bool added = paneGuard->addWidget(contentGuard, expected.title);
    if (shellGuard == nullptr) {
        return interrupted();
    }
    panelIndex = stablePanelIndex(expected);
    contentOwnerGuard = ownerObserver.owner();
    contentOwnerIdentity = ownerObserver.ownerIdentity();
    if (panelIndex >= 0) {
        panels[panelIndex].contentOwner = contentOwnerGuard;
        panels[panelIndex].contentOwnerIdentity = contentOwnerIdentity;
    }
    if (!added || !audit(appendOrder, rowsBefore, true)) {
        return reject(QStringLiteral(
            "Side panel content adoption was interrupted"));
    }
    if (activate) {
        const bool currentWidgetSet = paneGuard->setCurrentWidget(contentGuard);
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (!currentWidgetSet || !audit(appendOrder, rowsBefore, true)) {
            return reject(QStringLiteral(
                "Side panel current state was interrupted"));
        }
    }
    if (targetStackIndex != panelsBefore.size()) {
        const bool moved = stackGuard->movePanel(
            contentGuard, targetStackIndex);
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (!moved) {
            return reject(QStringLiteral(
                "Side panel content ordering was interrupted"));
        }
    }
    if (!audit(canonicalOrder, rowsBefore, true)) {
        return reject(QStringLiteral(
            "Side panel content ordering was interrupted"));
    }

    if (activate) {
        zzActivityModel(modelGuard)->append(ZzActivityRow{
            id, expected.title, expected.icon, expected.activityArea, 0,
            ZzActivityRowKind::SidePanel, {}, {}});
        activityAppended = true;
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (!audit(canonicalOrder, rowsAfter, true)) {
            return reject(QStringLiteral(
                "Side panel activity registration was interrupted"));
        }
        const QModelIndex sourceIndex = zzActivityModel(modelGuard)->indexFor(id);
        if (!sourceIndex.isValid() || sourceIndex.model() != modelGuard) {
            return reject(QStringLiteral(
                "Side panel activity registration was interrupted"));
        }
        ZzWorkspacePanelId &currentPanel = zzIsLeftArea(expected.activityArea)
            ? leftCurrentPanel : rightCurrentPanel;
        bool &paneExpanded = zzIsLeftArea(expected.activityArea)
            ? leftPaneExpanded : rightPaneExpanded;
        currentPanel = id;
        paneExpanded = true;
        ZzFluentUI::ZzActivityBar *const owningBar =
            zzIsLeftArea(expected.activityArea)
            ? leftBarGuard.data() : rightBarGuard.data();
        owningBar->setCurrentSourceIndex(sourceIndex);
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (!audit(canonicalOrder, rowsAfter, true)) {
            return reject(QStringLiteral(
                "Side panel activity state was interrupted"));
        }
        paneGuard->setCollapsed(false);
        if (shellGuard == nullptr) {
            return interrupted();
        }
        syncSideEdgeVisibility();
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (!audit(canonicalOrder, rowsAfter, true)) {
            return reject(QStringLiteral(
                "Side panel activation was interrupted"));
        }
    } else {
        const bool hidden = paneGuard->setWidgetVisible(contentGuard, false);
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (!hidden || paneGuard == nullptr || stackGuard == nullptr
            || contentGuard == nullptr) {
            return reject(QStringLiteral(
                "Side panel pending visibility was interrupted"));
        }
        if (currentBefore != nullptr) {
            const bool currentWidgetSet =
                paneGuard->setCurrentWidget(currentBefore);
            if (shellGuard == nullptr) {
                return interrupted();
            }
            if (!currentWidgetSet) {
                return reject(QStringLiteral(
                    "Side panel current state was interrupted"));
            }
        }
        if (!sizesBefore.isEmpty()) {
            const bool sizesRestored = stackGuard->setPanelSizes(sizesBefore);
            if (shellGuard == nullptr) {
                return interrupted();
            }
            if (!sizesRestored) {
                return reject(QStringLiteral(
                    "Side panel sizes changed during adoption"));
            }
        }
        paneGuard->setCollapsed(collapsedBefore);
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (paneGuard->visibleWidgets() != visibleBefore
            || paneGuard->currentWidget() != currentBefore
            || stackGuard->panelSizes() != sizesBefore
            || paneGuard->isCollapsed() != collapsedBefore) {
            return reject(QStringLiteral(
                "Side panel state changed during adoption"));
        }
    }
    panelIndex = stablePanelIndex(expected);
    if (panelIndex < 0 || contentGuard == nullptr) {
        return reject(QStringLiteral(
            "Side panel content identity changed"));
    }
    panels[panelIndex].materialization = ZzMaterializationState::Ready;
    panels[panelIndex].registrationInProgress = false;
    if (!audit(canonicalOrder, rowsAfter, false)) {
        return reject(QStringLiteral(
            "Side panel adoption commit was interrupted"));
    }
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<std::unique_ptr<QWidget>>
ZzWorkspaceShellPrivate::createPendingSidePanelContent(
    const ZzWorkspacePanelId &id)
{
    int panelIndex = indexOf(id);
    if (panelIndex < 0) {
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("Workspace panel is not registered"), id.value());
    }
    const ZzPanelRecord before = panels.at(panelIndex);
    if (before.kind != ZzPanelKind::Side) {
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Only side panels can be materialized"), id.value());
    }
    if (before.materialization != ZzMaterializationState::Pending
        || before.registrationInProgress || before.removalInProgress
        || !before.factory) {
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel materialization is in progress"),
            id.value());
    }
    if (host == nullptr || workspaceRoot == nullptr
        || activityModel == nullptr) {
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host has been destroyed"), id.value());
    }

    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    ZzWorkspacePanelFactory factory = std::move(panels[panelIndex].factory);
    panels[panelIndex].materialization = ZzMaterializationState::Materializing;
    const auto restorePending = [this, &before] {
        const int currentIndex = stablePanelIndex(before);
        if (currentIndex >= 0
            && panels.at(currentIndex).materialization
                == ZzMaterializationState::Materializing) {
            panels[currentIndex].materialization =
                ZzMaterializationState::Pending;
            panels[currentIndex].registrationInProgress = false;
        }
    };
    const auto invokeFactory = [&before, &factory]()
        -> ZzCore::ZzResult<std::unique_ptr<QWidget>> {
        try {
            return factory();
        } catch (const std::exception &exception) {
            return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Side panel factory threw an exception: %1")
                    .arg(QString::fromUtf8(exception.what())),
                before.id.value());
        } catch (...) {
            return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Side panel factory threw an unknown exception"),
                before.id.value());
        }
    };

    auto createdResult = invokeFactory();
    if (shellGuard == nullptr) {
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace shell was destroyed during side panel creation"),
            id.value());
    }
    panelIndex = stablePanelIndex(before);
    if (panelIndex < 0
        || panels.at(panelIndex).materialization
            != ZzMaterializationState::Materializing) {
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel registration changed during creation"),
            id.value());
    }
    panels[panelIndex].factory = std::move(factory);
    if (!createdResult) {
        restorePending();
        return ZzCore::ZzResult<std::unique_ptr<QWidget>>::failure(
            createdResult.error());
    }
    std::unique_ptr<QWidget> content = std::move(createdResult).value();
    if (!content) {
        restorePending();
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel factory returned null content"),
            id.value());
    }
    if (content->parent() != nullptr) {
        [[maybe_unused]] QWidget *const parentOwned = content.release();
        restorePending();
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel factory returned parented content"),
            id.value());
    }
    if (!zzIsCurrentThread(content.get())) {
        QWidget *const wrongThreadContent = content.release();
        static_cast<void>(QMetaObject::invokeMethod(
            wrongThreadContent, &QObject::deleteLater,
            Qt::QueuedConnection));
        restorePending();
        return zzWorkspaceFailure<std::unique_ptr<QWidget>>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel factory returned content from another thread"),
            id.value());
    }
    return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
        std::move(content));
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::materializeSidePanel(
    const ZzWorkspacePanelId &id)
{
    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    const int initialIndex = indexOf(id);
    if (initialIndex < 0) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("Workspace panel is not registered"), id.value());
    }
    if (panels.at(initialIndex).kind != ZzPanelKind::Side) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Only side panels can be materialized"), id.value());
    }
    if (panels.at(initialIndex).materialization
        == ZzMaterializationState::Ready) {
        return ZzCore::ZzResult<void>::success();
    }

    auto createdResult = createPendingSidePanelContent(id);
    if (!createdResult) {
        return ZzCore::ZzResult<void>::failure(createdResult.error());
    }
    std::unique_ptr<QWidget> content = std::move(createdResult).value();
    int panelIndex = indexOf(id);
    if (panelIndex < 0
        || panels.at(panelIndex).materialization
            != ZzMaterializationState::Materializing) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel registration changed during creation"),
            id.value());
    }
    const ZzPanelRecord before = panels.at(panelIndex);
    const auto restorePending = [this, &before] {
        const int currentIndex = stablePanelIndex(before);
        if (currentIndex >= 0
            && panels.at(currentIndex).materialization
                == ZzMaterializationState::Materializing) {
            panels[currentIndex].materialization =
                ZzMaterializationState::Pending;
            panels[currentIndex].registrationInProgress = false;
        }
    };
    const QPointer<QWidget> contentGuard(content.get());
    auto adopted = adoptSidePanelContent(
        id, before.registrationGeneration, content.get(), false);
    if (shellGuard == nullptr) {
        if (contentGuard == nullptr) {
            [[maybe_unused]] QWidget *const destroyedContent = content.release();
        }
        return adopted ? zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace was destroyed during panel materialization"),
            id.value()) : adopted;
    }
    if (!adopted) {
        if (contentGuard == nullptr) {
            [[maybe_unused]] QWidget *const destroyedContent = content.release();
        }
        restorePending();
        return adopted;
    }
    [[maybe_unused]] QWidget *const adoptedContent = content.release();
    panelIndex = indexOf(id);
    if (panelIndex < 0
        || panels.at(panelIndex).registrationGeneration
            != before.registrationGeneration
        || panels.at(panelIndex).materialization
            != ZzMaterializationState::Ready) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Side panel materialization commit was interrupted"),
            id.value());
    }
    panels[panelIndex].factory = {};
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::registerBottomPanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    const ZzFluentUI::ZzIconDescriptor &icon,
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
    if (hasRegisteredStableId(id.value())) {
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
    record.materialization = ZzMaterializationState::Ready;
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
    if (hasRegisteredStableId(id.value())) {
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
    record.materialization = ZzMaterializationState::Ready;
    record.registrationGeneration = ++nextPanelRegistrationGeneration;
    record.dock = dock;
    record.dockIdentity = dock;
    record.registrationInProgress = true;
    panels.append(std::move(record));
    connectPanelContentDestroyed(id, content);

    const QPointer<QObject> dockGuard(dock);
    dock->setWidget(content);
    int panelIndex = indexOf(id);
    ZzFluentUI::ZzDockPanel *registeredDock = panelIndex >= 0
        ? zzLiveDockPanel(panels.at(panelIndex)) : nullptr;
    if (dockGuard == nullptr || host == nullptr || registeredDock == nullptr
        || panels.at(panelIndex).contentIdentity != content
        || panels.at(panelIndex).content != content
        || registeredDock != dock
        || registeredDock->widget() != content) {
        rollbackPanelRegistration(id, content);
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Dock panel registration was interrupted"),
            id.value());
    }
    host->addDockWidget(area, registeredDock);
    panelIndex = indexOf(id);
    registeredDock = panelIndex >= 0
        ? zzLiveDockPanel(panels.at(panelIndex)) : nullptr;
    if (dockGuard == nullptr || host == nullptr || registeredDock == nullptr
        || panels.at(panelIndex).contentIdentity != content
        || panels.at(panelIndex).content != content
        || registeredDock != dock
        || registeredDock->widget() != content) {
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
    const ZzWorkspacePanelId &id,
    bool withinNavigationIntegration)
{
    if (transactionKind != ZzTransactionKind::None
        && !(withinNavigationIntegration
            && transactionKind == ZzTransactionKind::NavigationIntegration)) {
        return zzWorkspaceFailure<QWidget *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
    int panelIndex = indexOf(id);
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
    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    QPointer<QWidget> contentGuard(record.content);
    QWidget *content = nullptr;
    switch (record.kind) {
    case ZzPanelKind::Side: {
        if (record.materialization == ZzMaterializationState::Materializing) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Side panel materialization is in progress"),
                id.value());
        }
        if (record.materialization == ZzMaterializationState::Pending) {
            auto createdResult = createPendingSidePanelContent(id);
            if (!createdResult) {
                return ZzCore::ZzResult<QWidget *>::failure(
                    createdResult.error());
            }
            std::unique_ptr<QWidget> pendingContent =
                std::move(createdResult).value();
            const QPointer<QWidget> pendingContentGuard(pendingContent.get());
            pendingContent->hide();
            if (pendingContentGuard == nullptr
                || pendingContentGuard->parent() != nullptr
                || pendingContentGuard->isVisible()) {
                if (pendingContentGuard == nullptr
                    || pendingContentGuard->parent() != nullptr) {
                    [[maybe_unused]] QWidget *const parentOwnedContent =
                        pendingContent.release();
                }
                panelIndex = stablePanelIndex(record);
                if (panelIndex >= 0
                    && panels.at(panelIndex).materialization
                        == ZzMaterializationState::Materializing) {
                    panels[panelIndex].materialization =
                        ZzMaterializationState::Pending;
                    panels[panelIndex].registrationInProgress = false;
                }
                return zzWorkspaceFailure<QWidget *>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("Pending side panel content stayed visible"),
                    id.value());
            }
            panelIndex = stablePanelIndex(record);
            if (panelIndex < 0
                || panels.at(panelIndex).materialization
                    != ZzMaterializationState::Materializing
                || activityModel == nullptr
                || !zzActivityModel(activityModel)->remove(id)) {
                panelIndex = stablePanelIndex(record);
                if (panelIndex >= 0
                    && panels.at(panelIndex).materialization
                        == ZzMaterializationState::Materializing) {
                    panels[panelIndex].materialization =
                        ZzMaterializationState::Pending;
                    panels[panelIndex].registrationInProgress = false;
                }
                return zzWorkspaceFailure<QWidget *>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("Pending side panel removal was interrupted"),
                    id.value());
            }
            panelIndex = stablePanelIndex(record);
            if (panelIndex < 0) {
                return zzWorkspaceFailure<QWidget *>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("Pending side panel registration changed"),
                    id.value());
            }
            panels.removeAt(panelIndex);
            syncSideEdgeVisibility();
            return ZzCore::ZzResult<QWidget *>::success(
                pendingContent.release());
        }
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
        if (shellGuard == nullptr) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace was destroyed during panel removal"),
                id.value());
        }
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
            || modelGuard == nullptr || modelGuard != activityModel) {
            cleanupInterruptedPanelRemoval(
                id, record.contentIdentity, record.registrationGeneration);
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Side panel removal was interrupted"),
                id.value());
        }
        const bool activityRemoved = zzActivityModel(modelGuard)->remove(id);
        if (shellGuard == nullptr) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace was destroyed during panel removal"),
                id.value());
        }
        if (!activityRemoved || modelGuard == nullptr
            || modelGuard != activityModel) {
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
        ZzFluentUI::ZzDockPanel *const dock = zzLiveDockPanel(record);
        if (dock == nullptr || contentGuard == nullptr
            || dock->widget() != contentGuard) {
            return zzWorkspaceFailure<QWidget *>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Workspace panel content is unavailable"),
                id.value());
        }
        panels[panelIndex].removalInProgress = true;
        QObject::disconnect(panels[panelIndex].contentDestroyedConnection);
        content = dock->takeContentWidget();
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
            || zzLiveDockPanel(record) != dock
            || dock->widget() != nullptr) {
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

ZzCore::ZzResult<void>
ZzWorkspaceShellPrivate::integrateApplicationNavigation(
    const ZzWorkspacePanelId &panelId,
    const QString &panelTitle,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    const QString &centralTabTitle)
{
    return ZzWorkspaceNavigationIntegrationTransactionPrivate::execute(
        *this,
        panelId,
        panelTitle,
        std::move(icon),
        area,
        centralTabTitle);
}

ZzCore::ZzResult<void> ZzWorkspaceShellPrivate::showPanel(
    const ZzWorkspacePanelId &id,
    bool visible)
{
    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    const auto interrupted = [&] {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace shell was destroyed during panel update"),
            id.value());
    };
    if (transactionKind != ZzTransactionKind::None) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"),
            id.value());
    }
    int panelIndex = indexOf(id);
    if (panelIndex < 0) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::NotFound,
            QStringLiteral("Workspace panel is not registered"), id.value());
    }
    if (panels.at(panelIndex).kind == ZzPanelKind::Side) {
        const ZzMaterializationState state =
            panels.at(panelIndex).materialization;
        if (state == ZzMaterializationState::Materializing) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Side panel materialization is in progress"),
                id.value());
        }
        if (state == ZzMaterializationState::Pending) {
            if (!visible) {
                return ZzCore::ZzResult<void>::success();
            }
            auto materialized = materializeSidePanel(id);
            if (shellGuard == nullptr) {
                return interrupted();
            }
            if (!materialized) {
                return materialized;
            }
            panelIndex = indexOf(id);
            if (panelIndex < 0) {
                return zzWorkspaceFailure<void>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral("Side panel registration was lost"),
                    id.value());
            }
        }
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
        if (ZzFluentUI::ZzDockPanel *const dock = zzLiveDockPanel(record);
            dock == nullptr) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Dock panel has been destroyed"), id.value());
        } else {
            dock->setVisible(visible);
            if (shellGuard == nullptr) {
                return interrupted();
            }
        }
        return ZzCore::ZzResult<void>::success();
    case ZzPanelKind::Bottom: {
        const QPointer<ZzFluentUI::ZzBottomPane> paneGuard(bottomPane);
        const QPointer<QWidget> contentGuard(record.content);
        if (paneGuard == nullptr || contentGuard == nullptr) {
            return zzWorkspaceFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("Bottom panel has been destroyed"), id.value());
        }
        if (visible) {
            const bool currentWidgetSet =
                paneGuard->setCurrentWidget(contentGuard);
            if (shellGuard == nullptr) {
                return interrupted();
            }
            if (!currentWidgetSet) {
                return zzWorkspaceFailure<void>(
                    ZzCore::ZzErrorCode::InvalidState,
                    QStringLiteral(
                        "Bottom panel content is no longer registered"),
                    id.value());
            }
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
        if (shellGuard == nullptr) {
            return interrupted();
        }
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
                            &rightBarGuard, &modelGuard, &shellGuard] {
        if (shellGuard == nullptr
            || leftPaneGuard == nullptr || rightPaneGuard == nullptr
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
            if (stablePanelIndex(expected) < 0) {
                return false;
            }
            if (expected.materialization != ZzMaterializationState::Ready) {
                continue;
            }
            if (expected.content == nullptr
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
    const QVector<ZzSideLayoutEntry> rowsBefore = activityRows();
    const QList<QWidget *> leftOrderBefore = leftStackGuard->panels();
    const QList<QWidget *> rightOrderBefore = rightStackGuard->panels();
    const QList<QWidget *> leftVisibleBefore = leftPaneGuard->visibleWidgets();
    const QList<QWidget *> rightVisibleBefore = rightPaneGuard->visibleWidgets();
    const QList<int> leftSizesBefore = leftStackGuard->panelSizes();
    const QList<int> rightSizesBefore = rightStackGuard->panelSizes();
    const QPointer<QWidget> leftWidgetBefore(leftPaneGuard->currentWidget());
    const QPointer<QWidget> rightWidgetBefore(rightPaneGuard->currentWidget());
    const bool leftCollapsedBefore = leftPaneGuard->isCollapsed();
    const bool rightCollapsedBefore = rightPaneGuard->isCollapsed();
    const bool leftHiddenBefore = leftPaneGuard->isHidden();
    const bool rightHiddenBefore = rightPaneGuard->isHidden();
    const bool leftBarHiddenBefore = leftBarGuard->isHidden();
    const bool rightBarHiddenBefore = rightBarGuard->isHidden();
    const QModelIndex leftBarCurrentBefore = leftBarGuard->currentSourceIndex();
    const QModelIndex rightBarCurrentBefore = rightBarGuard->currentSourceIndex();
    const QList<QModelIndex> leftBarActiveBefore =
        leftBarGuard->activeSourceIndexes();
    const QList<QModelIndex> rightBarActiveBefore =
        rightBarGuard->activeSourceIndexes();
    const ZzWorkspacePanelId leftCurrentPanelBefore = leftCurrentPanel;
    const ZzWorkspacePanelId rightCurrentPanelBefore = rightCurrentPanel;
    const bool leftPaneExpandedBefore = leftPaneExpanded;
    const bool rightPaneExpandedBefore = rightPaneExpanded;
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
    const auto snapshotMatches = [&] {
        return stable()
            && sameRows(activityRows(), rowsBefore)
            && leftStackGuard->panels() == leftOrderBefore
            && rightStackGuard->panels() == rightOrderBefore
            && leftPaneGuard->visibleWidgets() == leftVisibleBefore
            && rightPaneGuard->visibleWidgets() == rightVisibleBefore
            && leftStackGuard->panelSizes() == leftSizesBefore
            && rightStackGuard->panelSizes() == rightSizesBefore
            && leftPaneGuard->currentWidget() == leftWidgetBefore
            && rightPaneGuard->currentWidget() == rightWidgetBefore
            && leftPaneGuard->isCollapsed() == leftCollapsedBefore
            && rightPaneGuard->isCollapsed() == rightCollapsedBefore
            && leftPaneGuard->isHidden() == leftHiddenBefore
            && rightPaneGuard->isHidden() == rightHiddenBefore
            && leftBarGuard->isHidden() == leftBarHiddenBefore
            && rightBarGuard->isHidden() == rightBarHiddenBefore
            && leftBarGuard->currentSourceIndex() == leftBarCurrentBefore
            && rightBarGuard->currentSourceIndex() == rightBarCurrentBefore
            && leftBarGuard->activeSourceIndexes() == leftBarActiveBefore
            && rightBarGuard->activeSourceIndexes() == rightBarActiveBefore
            && leftCurrentPanel == leftCurrentPanelBefore
            && rightCurrentPanel == rightCurrentPanelBefore
            && leftPaneExpanded == leftPaneExpandedBefore
            && rightPaneExpanded == rightPaneExpandedBefore;
    };
    const auto restorePane = [&shellGuard](
                                const QPointer<ZzFluentUI::ZzSidePane> &sidePane,
                                const QPointer<ZzFluentUI::ZzPanelStack> &panelStack,
                                const QList<QWidget *> &order,
                                const QList<QWidget *> &visibleWidgets,
                                const QPointer<QWidget> &currentWidget,
                                const QList<int> &sizes,
                                bool collapsed,
                                bool hidden) {
        if (shellGuard == nullptr
            || sidePane == nullptr || panelStack == nullptr) {
            return false;
        }
        for (qsizetype index = 0; index < order.size(); ++index) {
            if (!panelStack->panels().contains(order.at(index))) {
                return false;
            }
            if (panelStack->panels().at(index) != order.at(index)
                && !panelStack->movePanel(order.at(index), static_cast<int>(index))) {
                return false;
            }
            if (shellGuard == nullptr
                || sidePane == nullptr || panelStack == nullptr) {
                return false;
            }
        }
        for (QWidget *const widget : panelStack->panels()) {
            if (widget != nullptr
                && !sidePane->setWidgetVisible(
                    widget, visibleWidgets.contains(widget))) {
                return false;
            }
            if (shellGuard == nullptr
                || sidePane == nullptr || panelStack == nullptr) {
                return false;
            }
        }
        if (currentWidget != nullptr) {
            if (!panelStack->panels().contains(currentWidget)
                || !sidePane->setCurrentWidget(currentWidget)) {
                return false;
            }
            if (shellGuard == nullptr
                || sidePane == nullptr || panelStack == nullptr) {
                return false;
            }
        }
        if (panelStack->panelSizes() != sizes) {
            if (!panelStack->setPanelSizes(sizes)) {
                return false;
            }
            if (shellGuard == nullptr
                || sidePane == nullptr || panelStack == nullptr) {
                return false;
            }
        }
        sidePane->setCollapsed(collapsed);
        if (shellGuard == nullptr || sidePane == nullptr) {
            return false;
        }
        sidePane->setVisible(!hidden);
        return shellGuard != nullptr && sidePane != nullptr;
    };
    const auto restoreSnapshot = [&] {
        if (shellGuard == nullptr) {
            return false;
        }
        leftCurrentPanel = leftCurrentPanelBefore;
        rightCurrentPanel = rightCurrentPanelBefore;
        leftPaneExpanded = leftPaneExpandedBefore;
        rightPaneExpanded = rightPaneExpandedBefore;
        if (!restorePane(
                leftPaneGuard, leftStackGuard, leftOrderBefore,
                leftVisibleBefore, leftWidgetBefore, leftSizesBefore,
                leftCollapsedBefore, leftHiddenBefore)
            || !restorePane(
                rightPaneGuard, rightStackGuard, rightOrderBefore,
                rightVisibleBefore, rightWidgetBefore, rightSizesBefore,
                rightCollapsedBefore, rightHiddenBefore)
            || !stable()) {
            return false;
        }
        syncSideEdgeVisibility();
        if (shellGuard == nullptr || !stable()) {
            return false;
        }
        leftBarGuard->setCurrentSourceIndex(leftBarCurrentBefore);
        if (shellGuard == nullptr || leftBarGuard == nullptr) {
            return false;
        }
        leftBarGuard->setActiveSourceIndexes(leftBarActiveBefore);
        if (shellGuard == nullptr || leftBarGuard == nullptr) {
            return false;
        }
        rightBarGuard->setCurrentSourceIndex(rightBarCurrentBefore);
        if (shellGuard == nullptr || rightBarGuard == nullptr) {
            return false;
        }
        rightBarGuard->setActiveSourceIndexes(rightBarActiveBefore);
        if (shellGuard == nullptr || rightBarGuard == nullptr) {
            return false;
        }
        return snapshotMatches();
    };
    const auto reject = [&](const QString &message) {
        if (shellGuard == nullptr) {
            return interrupted();
        }
        for (int attempt = 0; attempt < 2 && !snapshotMatches(); ++attempt) {
            static_cast<void>(restoreSnapshot());
            if (shellGuard == nullptr) {
                return interrupted();
            }
        }
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState, message, id.value());
    };
    struct ZzSideActivationScope final
    {
        ZzWorkspaceShellPrivate *shell;
        QPointer<ZzWorkspaceShell> shellGuard;
        ZzTransactionKind previous;

        explicit ZzSideActivationScope(
            ZzWorkspaceShellPrivate &target,
            const QPointer<ZzWorkspaceShell> &guard) noexcept
            : shell(&target)
            , shellGuard(guard)
            , previous(target.transactionKind)
        {
            shell->transactionKind = ZzTransactionKind::SideActivation;
        }

        ~ZzSideActivationScope()
        {
            if (shellGuard != nullptr) {
                shell->transactionKind = previous;
            }
        }
    } activationScope(*this, shellGuard);
    const bool left = zzIsLeftArea(record.activityArea);
    ZzWorkspacePanelId &currentPanel = left
        ? leftCurrentPanel : rightCurrentPanel;
    bool &paneExpanded = left ? leftPaneExpanded : rightPaneExpanded;
    const bool collapsesCurrent = currentPanel == id
        || pane->currentWidget() == contentGuard;
    if (visible) {
        const bool currentWidgetSet = pane->setCurrentWidget(contentGuard);
        if (shellGuard == nullptr) {
            return interrupted();
        }
        if (!currentWidgetSet || !stable()
            || pane->currentWidget() != contentGuard
            || pane->visibleWidgets() != QList<QWidget *>({contentGuard})) {
            return reject(QStringLiteral("Side panel activation was interrupted"));
        }
        currentPanel = id;
        paneExpanded = true;
        pane->setCollapsed(false);
        if (shellGuard == nullptr) {
            return interrupted();
        }
    } else if (currentPanel == id || pane->currentWidget() == contentGuard) {
        currentPanel = id;
        paneExpanded = false;
        pane->setCollapsed(true);
        if (shellGuard == nullptr) {
            return interrupted();
        }
    }
    if (!stable()) {
        return reject(QStringLiteral("Side panel collapse update was interrupted"));
    }
    syncSideEdgeVisibility();
    if (shellGuard == nullptr) {
        return interrupted();
    }
    if (!stable()) {
        return reject(QStringLiteral("Activity current state was interrupted"));
    }
    const QModelIndex expectedCurrent = zzActivityModel(modelGuard)->indexFor(id);
    ZzFluentUI::ZzActivityBar *const owningBar = left
        ? leftBarGuard.data() : rightBarGuard.data();
    const bool expandedStateMatches = currentPanel == id && paneExpanded
        && pane->currentWidget() == contentGuard
        && pane->visibleWidgets() == QList<QWidget *>({contentGuard})
        && !pane->isCollapsed() && !pane->isHidden()
        && expectedCurrent.isValid()
        && owningBar->currentSourceIndex() == expectedCurrent
        && owningBar->activeSourceIndexes()
            == QList<QModelIndex>({expectedCurrent});
    const bool collapsedStateMatches = currentPanel == id && !paneExpanded
        && pane->currentWidget() == contentGuard
        && pane->visibleWidgets() == QList<QWidget *>({contentGuard})
        && pane->isCollapsed() && pane->isHidden()
        && expectedCurrent.isValid()
        && owningBar->currentSourceIndex() == expectedCurrent
        && owningBar->activeSourceIndexes()
            == QList<QModelIndex>({expectedCurrent});
    if ((visible && !expandedStateMatches)
        || (!visible && ((collapsesCurrent && !collapsedStateMatches)
            || (!collapsesCurrent && !snapshotMatches())))) {
        return reject(QStringLiteral("Side panel state was interrupted"));
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
    if (transactionKind == ZzTransactionKind::NavigationIntegration) {
        return zzWorkspaceFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace panel transaction is in progress"));
    }
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
        ZzFluentUI::ZzDockPanel *const dock = zzLiveDockPanel(record);
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
        if (zzLiveDockPanel(record) == nullptr) {
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
        if (zzLiveDockPanel(record) != nullptr
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
    const QPointer<QObject> dockGuard(dockIdentity);
    const auto liveDock = [&dockGuard, dockIdentity] {
        auto *dock = qobject_cast<ZzFluentUI::ZzDockPanel *>(
            dockGuard.data());
        return dock == dockIdentity ? dock : nullptr;
    };
    const auto preserveForRetry = [&dockGuard] {
        auto *dock = qobject_cast<ZzFluentUI::ZzDockPanel *>(
            dockGuard.data());
        if (dock == nullptr) {
            return;
        }
        auto *const dockHost = qobject_cast<QMainWindow *>(
            dock->parentWidget());
        if (dockHost != nullptr && dockHost->layout() != nullptr) {
            dockHost->removeDockWidget(dock);
        }
        dock = qobject_cast<ZzFluentUI::ZzDockPanel *>(dockGuard.data());
        if (dock != nullptr) {
            dock->hide();
            dock->setParent(nullptr);
        }
    };
    ZzFluentUI::ZzDockPanel *dock = liveDock();
    if (dock == nullptr) {
        return true;
    }
    if (dock->widget() != nullptr) {
        static_cast<void>(dock->takeContentWidget());
    }
    dock = liveDock();
    if (dock == nullptr || dock->widget() != nullptr) {
        if (dock != nullptr) {
            preserveForRetry();
            return false;
        }
        return true;
    }
    const QPointer<QMainWindow> dockHost(qobject_cast<QMainWindow *>(
        dock->parentWidget()));
    if (dockHost != nullptr && dockHost->layout() != nullptr) {
        dockHost->removeDockWidget(dock);
    }
    dock = liveDock();
    if (dock == nullptr || dock->widget() != nullptr) {
        if (dock != nullptr) {
            preserveForRetry();
            return false;
        }
        return true;
    }
    if (dockHost == nullptr || dockHost->layout() != nullptr) {
        delete dock;
    }
    return true;
}

// 清理会修改 panels；按值保存身份快照，避免循环中的引用失效。
void ZzWorkspaceShellPrivate::cleanupPendingDockPanelForDestruction(
    ZzPanelRecord expected) // NOLINT(performance-unnecessary-value-param)
{
    while (true) {
        const int panelIndex = stablePanelIndex(expected);
        if (panelIndex < 0 || !panels.at(panelIndex).removalInProgress) {
            return;
        }
        ZzFluentUI::ZzDockPanel *const dock =
            zzLiveDockPanel(panels.at(panelIndex));
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

void ZzWorkspaceShellPrivate::activateActivity(
    const QModelIndex &sourceIndex,
    bool collapse)
{
    if (transactionKind != ZzTransactionKind::None
        || !sourceIndex.isValid() || sourceIndex.model() != activityModel) {
        return;
    }
    const ZzActivityRow *const activityRow =
        zzActivityModel(activityModel)->rowAt(sourceIndex.row());
    if (activityRow == nullptr) {
        return;
    }
    if (activityRow->kind == ZzActivityRowKind::FixedAction) {
        const QPointer<QAction> action(activityRow->action);
        if (!collapse && action != nullptr && action->isEnabled()) {
            ZzFluentUI::ZzActivityBar *const owningBar = zzIsLeftArea(
                activityRow->area) ? leftActivityBar : rightActivityBar;
            if (owningBar != nullptr) {
                owningBar->setSelectionVisible(true);
                owningBar->setCurrentSourceIndex(sourceIndex);
            }
            action->trigger();
        }
        return;
    }
    const ZzWorkspacePanelId id = activityRow->id;
    const int panelIndex = indexOf(id);
    if (panelIndex < 0 || panels.at(panelIndex).kind != ZzPanelKind::Side) {
        return;
    }
    const ZzPanelRecord &record = panels.at(panelIndex);
    const bool left = zzIsLeftArea(record.activityArea);
    const ZzWorkspacePanelId &currentPanel = left
        ? leftCurrentPanel : rightCurrentPanel;
    const bool paneExpanded = left ? leftPaneExpanded : rightPaneExpanded;
    const bool expand = !collapse || currentPanel != id || !paneExpanded;
    static_cast<void>(showPanel(id, expand));
}

void ZzWorkspaceShellPrivate::handleFixedActivityActionDestroyed(
    const ZzWorkspaceActivityId &id,
    QAction *actionIdentity)
{
    const int recordIndex = fixedActivityIndex(id, actionIdentity);
    if (recordIndex < 0) {
        return;
    }
    QObject::disconnect(fixedActivities[recordIndex].destroyedConnection);
    QObject::disconnect(fixedActivities[recordIndex].changedConnection);
    fixedActivities.removeAt(recordIndex);

    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    if (modelGuard != nullptr) {
        static_cast<void>(zzActivityModel(modelGuard)->remove(id));
    }
    if (shellGuard != nullptr) {
        syncSideEdgeVisibility();
    }
}

void ZzWorkspaceShellPrivate::handleFixedActivityActionChanged(
    const ZzWorkspaceActivityId &id,
    QAction *actionIdentity)
{
    const int recordIndex = fixedActivityIndex(id, actionIdentity);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    if (recordIndex < 0 || modelGuard == nullptr) {
        return;
    }
    if (fixedActivities.at(recordIndex).registrationInProgress) {
        fixedActivities[recordIndex].actionChangePending = true;
        return;
    }
    zzActivityModel(modelGuard)->notifyActionChanged(id);
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

std::optional<ZzWorkspaceShellPrivate::ZzActivityRowSnapshot>
ZzWorkspaceShellPrivate::activityRowSnapshot(
    const ZzWorkspacePanelId &id) const
{
    if (activityModel == nullptr) {
        return std::nullopt;
    }
    const auto *const model = zzActivityModel(activityModel);
    const QModelIndex index = model->indexFor(id);
    const ZzActivityRow *const row = model->rowAt(index.row());
    if (!index.isValid() || index.model() != activityModel || row == nullptr
        || row->id != id) {
        return std::nullopt;
    }
    return ZzActivityRowSnapshot{
        row->id, row->title, row->icon, row->area, row->badge, index.row()};
}

bool ZzWorkspaceShellPrivate::removeActivityRow(
    const ZzWorkspacePanelId &id)
{
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    if (modelGuard == nullptr) {
        return false;
    }
    const bool removed = zzActivityModel(modelGuard)->remove(id);
    return removed && modelGuard != nullptr && activityModel == modelGuard
        && !zzActivityModel(modelGuard)->indexFor(id).isValid();
}

bool ZzWorkspaceShellPrivate::restoreActivityRow(
    const ZzActivityRowSnapshot &snapshot)
{
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    if (modelGuard == nullptr || !snapshot.id.isValid()
        || zzActivityModel(modelGuard)->indexFor(snapshot.id).isValid()) {
        return false;
    }
    const bool inserted = zzActivityModel(modelGuard)->insert(
        snapshot.order,
        ZzActivityRow{snapshot.id, snapshot.title, snapshot.icon,
            snapshot.area, snapshot.badge,
            ZzActivityRowKind::SidePanel, {}, {}});
    return inserted && modelGuard != nullptr && activityModel == modelGuard
        && zzActivityModel(modelGuard)->indexFor(snapshot.id).isValid();
}

bool ZzWorkspaceShellPrivate::setActivityRowArea(
    const ZzWorkspacePanelId &id,
    ZzFluentUI::ZzActivityArea area)
{
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    if (modelGuard == nullptr) {
        return false;
    }
    const bool updated = zzActivityModel(modelGuard)->setArea(id, area);
    return updated && modelGuard != nullptr && activityModel == modelGuard;
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

int ZzWorkspaceShellPrivate::sidePanelTargetRow(
    ZzFluentUI::ZzActivityArea area,
    int projectionRow) const noexcept
{
    if (activityModel == nullptr || projectionRow < 0) {
        return -1;
    }
    const auto *const model = zzActivityModel(activityModel);
    int areaRow = 0;
    int sidePanelRow = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const ZzActivityRow *const activityRow = model->rowAt(row);
        if (activityRow == nullptr || activityRow->area != area) {
            continue;
        }
        if (areaRow >= projectionRow) {
            break;
        }
        if (activityRow->kind == ZzActivityRowKind::SidePanel) {
            ++sidePanelRow;
        }
        ++areaRow;
    }
    return sidePanelRow;
}

void ZzWorkspaceShellPrivate::syncSideEdgeVisibility()
{
    const QPointer<ZzWorkspaceShell> shellGuard(q_ptr);
    const QPointer<ZzFluentUI::ZzSidePane> leftPaneGuard(leftSidePane);
    const QPointer<ZzFluentUI::ZzSidePane> rightPaneGuard(rightSidePane);
    const QPointer<ZzFluentUI::ZzActivityBar> leftBarGuard(leftActivityBar);
    const QPointer<ZzFluentUI::ZzActivityBar> rightBarGuard(rightActivityBar);
    const QPointer<QAbstractListModel> modelGuard(activityModel);
    const bool leftHasActivity = modelGuard != nullptr
        && zzActivityModel(modelGuard)->hasRowsForSide(true);
    const bool rightHasActivity = modelGuard != nullptr
        && zzActivityModel(modelGuard)->hasRowsForSide(false);
    ++sideEdgeVisibilitySyncDepth;

    const bool derivePhysicalState =
        transactionKind == ZzTransactionKind::LayoutRestore;
    const auto syncEdge = [this, &shellGuard, &modelGuard, derivePhysicalState](
                              bool left,
                              const QPointer<ZzFluentUI::ZzSidePane> &pane,
                              const QPointer<ZzFluentUI::ZzActivityBar> &bar,
                              bool hasActivity) {
        const auto alive = [this, left, &shellGuard, &modelGuard, &pane, &bar] {
            return shellGuard != nullptr && modelGuard != nullptr
                && pane != nullptr && bar != nullptr
                && activityModel == modelGuard
                && (left ? leftSidePane == pane : rightSidePane == pane)
                && (left ? leftActivityBar == bar : rightActivityBar == bar);
        };
        if (!alive() || pane->panelStack() == nullptr) {
            return false;
        }

        ZzWorkspacePanelId currentPanel = left
            ? leftCurrentPanel : rightCurrentPanel;
        bool paneExpanded = left ? leftPaneExpanded : rightPaneExpanded;
        if (derivePhysicalState) {
            currentPanel = currentSideId(pane);
            paneExpanded = currentPanel.isValid() && !pane->isCollapsed();
        }

        const auto readyRecord = [this, left](const ZzWorkspacePanelId &id)
            -> const ZzPanelRecord * {
            const int panelIndex = indexOf(id);
            if (panelIndex < 0) {
                return nullptr;
            }
            const ZzPanelRecord &record = panels.at(panelIndex);
            return record.kind == ZzPanelKind::Side
                    && record.materialization == ZzMaterializationState::Ready
                    && !record.removalInProgress
                    && zzIsLeftArea(record.activityArea) == left
                    && record.content != nullptr
                    && record.content.data() == record.contentIdentity
                ? &record : nullptr;
        };
        const bool requestedCurrentWasInvalid = currentPanel.isValid()
            && readyRecord(currentPanel) == nullptr;
        if (requestedCurrentWasInvalid) {
            currentPanel = {};
            for (const ZzSideLayoutEntry &row : activityRows()) {
                if (zzIsLeftArea(row.area) == left
                    && readyRecord(row.id) != nullptr) {
                    currentPanel = row.id;
                    break;
                }
            }
        }

        const ZzPanelRecord *const currentRecord = readyRecord(currentPanel);
        if (currentRecord == nullptr) {
            currentPanel = {};
            paneExpanded = false;
        }
        if (left) {
            leftCurrentPanel = currentPanel;
            leftPaneExpanded = paneExpanded;
        } else {
            rightCurrentPanel = currentPanel;
            rightPaneExpanded = paneExpanded;
        }

        if (currentRecord != nullptr) {
            const QPointer<QWidget> content(currentRecord->content);
            if (pane->currentWidget() != content
                || pane->visibleWidgets() != QList<QWidget *>({content.data()})) {
                if (!pane->setCurrentWidget(content) || !alive()) {
                    return false;
                }
            }
        } else {
            const QList<QWidget *> visible = pane->visibleWidgets();
            for (QWidget *const content : visible) {
                if (content != nullptr
                    && (!pane->setWidgetVisible(content, false) || !alive())) {
                    return false;
                }
            }
        }

        bar->setCurrentSourceIndex(
            zzActivityModel(modelGuard)->indexFor(currentPanel));
        if (!alive()) {
            return false;
        }
        pane->setCollapsed(currentRecord == nullptr || !paneExpanded);
        if (!alive()) {
            return false;
        }
        bar->setSelectionVisible(!pane->isCollapsed());
        if (!alive()) {
            return false;
        }
        pane->setVisible(currentRecord != nullptr && paneExpanded);
        if (!alive()) {
            return false;
        }
        bar->setVisible(hasActivity);
        if (!alive()) {
            return false;
        }
        return true;
    };
    if (!syncEdge(
            true, leftPaneGuard, leftBarGuard, leftHasActivity)
        || !syncEdge(
            false, rightPaneGuard, rightBarGuard, rightHasActivity)) {
        if (shellGuard != nullptr) {
            --sideEdgeVisibilitySyncDepth;
        }
        return;
    }
    --sideEdgeVisibilitySyncDepth;
}

bool ZzWorkspaceShellPrivate::hasRegisteredStableId(
    const QString &value) const noexcept
{
    return std::any_of(
               panels.cbegin(), panels.cend(), [&value](const ZzPanelRecord &record) {
                   return record.id.value() == value;
               })
        || std::any_of(
            fixedActivities.cbegin(), fixedActivities.cend(),
            [&value](const ZzFixedActivityRecord &record) {
                return record.id.value() == value;
            });
}

int ZzWorkspaceShellPrivate::fixedActivityIndex(
    const ZzWorkspaceActivityId &id) const noexcept
{
    for (qsizetype index = 0; index < fixedActivities.size(); ++index) {
        if (fixedActivities.at(index).id == id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int ZzWorkspaceShellPrivate::fixedActivityIndex(
    const ZzWorkspaceActivityId &id,
    QAction *actionIdentity) const noexcept
{
    for (qsizetype index = 0; index < fixedActivities.size(); ++index) {
        if (fixedActivities.at(index).id == id
            && fixedActivities.at(index).actionIdentity == actionIdentity) {
            return static_cast<int>(index);
        }
    }
    return -1;
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
