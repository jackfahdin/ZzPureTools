#include "ZzSplitWorkspacePrivate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <QtCore/QUuid>
#include <QtCore/QDataStream>
#include <QtCore/QEvent>
#include <QtCore/QMimeData>
#include <QtCore/QTimer>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzTabBarPrivate.h"
#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

namespace {

constexpr auto zzWorkspaceTabMimeType =
    "application/x-zz-split-workspace-tab-v1";

/** @brief 绘制工作区唯一共享的当前拖放目标区域。 */
class ZzWorkspaceDropOverlay final : public QWidget
{
public:
    explicit ZzWorkspaceDropOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("zzSplitWorkspaceDropOverlay"));
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QColor fill = palette().color(QPalette::Highlight);
        fill.setAlpha(72);
        QColor stroke = palette().color(QPalette::Highlight);
        stroke.setAlpha(180);
        QPainter painter(this);
        painter.fillRect(rect(), fill);
        painter.setPen(QPen(stroke, 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

[[nodiscard]] QPoint zzWorkspacePosition(
    QWidget *watched,
    ZzSplitWorkspace *workspace,
    const QPoint &position)
{
    return watched == workspace
        ? position
        : watched->mapTo(workspace, position);
}

} // namespace

ZzNode::ZzNode(ZzBranch branch)
    : value(std::move(branch))
{
}

ZzNode::ZzNode(ZzLeaf leaf)
    : value(std::move(leaf))
{
}

ZzSplitWorkspacePrivate::ZzSplitWorkspacePrivate(
    ZzSplitWorkspace *publicObject)
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    auto *workspaceLayout = new QVBoxLayout(q_ptr);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    rootHost = new QWidget(q_ptr);
    rootLayout = new QVBoxLayout(rootHost);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    workspaceLayout->addWidget(rootHost);

    activeId = createGroupId();
    root = std::make_unique<ZzNode>(
        ZzLeaf {activeId, new ZzTabWidget(rootHost)});
    prepareTabs(std::get<ZzLeaf>(root->value).tabs);
    rootLayout->addWidget(std::get<ZzLeaf>(root->value).tabs);

    q_ptr->setAcceptDrops(true);
    dragTokenExpiryTimer = new QTimer(q_ptr);
    dragTokenExpiryTimer->setSingleShot(true);
    dragTokenExpiryTimer->setInterval(5000);
    QObject::connect(
        dragTokenExpiryTimer,
        &QTimer::timeout,
        q_ptr,
        [this] { dragTokens.clear(); });

    QObject::connect(
        qApp,
        &QApplication::focusChanged,
        q_ptr,
        [this](QWidget *, QWidget *focused) {
            handleFocusChanged(focused);
        });
}

ZzSplitWorkspacePrivate::~ZzSplitWorkspacePrivate() = default;

QList<ZzTabGroupId> ZzSplitWorkspacePrivate::groupIds() const
{
    std::vector<const ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    QList<ZzTabGroupId> result;
    result.reserve(static_cast<qsizetype>(leaves.size()));
    for (const ZzNode *leafNode : leaves) {
        result.push_back(std::get<ZzLeaf>(leafNode->value).id);
    }
    return result;
}

ZzNode *ZzSplitWorkspacePrivate::findLeaf(
    const ZzTabGroupId &id) const noexcept
{
    if (!id.isValid()) {
        return nullptr;
    }
    std::vector<ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    const auto found = std::find_if(
        leaves.cbegin(),
        leaves.cend(),
        [&id](const ZzNode *node) {
            return std::get<ZzLeaf>(node->value).id == id;
        });
    return found != leaves.cend() ? *found : nullptr;
}

ZzNode *ZzSplitWorkspacePrivate::findLeaf(
    const ZzTabWidget *tabs) const noexcept
{
    if (tabs == nullptr) {
        return nullptr;
    }
    std::vector<ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    const auto found = std::find_if(
        leaves.cbegin(),
        leaves.cend(),
        [tabs](const ZzNode *node) {
            return std::get<ZzLeaf>(node->value).tabs == tabs;
        });
    return found != leaves.cend() ? *found : nullptr;
}

std::optional<ZzTabGroupId> ZzSplitWorkspacePrivate::splitGroup(
    const ZzTabGroupId &source,
    Qt::Orientation orientation,
    ZzSplitPlacement placement,
    const ZzTabGroupId &requestedId,
    bool rebuildViewAfterCommit)
{
    const QList<ZzTabGroupId> ids = groupIds();
    ZzNode *const sourceNode = findLeaf(source);
    if (sourceNode == nullptr
        || ids.size() >= maximumGroupCount
        || (orientation != Qt::Horizontal && orientation != Qt::Vertical)
        || (placement != ZzSplitPlacement::Before
            && placement != ZzSplitPlacement::After)) {
        return std::nullopt;
    }

    const ZzTabGroupId newId = requestedId.isValid()
        ? requestedId
        : createGroupId();
    if (std::find(ids.cbegin(), ids.cend(), newId) != ids.cend()) {
        return std::nullopt;
    }

    const bool mergesWithParent = sourceNode->parent != nullptr
        && std::holds_alternative<ZzBranch>(sourceNode->parent->value)
        && std::get<ZzBranch>(sourceNode->parent->value).orientation
            == orientation;
    if (!mergesWithParent
        && nodeDepth(sourceNode) >= maximumTreeDepth) {
        return std::nullopt;
    }

    auto newLeaf = std::make_unique<ZzNode>(
        ZzLeaf {newId, new ZzTabWidget(rootHost)});
    prepareTabs(std::get<ZzLeaf>(newLeaf->value).tabs);
    if (mergesWithParent) {
        ZzNode *const parent = sourceNode->parent;
        auto &siblings = std::get<ZzBranch>(parent->value).children;
        const auto sourceIt = std::find_if(
            siblings.begin(),
            siblings.end(),
            [sourceNode](const auto &child) {
                return child.get() == sourceNode;
            });
        Q_ASSERT(sourceIt != siblings.end());
        const auto offset = placement == ZzSplitPlacement::After ? 1 : 0;
        newLeaf->parent = parent;
        siblings.insert(sourceIt + offset, std::move(newLeaf));
    } else {
        ZzNode *const oldParent = sourceNode->parent;
        std::unique_ptr<ZzNode> *owner = &root;
        if (oldParent != nullptr) {
            auto &siblings = std::get<ZzBranch>(oldParent->value).children;
            const auto sourceIt = std::find_if(
                siblings.begin(),
                siblings.end(),
                [sourceNode](const auto &child) {
                    return child.get() == sourceNode;
                });
            Q_ASSERT(sourceIt != siblings.end());
            owner = &*sourceIt;
        }

        auto oldLeaf = std::move(*owner);
        ZzBranch branch;
        branch.orientation = orientation;
        if (placement == ZzSplitPlacement::Before) {
            branch.children.push_back(std::move(newLeaf));
            branch.children.push_back(std::move(oldLeaf));
        } else {
            branch.children.push_back(std::move(oldLeaf));
            branch.children.push_back(std::move(newLeaf));
        }
        auto replacement = std::make_unique<ZzNode>(std::move(branch));
        replacement->parent = oldParent;
        for (auto &child : std::get<ZzBranch>(replacement->value).children) {
            child->parent = replacement.get();
        }
        *owner = std::move(replacement);
    }

    root = normalize(std::move(root), nullptr);
    if (rebuildViewAfterCommit) {
        rebuildView();
    }
    return newId;
}

bool ZzSplitWorkspacePrivate::removeEmptyGroup(
    const ZzTabGroupId &id,
    bool rebuildViewAfterCommit)
{
    const QList<ZzTabGroupId> ids = groupIds();
    ZzNode *const leafNode = findLeaf(id);
    const QPointer<ZzTabWidget> tabs = leafNode != nullptr
        ? std::get<ZzLeaf>(leafNode->value).tabs
        : QPointer<ZzTabWidget> {};
    if (ids.size() <= 1 || leafNode == nullptr
        || (!tabs.isNull() && tabs->count() != 0)) {
        return false;
    }

    ZzNode *const parent = leafNode->parent;
    Q_ASSERT(parent != nullptr);
    auto &siblings = std::get<ZzBranch>(parent->value).children;
    const auto leafIt = std::find_if(
        siblings.begin(),
        siblings.end(),
        [leafNode](const auto &child) {
            return child.get() == leafNode;
        });
    Q_ASSERT(leafIt != siblings.end());
    siblings.erase(leafIt);

    root = normalize(std::move(root), nullptr);
    if (activeId == id) {
        activeId = groupIds().constFirst();
    }
    if (rebuildViewAfterCommit) {
        rebuildView();
    }
    return true;
}

ZzTabGroupId ZzSplitWorkspacePrivate::adjacentGroup(
    Qt::Edge direction) const
{
    if (direction != Qt::LeftEdge && direction != Qt::TopEdge
        && direction != Qt::RightEdge && direction != Qt::BottomEdge) {
        return {};
    }

    ZzNode *const activeNode = findLeaf(activeId);
    if (activeNode == nullptr) {
        return {};
    }
    const auto *activeTabs =
        std::get<ZzLeaf>(activeNode->value).tabs.data();
    if (activeTabs == nullptr) {
        return {};
    }
    const QPoint activeCenter = activeTabs->mapToGlobal(
        activeTabs->rect().center());

    std::vector<const ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    const ZzNode *best = nullptr;
    int bestPrimary = std::numeric_limits<int>::max();
    int bestSecondary = std::numeric_limits<int>::max();
    qsizetype bestOrder = std::numeric_limits<qsizetype>::max();
    for (qsizetype index = 0;
         index < static_cast<qsizetype>(leaves.size());
         ++index) {
        const ZzNode *candidate = leaves[static_cast<std::size_t>(index)];
        if (candidate == activeNode) {
            continue;
        }
        const auto *tabs = std::get<ZzLeaf>(candidate->value).tabs.data();
        if (tabs == nullptr) {
            continue;
        }
        const QPoint center = tabs->mapToGlobal(tabs->rect().center());
        const int deltaX = center.x() - activeCenter.x();
        const int deltaY = center.y() - activeCenter.y();
        const bool inDirection = direction == Qt::LeftEdge ? deltaX < 0
            : direction == Qt::RightEdge                  ? deltaX > 0
            : direction == Qt::TopEdge                    ? deltaY < 0
                                                          : deltaY > 0;
        if (!inDirection) {
            continue;
        }
        const int primary = (direction == Qt::LeftEdge
                             || direction == Qt::RightEdge)
            ? std::abs(deltaX)
            : std::abs(deltaY);
        const int secondary = (direction == Qt::LeftEdge
                               || direction == Qt::RightEdge)
            ? std::abs(deltaY)
            : std::abs(deltaX);
        if (primary < bestPrimary
            || (primary == bestPrimary && secondary < bestSecondary)
            || (primary == bestPrimary && secondary == bestSecondary
                && index < bestOrder)) {
            best = candidate;
            bestPrimary = primary;
            bestSecondary = secondary;
            bestOrder = index;
        }
    }
    return best != nullptr ? std::get<ZzLeaf>(best->value).id
                           : ZzTabGroupId {};
}

ZzTreeSnapshot ZzSplitWorkspacePrivate::captureTreeSnapshot() const
{
    return ZzTreeSnapshot {captureNodeSnapshot(root.get()), activeId};
}

bool ZzSplitWorkspacePrivate::restoreTreeSnapshot(
    const ZzTreeSnapshot &snapshot)
{
    QPointer<ZzSplitWorkspace> guardedWorkspace = q_ptr;
    root = buildSnapshotNode(snapshot.root, nullptr);
    activeId = snapshot.activeId;
    rebuildView();
    if (guardedWorkspace.isNull()) {
        return false;
    }
    restoreNodeSizes(snapshot.root, root.get());
    return true;
}

bool ZzSplitWorkspacePrivate::eventFilter(QObject *watched, QEvent *event)
{
    auto *watchedWidget = qobject_cast<QWidget *>(watched);
    if (watchedWidget == nullptr || event == nullptr) {
        return false;
    }
    switch (event->type()) {
    case QEvent::DragEnter:
        return handleDragEnter(
            watchedWidget, static_cast<QDragEnterEvent *>(event));
    case QEvent::DragMove:
        return handleDragMove(
            watchedWidget, static_cast<QDragMoveEvent *>(event));
    case QEvent::DragLeave:
        return handleDragLeave(static_cast<QDragLeaveEvent *>(event));
    case QEvent::Drop:
        return handleDrop(watchedWidget, static_cast<QDropEvent *>(event));
    default:
        return false;
    }
}

bool ZzSplitWorkspacePrivate::handleDragEnter(
    QWidget *watched,
    QDragEnterEvent *event)
{
    if (watched == nullptr || event == nullptr
        || !ensureDragToken(event->mimeData())) {
        if (event != nullptr) {
            event->ignore();
        }
        return event != nullptr
            && (event->mimeData()->hasFormat(
                    QString::fromLatin1(zzWorkspaceTabMimeType))
                || event->mimeData()->hasFormat(ZzTabMimeData::format()));
    }
    const QPoint position = zzWorkspacePosition(
        watched, q_ptr, event->position().toPoint());
    const ZzTabGroupId target = groupAt(position);
    if (!target.isValid()) {
        event->ignore();
        hideDropOverlay();
        return true;
    }
    showDropOverlay(dropZoneRect(target, dropZoneAt(target, position)));
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

bool ZzSplitWorkspacePrivate::handleDragMove(
    QWidget *watched,
    QDragMoveEvent *event)
{
    if (watched == nullptr || event == nullptr
        || !dragRecord(event->mimeData()).has_value()) {
        if (event != nullptr) {
            event->ignore();
        }
        hideDropOverlay();
        return event != nullptr
            && event->mimeData()->hasFormat(
                QString::fromLatin1(zzWorkspaceTabMimeType));
    }
    const QPoint position = zzWorkspacePosition(
        watched, q_ptr, event->position().toPoint());
    const ZzTabGroupId target = groupAt(position);
    if (!target.isValid()) {
        event->ignore();
        hideDropOverlay();
        return true;
    }
    showDropOverlay(dropZoneRect(target, dropZoneAt(target, position)));
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

bool ZzSplitWorkspacePrivate::handleDragLeave(QDragLeaveEvent *event)
{
    hideDropOverlay();
    discardDragTokens();
    if (event == nullptr) {
        return false;
    }
    event->accept();
    return true;
}

bool ZzSplitWorkspacePrivate::handleDrop(
    QWidget *watched,
    QDropEvent *event)
{
    const auto record = event == nullptr
        ? std::nullopt
        : dragRecord(event->mimeData());
    if (watched == nullptr || event == nullptr || !record.has_value()) {
        if (event != nullptr) {
            event->ignore();
        }
        hideDropOverlay();
        return event != nullptr
            && event->mimeData()->hasFormat(
                QString::fromLatin1(zzWorkspaceTabMimeType));
    }
    const QPoint position = zzWorkspacePosition(
        watched, q_ptr, event->position().toPoint());
    const ZzTabGroupId target = groupAt(position);
    if (!target.isValid()) {
        event->ignore();
        hideDropOverlay();
        discardDragTokens();
        return true;
    }
    const ZzWorkspaceDropZone zone = dropZoneAt(target, position);
    const ZzWorkspaceDragRecord stableRecord = record.value();
    discardDragTokens();
    hideDropOverlay();
    const bool committed = q_ptr->moveTabToDropZone(
        stableRecord.sourceId,
        stableRecord.sourceIndex,
        target,
        zone);
    if (!committed) {
        event->ignore();
        return true;
    }
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return true;
}

void ZzSplitWorkspacePrivate::handleFocusChanged(QWidget *focused)
{
    for (QWidget *current = focused;
         current != nullptr && current != q_ptr;
         current = current->parentWidget()) {
        if (auto *tabs = qobject_cast<ZzTabWidget *>(current);
            tabs != nullptr) {
            ZzNode *const node = findLeaf(tabs);
            if (node == nullptr) {
                return;
            }
            const ZzTabGroupId id = std::get<ZzLeaf>(node->value).id;
            if (activeId != id) {
                activeId = id;
                Q_EMIT q_ptr->activeGroupChanged(id);
            }
            return;
        }
    }
}

int ZzSplitWorkspacePrivate::nodeDepth(const ZzNode *node) noexcept
{
    int depth = 0;
    for (const ZzNode *current = node;
         current != nullptr;
         current = current->parent) {
        ++depth;
    }
    return depth;
}

void ZzSplitWorkspacePrivate::collectLeaves(
    ZzNode *node,
    std::vector<ZzNode *> &leaves)
{
    if (node == nullptr) {
        return;
    }
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        leaves.push_back(node);
        return;
    }
    for (auto &child : std::get<ZzBranch>(node->value).children) {
        collectLeaves(child.get(), leaves);
    }
}

void ZzSplitWorkspacePrivate::collectLeaves(
    const ZzNode *node,
    std::vector<const ZzNode *> &leaves)
{
    if (node == nullptr) {
        return;
    }
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        leaves.push_back(node);
        return;
    }
    for (const auto &child : std::get<ZzBranch>(node->value).children) {
        collectLeaves(child.get(), leaves);
    }
}

std::unique_ptr<ZzNode> ZzSplitWorkspacePrivate::normalize(
    std::unique_ptr<ZzNode> node,
    ZzNode *parent)
{
    node->parent = parent;
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        return node;
    }

