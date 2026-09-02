#include <ZzFluentUI/ZzTabWidget.h>
#include "private/ZzWorkspaceTransferRegistryPrivate.h"

#include "private/ZzTabBarPrivate.h"
#include "private/ZzTabWidgetPrivate.h"

#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>

#include <QtWidgets/QFrame>
#include <QtWidgets/QStackedWidget>

namespace ZzFluentUI {

ZzTabWidget::ZzTabWidget(QWidget *parent)
    : QTabWidget(parent)
    , d_ptr(std::make_unique<ZzTabWidgetPrivate>(this))
{
    d_ptr->tabBar = new ZzTabBar(this);
    d_ptr->tabBar->d_ptr->setHost(this);
    setTabBar(d_ptr->tabBar);
    // 页面区域由工作区自身管理；document mode 禁止 Qt 额外绘制外框。
    setDocumentMode(true);
    if (auto *stack = findChild<QStackedWidget *>()) {
        stack->setFrameShape(QFrame::NoFrame);
    }
    setCornerWidget(d_ptr->tabBar->newTabButton(), Qt::TopRightCorner);
    setMovable(true);
    connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (isTabCloseEnabled(index) && !isTabPinned(index)) {
            Q_EMIT tabsCloseRequested({widget(index)});
        }
    });
    connect(d_ptr->tabBar, &ZzTabBar::newTabRequested, this, &ZzTabWidget::newTabRequested);
    connect(d_ptr->tabBar, &ZzTabBar::closeOtherTabsRequested, this, &ZzTabWidget::closeOtherTabs);
    connect(d_ptr->tabBar, &ZzTabBar::closeTabsToRightRequested, this, &ZzTabWidget::closeTabsToRight);
    connect(d_ptr->tabBar, &QTabBar::tabMoved, this, [this](int, int) { d_ptr->normalizePinnedOrder(); });

    connect(
        d_ptr->tabBar,
        &ZzTabBar::tearOffRequested,
        this,
        [this](int index, const QPoint &globalPosition) {
            QWidget *page = widget(index);
            if (page != nullptr) {
                Q_EMIT tearOffRequested(index, page, globalPosition);
            }
        });
}

bool ZzTabWidget::isTabPinned(int index) const
{
    return index >= 0 && index < count()
        && d_ptr->metadata(widget(index)).pinned;
}

void ZzTabWidget::setTabPinned(int index, bool value)
{
    if (index < 0 || index >= count()) {
        return;
    }
    QWidget *const page = widget(index);
    auto &state = d_ptr->ensureMetadata(page);
    if (state.pinned == value) {
        return;
    }
    state.pinned = value;
    d_ptr->normalizePinnedOrder();
    Q_EMIT tabPinnedChanged(indexOf(page), value);
}

bool ZzTabWidget::isTabModified(int index) const
{
    return index >= 0 && index < count()
        && d_ptr->metadata(widget(index)).modified;
}

void ZzTabWidget::setTabModified(int index, bool value)
{
    if (index < 0 || index >= count()) {
        return;
    }
    auto &state = d_ptr->ensureMetadata(widget(index));
    if (state.modified == value) {
        return;
    }
    state.modified = value;
    Q_EMIT tabModifiedChanged(index, value);
}

bool ZzTabWidget::hasTabAttention(int index) const
{
    return index >= 0 && index < count()
        && d_ptr->metadata(widget(index)).attention;
}

void ZzTabWidget::setTabAttention(int index, bool value)
{
    if (index < 0 || index >= count()) {
        return;
    }
    auto &state = d_ptr->ensureMetadata(widget(index));
    if (state.attention == value) {
        return;
    }
    state.attention = value;
    Q_EMIT tabAttentionChanged(index, value);
}

bool ZzTabWidget::isTabCloseEnabled(int index) const
{
    return index >= 0 && index < count()
        && d_ptr->metadata(widget(index)).closeEnabled;
}

