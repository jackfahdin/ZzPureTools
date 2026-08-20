#include <ZzFluentUI/ZzTabWidget.h>

#include "private/ZzTabBarPrivate.h"
#include "private/ZzTabWidgetPrivate.h"

#include <ZzFluentUI/ZzTabBar.h>

namespace ZzFluentUI {

ZzTabWidget::ZzTabWidget(QWidget *parent)
    : QTabWidget(parent)
    , d_ptr(std::make_unique<ZzTabWidgetPrivate>(this))
{
    d_ptr->tabBar = new ZzTabBar(this);
    d_ptr->tabBar->d_ptr->setHost(this);
    setTabBar(d_ptr->tabBar);
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
    setTabText(index, title);
    if (QWidget *const page = widget(index); page != nullptr) {
        page->setWindowTitle(title);
    }
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
    return d_ptr->transferTo(target, sourceIndex, targetIndex);
}

} // namespace ZzFluentUI