    auto &branch = std::get<ZzBranch>(node->value);
    for (auto &child : branch.children) {
        child = normalize(std::move(child), node.get());
    }

    std::vector<std::unique_ptr<ZzNode>> flattened;
    flattened.reserve(branch.children.size());
    for (auto &child : branch.children) {
        if (std::holds_alternative<ZzBranch>(child->value)
            && std::get<ZzBranch>(child->value).orientation
                == branch.orientation) {
            auto &grandchildren =
                std::get<ZzBranch>(child->value).children;
            for (auto &grandchild : grandchildren) {
                grandchild->parent = node.get();
                flattened.push_back(std::move(grandchild));
            }
        } else {
            child->parent = node.get();
            flattened.push_back(std::move(child));
        }
    }
    branch.children = std::move(flattened);

    if (branch.children.size() == 1) {
        auto onlyChild = std::move(branch.children.front());
        onlyChild->parent = parent;
        return onlyChild;
    }
    return node;
}

void ZzSplitWorkspacePrivate::rebuildView()
{
    std::vector<ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    for (ZzNode *leafNode : leaves) {
        auto &leaf = std::get<ZzLeaf>(leafNode->value);
        if (leaf.tabs.isNull()) {
            leaf.tabs = new ZzTabWidget(rootHost);
            prepareTabs(leaf.tabs);
        }
        auto *tabs = leaf.tabs.data();
        tabs->setParent(rootHost);
    }

    if (rootLayout->count() > 0) {
        QWidget *const previousRoot = rootLayout->itemAt(0)->widget();
        rootLayout->removeWidget(previousRoot);
        if (qobject_cast<QSplitter *>(previousRoot) != nullptr) {
            QPointer<ZzSplitWorkspace> guardedWorkspace = q_ptr;
            QPointer<QWidget> guardedPreviousRoot = previousRoot;
            previousRoot->setParent(nullptr);
            if (guardedWorkspace.isNull()) {
                delete guardedPreviousRoot.data();
                return;
            }
            delete previousRoot;
            if (guardedWorkspace.isNull()) {
                return;
            }
        }
    }
    clearSplitterPointers(root.get());
    QWidget *const rootWidget = buildNodeWidget(root.get(), rootHost);
    rootLayout->addWidget(rootWidget);
    rootWidget->show();
}

