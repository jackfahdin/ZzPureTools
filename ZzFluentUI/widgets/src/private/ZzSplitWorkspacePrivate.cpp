#include "ZzSplitWorkspacePrivate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <QtCore/QUuid>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzFluentUI {

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
    rootLayout->addWidget(std::get<ZzLeaf>(root->value).tabs);

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
    const ZzTabGroupId &requestedId)
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
    rebuildView();
    return newId;
}

bool ZzSplitWorkspacePrivate::removeEmptyGroup(const ZzTabGroupId &id)
{
    const QList<ZzTabGroupId> ids = groupIds();
    ZzNode *const leafNode = findLeaf(id);
    if (ids.size() <= 1 || leafNode == nullptr
        || std::get<ZzLeaf>(leafNode->value).tabs->count() != 0) {
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
    rebuildView();
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
    const auto *activeTabs = std::get<ZzLeaf>(activeNode->value).tabs;
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
        const auto *tabs = std::get<ZzLeaf>(candidate->value).tabs;
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
        auto *tabs = std::get<ZzLeaf>(leafNode->value).tabs;
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
        auto *tabs = std::get<ZzLeaf>(node->value).tabs;
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

ZzTabGroupId ZzSplitWorkspacePrivate::createGroupId()
{
    return ZzTabGroupId(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
}

} // namespace ZzFluentUI
