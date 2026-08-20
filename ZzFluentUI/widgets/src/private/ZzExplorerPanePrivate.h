#pragma once

#include <QtCore/QPointer>

class QAbstractItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTimer;
class QToolBar;
class QTreeView;

namespace ZzFluentUI {

class ZzExplorerPane;

/** @brief 保存资源浏览面板固定对象和外部模型观察状态。 */
class ZzExplorerPanePrivate final
{
public:
    explicit ZzExplorerPanePrivate(ZzExplorerPane *q);
    void setModel(QAbstractItemModel *model);
    void applySearch();
    ZzExplorerPane *const q_ptr;
    QPointer<QAbstractItemModel> sourceModel;
    QToolBar *toolBar = nullptr;
    QLineEdit *searchEdit = nullptr;
    QTreeView *treeView = nullptr;
    QSortFilterProxyModel *proxy = nullptr;
    QTimer *timer = nullptr;
    int delay = 60;
    QString text;
};

} // namespace ZzFluentUI