QWidget *ZzSplitWorkspacePrivate::buildNodeWidget(
    ZzNode *node,
    QWidget *parent)
{
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        auto &leaf = std::get<ZzLeaf>(node->value);
        if (leaf.tabs.isNull()) {
            leaf.tabs = new ZzTabWidget(rootHost);
            prepareTabs(leaf.tabs);
        }
        auto *tabs = leaf.tabs.data();
        tabs->setParent(parent);
        return tabs;
    }

    auto &branch = std::get<ZzBranch>(node->value);
    branch.splitter = new QSplitter(branch.orientation, parent);
    branch.splitter->setChildrenCollapsible(false);
    for (auto &child : branch.children) {
        branch.splitter->addWidget(
            buildNodeWidget(child.get(), branch.splitter));
    }
    return branch.splitter;
}

void ZzSplitWorkspacePrivate::clearSplitterPointers(ZzNode *node)
{
    if (node == nullptr || std::holds_alternative<ZzLeaf>(node->value)) {
        return;
    }
    auto &branch = std::get<ZzBranch>(node->value);
    branch.splitter = nullptr;
    for (auto &child : branch.children) {
        clearSplitterPointers(child.get());
    }
}

ZzTreeNodeSnapshot ZzSplitWorkspacePrivate::captureNodeSnapshot(
    const ZzNode *node)
{
    Q_ASSERT(node != nullptr);
    ZzTreeNodeSnapshot snapshot;
    if (std::holds_alternative<ZzLeaf>(node->value)) {
        const auto &leaf = std::get<ZzLeaf>(node->value);
        snapshot.id = leaf.id;
        snapshot.tabs = leaf.tabs;
        return snapshot;
    }

    snapshot.leaf = false;
    const auto &branch = std::get<ZzBranch>(node->value);
    snapshot.orientation = branch.orientation;
    if (branch.splitter != nullptr) {
        snapshot.sizes = branch.splitter->sizes();
    }
    snapshot.children.reserve(branch.children.size());
    for (const auto &child : branch.children) {
        snapshot.children.push_back(captureNodeSnapshot(child.get()));
    }
    return snapshot;
}