void ZzTabWidget::setTabCloseEnabled(int index, bool value)
{
    if (index < 0 || index >= count()) {
        return;
    }
    auto &state = d_ptr->ensureMetadata(widget(index));
    if (state.closeEnabled == value) {
        return;
    }
    state.closeEnabled = value;
    Q_EMIT tabCloseEnabledChanged(index, value);
}

void ZzTabWidget::setPageTitle(int index, const QString &title)
{
    if (index < 0 || index >= count()) {
        return;
    }
    QWidget *const page = widget(index);
    if (page == nullptr
        || (tabText(index) == title && page->windowTitle() == title)) {
        return;
    }
    setTabText(index, title);
    page->setWindowTitle(title);
    Q_EMIT pagePresentationChanged(page);
}

void ZzTabWidget::setPageTitle(QWidget *page, const QString &title)
{
    const int index = indexOf(page);
    if (index >= 0) {
        setPageTitle(index, title);
    }
}

void ZzTabWidget::closeOtherTabs(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    QList<QWidget *> pages;
    for (int tabIndex = 0; tabIndex < count(); ++tabIndex) {
        if (tabIndex != index && !isTabPinned(tabIndex)
            && isTabCloseEnabled(tabIndex)) {
            pages.push_back(widget(tabIndex));
        }
    }
    if (!pages.isEmpty()) {
        Q_EMIT tabsCloseRequested(pages);
    }
}

void ZzTabWidget::closeTabsToRight(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    QList<QWidget *> pages;
    for (int tabIndex = index + 1; tabIndex < count(); ++tabIndex) {
        if (!isTabPinned(tabIndex) && isTabCloseEnabled(tabIndex)) {
            pages.push_back(widget(tabIndex));
        }
    }
    if (!pages.isEmpty()) {
        Q_EMIT tabsCloseRequested(pages);
    }
}

void ZzTabWidget::tabInserted(int index)
{
    QTabWidget::tabInserted(index);
    if (d_ptr != nullptr && d_ptr->transferInsertionDepth == 0) {
        d_ptr->normalizePinnedOrder();
    }
}

void ZzTabWidget::tabRemoved(int index)
{
    if (d_ptr->removalNotified) d_ptr->removalNotified = false;
    else if (auto *registry = ZzWorkspaceTransferRegistryPrivate::instance(); registry != nullptr)
        registry->sourceTabRemoved(this, index, nullptr);
    QTabWidget::tabRemoved(index);
}

void ZzTabWidget::removeTab(int index)
{
    if (index < 0 || index >= count()) {
        QTabWidget::removeTab(index);
        return;
    }
    QWidget *page = index >= 0 ? widget(index) : nullptr;
    if (auto *registry = ZzWorkspaceTransferRegistryPrivate::instance(); registry != nullptr)
        registry->sourceTabRemoved(this, index, page);
    d_ptr->removalNotified = true;
    QTabWidget::removeTab(index);
}

ZzTabWidget::~ZzTabWidget()
{
    d_ptr->disconnectMetadataObservers();
}

ZzTabBar *ZzTabWidget::fluentTabBar() const noexcept
{
    return d_ptr->tabBar;
}

bool ZzTabWidget::transferTabTo(
    ZzTabWidget *target,
    int sourceIndex,
    int targetIndex)
{
    auto findWorkspace = [](QObject *object) -> ZzSplitWorkspace * {
        for (; object != nullptr; object = object->parent()) {
            if (auto *workspace = qobject_cast<ZzSplitWorkspace *>(object)) {
                return workspace;
            }
        }
        return nullptr;
    };
    auto *const sourceWorkspace = findWorkspace(this);
    auto *const targetWorkspace = findWorkspace(target);
    if (sourceWorkspace != nullptr && targetWorkspace != nullptr
        && sourceWorkspace != targetWorkspace) {
        const auto sourceGroup = sourceWorkspace->groupId(this);
        const auto targetGroup = targetWorkspace->groupId(target);
        return sourceWorkspace->transferTabToWorkspace(
                   sourceGroup, sourceIndex, targetWorkspace, targetGroup,
                   targetIndex)
            .hasValue();
    }
    return d_ptr->transferToDirect(target, sourceIndex, targetIndex);
}

} // namespace ZzFluentUI
