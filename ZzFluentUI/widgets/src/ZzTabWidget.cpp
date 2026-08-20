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
    setMovable(true);
    connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (isTabCloseEnabled(index)) Q_EMIT tabsCloseRequested({widget(index)});
    });
    connect(d_ptr->tabBar, &ZzTabBar::newTabRequested, this, &ZzTabWidget::newTabRequested);

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

bool ZzTabWidget::isTabPinned(int index) const { return index >= 0 && index < count() && d_ptr->metadata(widget(index)).pinned; }
void ZzTabWidget::setTabPinned(int index, bool value) { if(index<0||index>=count()) return; QWidget *page=widget(index); auto &s=d_ptr->ensureMetadata(page); if(s.pinned==value)return; s.pinned=value; int actual=index; if(value){ int firstUnpinned=0; while(firstUnpinned<count()&&isTabPinned(firstUnpinned)) ++firstUnpinned; if(index>firstUnpinned) { d_ptr->tabBar->moveTab(index,firstUnpinned); actual=firstUnpinned; } } Q_EMIT tabPinnedChanged(actual,value); }
bool ZzTabWidget::isTabModified(int index) const { return index >= 0 && index < count() && d_ptr->metadata(widget(index)).modified; }
void ZzTabWidget::setTabModified(int index, bool value) { if(index<0||index>=count()) return; auto &s=d_ptr->ensureMetadata(widget(index)); if(s.modified==value)return; s.modified=value; Q_EMIT tabModifiedChanged(index,value); }
bool ZzTabWidget::hasTabAttention(int index) const { return index >= 0 && index < count() && d_ptr->metadata(widget(index)).attention; }
void ZzTabWidget::setTabAttention(int index, bool value) { if(index<0||index>=count()) return; auto &s=d_ptr->ensureMetadata(widget(index)); if(s.attention==value)return; s.attention=value; Q_EMIT tabAttentionChanged(index,value); }
bool ZzTabWidget::isTabCloseEnabled(int index) const { return index >= 0 && index < count() && d_ptr->metadata(widget(index)).closeEnabled; }
void ZzTabWidget::setTabCloseEnabled(int index, bool value) { if(index<0||index>=count()) return; auto &s=d_ptr->ensureMetadata(widget(index)); if(s.closeEnabled==value)return; s.closeEnabled=value; Q_EMIT tabCloseEnabledChanged(index,value); }
void ZzTabWidget::setPageTitle(int index, const QString &title) { if(index<0||index>=count()) return; setTabText(index,title); if(auto *p=widget(index)) p->setWindowTitle(title); }
void ZzTabWidget::setPageTitle(QWidget *page, const QString &title) { const int i=indexOf(page); if(i>=0)setPageTitle(i,title); }
void ZzTabWidget::closeOtherTabs(int index) { if(index<0||index>=count()) return; QList<QWidget*> pages; for(int i=0;i<count();++i) if(i!=index&&!isTabPinned(i)&&isTabCloseEnabled(i)) pages.push_back(widget(i)); if(!pages.isEmpty()) Q_EMIT tabsCloseRequested(pages); }
void ZzTabWidget::closeTabsToRight(int index) { if(index<0||index>=count()) return; QList<QWidget*> pages; for(int i=index+1;i<count();++i) if(!isTabPinned(i)&&isTabCloseEnabled(i)) pages.push_back(widget(i)); if(!pages.isEmpty()) Q_EMIT tabsCloseRequested(pages); }

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