std::unique_ptr<ZzNode> ZzSplitWorkspacePrivate::buildSnapshotNode(
    const ZzTreeNodeSnapshot &snapshot,
    ZzNode *parent)
{
    if (snapshot.leaf) {
        QPointer<ZzTabWidget> tabs = snapshot.tabs;
        if (tabs.isNull()) {
            tabs = new ZzTabWidget(rootHost);
        }
        prepareTabs(tabs);
        auto node = std::make_unique<ZzNode>(
            ZzLeaf {snapshot.id, tabs});
        node->parent = parent;
        return node;
    }

    ZzBranch branch;
    branch.orientation = snapshot.orientation;
    auto node = std::make_unique<ZzNode>(std::move(branch));
    node->parent = parent;
    auto &children = std::get<ZzBranch>(node->value).children;
    children.reserve(snapshot.children.size());
    for (const auto &childSnapshot : snapshot.children) {
        children.push_back(buildSnapshotNode(childSnapshot, node.get()));
    }
    return node;
}

void ZzSplitWorkspacePrivate::restoreNodeSizes(
    const ZzTreeNodeSnapshot &snapshot,
    ZzNode *node)
{
    if (snapshot.leaf || node == nullptr
        || std::holds_alternative<ZzLeaf>(node->value)) {
        return;
    }
    auto &branch = std::get<ZzBranch>(node->value);
    if (branch.splitter != nullptr
        && snapshot.sizes.size()
            == static_cast<qsizetype>(branch.children.size())) {
        branch.splitter->setSizes(snapshot.sizes);
    }
    const auto childCount = std::min(
        snapshot.children.size(), branch.children.size());
    for (std::size_t index = 0; index < childCount; ++index) {
        restoreNodeSizes(
            snapshot.children[index], branch.children[index].get());
    }
}

