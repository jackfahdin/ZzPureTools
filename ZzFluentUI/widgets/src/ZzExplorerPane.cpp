#include <ZzFluentUI/ZzExplorerPane.h>

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>

#include "private/ZzExplorerPanePrivate.h"

namespace ZzFluentUI {

ZzExplorerPane::ZzExplorerPane(QWidget *parent) : QWidget(parent), d_ptr(std::make_unique<ZzExplorerPanePrivate>(this)) {}
ZzExplorerPane::~ZzExplorerPane() = default;
void ZzExplorerPane::setModel(QAbstractItemModel *model) { d_ptr->setModel(model); }
QAbstractItemModel *ZzExplorerPane::model() const noexcept { return d_ptr->sourceModel.data(); }
QToolBar *ZzExplorerPane::toolBar() const noexcept { return d_ptr->toolBar; }
QTreeView *ZzExplorerPane::treeView() const noexcept { return d_ptr->treeView; }
void ZzExplorerPane::setSearchDelay(int milliseconds) { const int bounded = std::clamp(milliseconds, 0, 500); if (d_ptr->delay == bounded) return; d_ptr->delay = bounded; Q_EMIT searchDelayChanged(bounded); }
int ZzExplorerPane::searchDelay() const noexcept { return d_ptr->delay; }
void ZzExplorerPane::setSearchText(const QString &text) { d_ptr->searchEdit->setText(text); }
QString ZzExplorerPane::searchText() const { return d_ptr->text; }
QModelIndex ZzExplorerPane::sourceIndex(const QModelIndex &index) const { return d_ptr->proxy->mapToSource(index); }
QModelIndex ZzExplorerPane::proxyIndex(const QModelIndex &index) const { return d_ptr->proxy->mapFromSource(index); }
QModelIndex ZzExplorerPane::currentSourceIndex() const { return sourceIndex(d_ptr->treeView->currentIndex()); }
void ZzExplorerPane::setCurrentSourceIndex(const QModelIndex &index) { d_ptr->treeView->setCurrentIndex(proxyIndex(index)); }

} // namespace ZzFluentUI
