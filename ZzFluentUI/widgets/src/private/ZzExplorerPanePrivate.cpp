#include "ZzExplorerPanePrivate.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTimer>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzExplorerPane.h>

namespace ZzFluentUI {

namespace {

class ZzRecursiveFilterProxy final : public QSortFilterProxyModel
{
public:
    QString query;
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (query.isEmpty()) {
            return true;
        }
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        const QString value = index.data(Qt::DisplayRole).toString().toCaseFolded();
        if (value == query || value.startsWith(query) || value.contains(query)) {
            return true;
        }
        const int childCount = sourceModel()->rowCount(index);
        for (int child = 0; child < childCount; ++child) {
            if (filterAcceptsRow(child, index)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace

ZzExplorerPanePrivate::ZzExplorerPanePrivate(ZzExplorerPane *q)
    : q_ptr(q)
{
    auto *layout = new QVBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QObject::tr("Explorer"), q_ptr);
    toolBar = new QToolBar(q_ptr);
    searchEdit = new QLineEdit(q_ptr);
    searchEdit->setPlaceholderText(QObject::tr("Search"));
    treeView = new QTreeView(q_ptr);
    treeView->setUniformRowHeights(true);
    proxy = new ZzRecursiveFilterProxy;
    proxy->setParent(q_ptr);
    treeView->setModel(proxy);
    timer = new QTimer(q_ptr);
    timer->setSingleShot(true);
    layout->addWidget(title);
    layout->addWidget(toolBar);
    layout->addWidget(searchEdit);
    layout->addWidget(treeView, 1);
    QObject::connect(timer, &QTimer::timeout, q_ptr, [this] { applySearch(); });
    QObject::connect(searchEdit, &QLineEdit::textChanged, q_ptr, [this](const QString &value) {
        text = value;
        Q_EMIT q_ptr->searchTextChanged(text);
        if (delay == 0) { applySearch(); } else { timer->start(delay); }
    });
    QObject::connect(treeView, &QTreeView::activated, q_ptr, [this](const QModelIndex &index) {
        Q_EMIT q_ptr->activated(proxy->mapToSource(index));
    });
    QObject::connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged, q_ptr,
        [this](const QModelIndex &current) { Q_EMIT q_ptr->currentSourceIndexChanged(proxy->mapToSource(current)); });
}

void ZzExplorerPanePrivate::setModel(QAbstractItemModel *model)
{
    if (sourceModel == model) return;
    sourceModel = model;
    proxy->setSourceModel(model);
    if (model != nullptr) {
        QObject::connect(model, &QObject::destroyed, q_ptr, [this, model] {
            if (sourceModel.data() != model) {
                return;
            }
            sourceModel = nullptr;
            proxy->setSourceModel(nullptr);
            treeView->clearSelection();
            Q_EMIT q_ptr->modelChanged(nullptr);
        });
    }
    Q_EMIT q_ptr->modelChanged(model);
}

void ZzExplorerPanePrivate::applySearch()
{
    auto *filter = static_cast<ZzRecursiveFilterProxy *>(proxy);
    const QString normalized = text.toCaseFolded();
    if (filter->query == normalized) return;
    filter->query = normalized;
    filter->invalidate();
}

} // namespace ZzFluentUI