void ZzSplitWorkspacePrivate::prepareTabs(ZzTabWidget *tabs)
{
    if (tabs == nullptr) {
        return;
    }
    tabs->installEventFilter(q_ptr);
    tabs->fluentTabBar()->installEventFilter(q_ptr);
}

bool ZzSplitWorkspacePrivate::ensureDragToken(const QMimeData *mimeData)
{
    if (dragRecord(mimeData).has_value()) {
        return true;
    }
    const auto *tabPayload = dynamic_cast<const ZzTabMimeData *>(mimeData);
    if (tabPayload == nullptr || tabPayload->source.isNull()
        || tabPayload->page.isNull()) {
        return false;
    }
    ZzNode *const sourceNode = findLeaf(tabPayload->source);
    if (sourceNode == nullptr
        || tabPayload->sourceIndex < 0
        || tabPayload->sourceIndex >= tabPayload->source->count()
        || tabPayload->source->widget(tabPayload->sourceIndex)
            != tabPayload->page) {
        return false;
    }

    const ZzTabGroupId sourceId =
        std::get<ZzLeaf>(sourceNode->value).id;
    const QString token =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream << quint32(1) << token << sourceId.value()
           << qint32(tabPayload->sourceIndex);
    dragTokens.clear();
    dragTokens.insert(
        token,
        ZzWorkspaceDragRecord {
            sourceId, tabPayload->sourceIndex, tabPayload->page});
    dragTokenExpiryTimer->start();
    const_cast<QMimeData *>(mimeData)->setData(
        QString::fromLatin1(zzWorkspaceTabMimeType), encoded);
    return true;
}

