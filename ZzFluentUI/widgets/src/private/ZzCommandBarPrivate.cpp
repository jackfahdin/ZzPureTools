#include "ZzCommandBarPrivate.h"

#include <algorithm>

#include <QtCore/QObject>
#include <QtGui/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionToolButton>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzCommandBar.h>

namespace ZzFluentUI {

namespace {

constexpr int zzMinimumCommandExtent = 28;

/** @brief 返回 action 列表中从开头可放入宽度的数量。 */
int zzFittingActionCount(
    const QList<ZzCommandBarPrivate::ZzCommandBarActionRecord> &records,
    int availableWidth,
    bool compact,
    ZzCommandBarPrivate *commandBar)
{
    int usedWidth = 0;
    int count = 0;
    for (const ZzCommandBarPrivate::ZzCommandBarActionRecord &record : records) {
        QAction *action = record.action.data();
        if (action == nullptr) {
            continue;
        }
        const int width = commandBar->actionWidth(action, compact);
        if (usedWidth + width > availableWidth) {
            break;
        }
        usedWidth += width;
        ++count;
    }
    return count;
}

} // namespace

ZzCommandBarPrivate::ZzCommandBarPrivate(ZzCommandBar *q)
    : q_ptr(q)
    , toolBar(new QToolBar(q))
    , moreButton(new QToolButton(q))
    , moreMenu(new QMenu(q))
{
    Q_ASSERT(q_ptr != nullptr);
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setContextMenuPolicy(Qt::NoContextMenu);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    toolBar->setContentsMargins(0, 0, 0, 0);

    moreButton->setAutoRaise(true);
    moreButton->setPopupMode(QToolButton::InstantPopup);
    moreButton->setMenu(moreMenu);
    moreButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    moreButton->setIcon(q_ptr->style()->standardIcon(QStyle::SP_ArrowDown));
    moreButton->setAccessibleName(ZzCommandBar::tr("更多命令"));
    moreButton->setToolTip(ZzCommandBar::tr("更多命令"));
    moreButton->hide();
}

ZzCommandBarPrivate::~ZzCommandBarPrivate()
{
    const auto disconnectRecords = [](QList<ZzCommandBarActionRecord> *records) {
        for (const ZzCommandBarActionRecord &record : *records) {
            QObject::disconnect(record.destroyedConnection);
            QObject::disconnect(record.changedConnection);
        }
    };
    disconnectRecords(&primaryRecords);
    disconnectRecords(&secondaryRecords);
}

bool ZzCommandBarPrivate::insertAction(
    QList<ZzCommandBarActionRecord> *group,
    int index,
    QAction *action)
{
    if (group == nullptr || action == nullptr || containsAction(action)
        || index < 0 || index > group->size()) {
        return false;
    }

    ZzCommandBarActionRecord record;
    record.action = action;
    record.destroyedConnection = QObject::connect(
        action,
        &QObject::destroyed,
        q_ptr,
        [this](QObject *object) {
            removeDestroyedAction(object);
        });
    record.changedConnection = QObject::connect(
        action,
        &QAction::changed,
        q_ptr,
        [this] {
            invalidateWidths();
        });
    group->insert(index, record);
    invalidateWidths();
    return true;
}

bool ZzCommandBarPrivate::removeAction(QAction *action)
{
    if (action == nullptr) {
        return false;
    }
    const auto removeFromGroup = [action](QList<ZzCommandBarActionRecord> *group) {
        for (auto iterator = group->begin(); iterator != group->end(); ++iterator) {
            if (iterator->action.data() != action) {
                continue;
            }
            QObject::disconnect(iterator->destroyedConnection);
            QObject::disconnect(iterator->changedConnection);
            group->erase(iterator);
            return true;
        }
        return false;
    };
    const bool removed = removeFromGroup(&primaryRecords)
        || removeFromGroup(&secondaryRecords);
    if (removed) {
        toolBar->removeAction(action);
        moreMenu->removeAction(action);
        invalidateWidths();
    }
    return removed;
}

QList<QAction *> ZzCommandBarPrivate::actions(
    const QList<ZzCommandBarActionRecord> &group) const
{
    QList<QAction *> result;
    result.reserve(group.size());
    for (const ZzCommandBarActionRecord &record : group) {
        if (QAction *action = record.action.data(); action != nullptr) {
            result.append(action);
        }
    }
    return result;
}

void ZzCommandBarPrivate::invalidateWidths()
{
    widthsDirty = true;
    rebuildPresentation();
}

void ZzCommandBarPrivate::rebuildPresentation()
{
    if (rebuilding) {
        return;
    }
    rebuilding = true;
    const ZzPresentation presentation = calculatePresentation(q_ptr->width());
    applyToolButtonStyle(presentation.compact);
    moveActionsWithoutCloning(presentation);
    updateMoreButton(presentation.hasOverflow);

    const QRect bounds = q_ptr->contentsRect();
    const int moreExtent = presentation.hasOverflow
        ? moreButton->sizeHint().width()
        : 0;
    const QRect logicalToolBar(
        bounds.left(),
        bounds.top(),
        std::max(0, bounds.width() - moreExtent),
        bounds.height());
    const QRect logicalMore(
        bounds.right() - moreExtent + 1,
        bounds.top(),
        moreExtent,
        bounds.height());
    toolBar->setGeometry(QStyle::visualRect(
        q_ptr->layoutDirection(), bounds, logicalToolBar));
    moreButton->setGeometry(QStyle::visualRect(
        q_ptr->layoutDirection(), bounds, logicalMore));
    rebuilding = false;
}

void ZzCommandBarPrivate::removeDestroyedAction(QObject *object)
{
    if (object == nullptr) {
        return;
    }
    const auto removeDestroyed = [object](QList<ZzCommandBarActionRecord> *group) {
        bool changed = false;
        for (auto iterator = group->begin(); iterator != group->end();) {
            if (!iterator->action.isNull()
                && iterator->action.data() != object) {
                ++iterator;
                continue;
            }
            QObject::disconnect(iterator->destroyedConnection);
            QObject::disconnect(iterator->changedConnection);
            iterator = group->erase(iterator);
            changed = true;
        }
        return changed;
    };
    if (removeDestroyed(&primaryRecords) || removeDestroyed(&secondaryRecords)) {
        invalidateWidths();
    }
}

ZzCommandBarPrivate::ZzPresentation
ZzCommandBarPrivate::calculatePresentation(int width)
{
    const QList<QAction *> primary = actions(primaryRecords);
    const QList<QAction *> secondary = actions(secondaryRecords);
    const int availableWidth = std::max(0, width);
    const auto totalWidth = [this, &primary, &secondary](bool compact) {
        int result = 0;
        for (QAction *action : primary) {
            result += actionWidth(action, compact);
        }
        if (!primary.isEmpty() && !secondary.isEmpty()) {
            result += separatorWidth();
        }
        for (QAction *action : secondary) {
            result += actionWidth(action, compact);
        }
        return result;
    };

    const bool canExpand = totalWidth(false) <= availableWidth;
    const bool canCompact = totalWidth(true) <= availableWidth;
    bool compact = displayMode == ZzCommandBarDisplayMode::Compact;
    if (displayMode == ZzCommandBarDisplayMode::Auto) {
        compact = !canExpand;
    }
    if (displayMode == ZzCommandBarDisplayMode::Expanded && canExpand) {
        compact = false;
    }
    if (displayMode == ZzCommandBarDisplayMode::Auto && canCompact) {
        return {compact, false, static_cast<int>(primary.size()),
                static_cast<int>(secondary.size())};
    }
    if (displayMode == ZzCommandBarDisplayMode::Expanded && canExpand) {
        return {false, false, static_cast<int>(primary.size()),
                static_cast<int>(secondary.size())};
    }
    if (displayMode == ZzCommandBarDisplayMode::Compact && canCompact) {
        return {true, false, static_cast<int>(primary.size()),
                static_cast<int>(secondary.size())};
    }

    const int menuExtent = moreButton->sizeHint().width();
    int remaining = std::max(0, availableWidth - menuExtent);
    int primaryCount = zzFittingActionCount(
        primaryRecords, remaining, compact, this);
    for (int index = 0; index < primaryCount; ++index) {
        remaining -= actionWidth(primary.at(index), compact);
    }

    int secondaryCount = 0;
    if (primaryCount == primary.size() && !secondary.isEmpty()) {
        remaining -= separatorWidth();
        if (remaining >= 0) {
            secondaryCount = zzFittingActionCount(
                secondaryRecords, remaining, compact, this);
        }
    }
    return {compact, true, primaryCount, secondaryCount};
}

void ZzCommandBarPrivate::moveActionsWithoutCloning(
    const ZzPresentation &presentation)
{
    for (const ZzCommandBarActionRecord &record : primaryRecords) {
        if (QAction *action = record.action.data(); action != nullptr) {
            toolBar->removeAction(action);
            moreMenu->removeAction(action);
        }
    }
    for (const ZzCommandBarActionRecord &record : secondaryRecords) {
        if (QAction *action = record.action.data(); action != nullptr) {
            toolBar->removeAction(action);
            moreMenu->removeAction(action);
        }
    }
    toolBar->removeAction(separatorAction);
    moreMenu->removeAction(overflowSeparatorAction);

    const QList<QAction *> primary = actions(primaryRecords);
    const QList<QAction *> secondary = actions(secondaryRecords);
    for (int index = 0; index < primary.size(); ++index) {
        QAction *action = primary.at(index);
        if (index < presentation.visiblePrimaryCount) {
            toolBar->addAction(action);
        } else {
            moreMenu->addAction(action);
        }
    }
    const bool primaryOverflows = presentation.visiblePrimaryCount < primary.size();
    const bool secondaryOverflows =
        presentation.visibleSecondaryCount < secondary.size();
    if (presentation.visibleSecondaryCount > 0) {
        if (presentation.visiblePrimaryCount > 0) {
            if (separatorAction == nullptr) {
                separatorAction = toolBar->addSeparator();
            } else {
                toolBar->addAction(separatorAction);
            }
        }
        for (int index = 0; index < secondary.size(); ++index) {
            QAction *action = secondary.at(index);
            if (index < presentation.visibleSecondaryCount) {
                toolBar->addAction(action);
            } else {
                if (primaryOverflows && index == presentation.visibleSecondaryCount) {
                    if (overflowSeparatorAction == nullptr) {
                        overflowSeparatorAction = moreMenu->addSeparator();
                    } else {
                        moreMenu->addAction(overflowSeparatorAction);
                    }
                }
                moreMenu->addAction(action);
            }
        }
    } else {
        if (primaryOverflows && secondaryOverflows) {
            if (overflowSeparatorAction == nullptr) {
                overflowSeparatorAction = moreMenu->addSeparator();
            } else {
                moreMenu->addAction(overflowSeparatorAction);
            }
        }
        for (QAction *action : secondary) {
            moreMenu->addAction(action);
        }
    }
}

void ZzCommandBarPrivate::applyToolButtonStyle(bool compact)
{
    toolBar->setToolButtonStyle(
        compact ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
}

void ZzCommandBarPrivate::updateMoreButton(bool hasOverflow)
{
    moreButton->setVisible(hasOverflow);
}

int ZzCommandBarPrivate::actionWidth(QAction *action, bool compact)
{
    if (action == nullptr) {
        return 0;
    }
    if (widthsDirty) {
        expandedWidths.clear();
        compactWidths.clear();
        widthsDirty = false;
    }
    const QList<QAction *> primary = actions(primaryRecords);
    const QList<QAction *> secondary = actions(secondaryRecords);
    QList<QAction *> allActions = primary;
    allActions.append(secondary);
    const qsizetype index = allActions.indexOf(action);
    if (index < 0) {
        return 0;
    }
    QList<QSize> *cache = compact ? &compactWidths : &expandedWidths;
    while (cache->size() <= index) {
        cache->append(QSize());
    }
    QSize &cached = (*cache)[index];
    if (cached.isValid()) {
        return cached.width();
    }

    QStyleOptionToolButton option;
    option.initFrom(q_ptr);
    option.text = action->text();
    option.icon = action->icon();
    option.iconSize = toolBar->iconSize();
    option.toolButtonStyle = compact
        ? Qt::ToolButtonIconOnly
        : Qt::ToolButtonTextBesideIcon;
    const QSize size = q_ptr->style()->sizeFromContents(
        QStyle::CT_ToolButton,
        &option,
        QSize(),
        toolBar);
    cached = QSize(std::max(zzMinimumCommandExtent, size.width()), size.height());
    return cached.width();
}

bool ZzCommandBarPrivate::containsAction(const QAction *action) const
{
    const auto contains = [action](const QList<ZzCommandBarActionRecord> &records) {
        return std::any_of(
            records.cbegin(),
            records.cend(),
            [action](const ZzCommandBarActionRecord &record) {
                return record.action.data() == action;
            });
    };
    return contains(primaryRecords) || contains(secondaryRecords);
}

int ZzCommandBarPrivate::separatorWidth() const
{
    return q_ptr->style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent, nullptr, toolBar);
}

} // namespace ZzFluentUI