std::optional<ZzWorkspaceDragRecord> ZzSplitWorkspacePrivate::dragRecord(
    const QMimeData *mimeData) const
{
    const QString format = QString::fromLatin1(zzWorkspaceTabMimeType);
    if (mimeData == nullptr || !mimeData->hasFormat(format)) {
        return std::nullopt;
    }
    const QByteArray encoded = mimeData->data(format);
    if (encoded.isEmpty() || encoded.size() > 4096) {
        return std::nullopt;
    }
    QDataStream stream(encoded);
    quint32 version = 0;
    QString token;
    QString sourceValue;
    qint32 sourceIndex = -1;
    stream >> version >> token >> sourceValue >> sourceIndex;
    if (stream.status() != QDataStream::Ok || !stream.atEnd()
        || version != 1) {
        return std::nullopt;
    }
    const auto found = dragTokens.constFind(token);
    if (found == dragTokens.cend()
        || found->sourceId.value() != sourceValue
        || found->sourceIndex != static_cast<int>(sourceIndex)
        || found->page.isNull()) {
        return std::nullopt;
    }
    ZzNode *const sourceNode = findLeaf(found->sourceId);
    if (sourceNode == nullptr) {
        return std::nullopt;
    }
    const QPointer<ZzTabWidget> tabs =
        std::get<ZzLeaf>(sourceNode->value).tabs;
    if (tabs.isNull() || found->sourceIndex < 0
        || found->sourceIndex >= tabs->count()
        || tabs->widget(found->sourceIndex) != found->page) {
        return std::nullopt;
    }
    return found.value();
}

ZzTabGroupId ZzSplitWorkspacePrivate::groupAt(
    const QPoint &position) const
{
    std::vector<const ZzNode *> leaves;
    collectLeaves(root.get(), leaves);
    for (const ZzNode *leafNode : leaves) {
        const auto &leaf = std::get<ZzLeaf>(leafNode->value);
        if (leaf.tabs.isNull()) {
            continue;
        }
        const QRect geometry(
            leaf.tabs->mapTo(q_ptr, QPoint(0, 0)), leaf.tabs->size());
        if (geometry.contains(position)) {
            return leaf.id;
        }
    }
    return {};
}

ZzWorkspaceDropZone ZzSplitWorkspacePrivate::dropZoneAt(
    const ZzTabGroupId &target,
    const QPoint &position) const
{
    const QRect targetRect = dropZoneRect(
        target, ZzWorkspaceDropZone::Center)
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Left))
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Top))
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Right))
        .united(dropZoneRect(target, ZzWorkspaceDropZone::Bottom));
    const ZzWidgetTheme theme(q_ptr);
    const int extent = std::max(
        1,
        qCeil(theme.snapshot()->metric(
            ZzMetricToken::WorkspaceDropTargetExtent)));
    const int horizontalExtent = std::min(
        extent, std::max(1, (targetRect.width() - 1) / 2));
    const int verticalExtent = std::min(
        extent, std::max(1, (targetRect.height() - 1) / 2));
    if (position.x() < targetRect.left() + horizontalExtent) {
        return ZzWorkspaceDropZone::Left;
    }
    if (position.x() > targetRect.right() - horizontalExtent) {
        return ZzWorkspaceDropZone::Right;
    }
    if (position.y() < targetRect.top() + verticalExtent) {
        return ZzWorkspaceDropZone::Top;
    }
    if (position.y() > targetRect.bottom() - verticalExtent) {
        return ZzWorkspaceDropZone::Bottom;
    }
    return ZzWorkspaceDropZone::Center;
}

QRect ZzSplitWorkspacePrivate::dropZoneRect(
    const ZzTabGroupId &target,
    ZzWorkspaceDropZone zone) const
{
    ZzNode *const targetNode = findLeaf(target);
    if (targetNode == nullptr) {
        return {};
    }
    const QPointer<ZzTabWidget> tabs =
        std::get<ZzLeaf>(targetNode->value).tabs;
    if (tabs.isNull()) {
        return {};
    }
    const QRect targetRect(tabs->mapTo(q_ptr, QPoint(0, 0)), tabs->size());
    const ZzWidgetTheme theme(q_ptr);
    const int extent = std::max(
        1,
        qCeil(theme.snapshot()->metric(
            ZzMetricToken::WorkspaceDropTargetExtent)));
    const int horizontalExtent = std::min(
        extent, std::max(1, (targetRect.width() - 1) / 2));
    const int verticalExtent = std::min(
        extent, std::max(1, (targetRect.height() - 1) / 2));
    switch (zone) {
    case ZzWorkspaceDropZone::Left:
        return QRect(
            targetRect.left(), targetRect.top(),
            horizontalExtent, targetRect.height());
    case ZzWorkspaceDropZone::Right:
        return QRect(
            targetRect.right() - horizontalExtent + 1,
            targetRect.top(), horizontalExtent, targetRect.height());
    case ZzWorkspaceDropZone::Top:
        return QRect(
            targetRect.left() + horizontalExtent,
            targetRect.top(),
            targetRect.width() - horizontalExtent * 2,
            verticalExtent);
    case ZzWorkspaceDropZone::Bottom:
        return QRect(
            targetRect.left() + horizontalExtent,
            targetRect.bottom() - verticalExtent + 1,
            targetRect.width() - horizontalExtent * 2,
            verticalExtent);
    case ZzWorkspaceDropZone::Center:
        return targetRect.adjusted(
            horizontalExtent,
            verticalExtent,
            -horizontalExtent,
            -verticalExtent);
    default:
        return {};
    }
}

void ZzSplitWorkspacePrivate::showDropOverlay(const QRect &geometry)
{
    if (geometry.isEmpty()) {
        hideDropOverlay();
        return;
    }
    if (dropOverlay == nullptr) {
        dropOverlay = new ZzWorkspaceDropOverlay(q_ptr);
    }
    dropOverlay->setGeometry(geometry);
    dropOverlay->show();
    dropOverlay->raise();
}

void ZzSplitWorkspacePrivate::hideDropOverlay()
{
    if (dropOverlay != nullptr) {
        dropOverlay->hide();
    }
}

void ZzSplitWorkspacePrivate::discardDragTokens()
{
    dragTokens.clear();
    dragTokenExpiryTimer->stop();
}

ZzTabGroupId ZzSplitWorkspacePrivate::createGroupId()
{
    return ZzTabGroupId(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
}

} // namespace ZzFluentUI
