#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>

class QAbstractItemModel;
class QLineEdit;
class QListView;
class QSortFilterProxyModel;
class QWidget;

namespace ZzFluentUI {

class ZzCommandPalette;

/** @brief 保存命令覆盖层固定控件、代理和焦点恢复状态。 */
class ZzCommandPalettePrivate final
{
public:
    explicit ZzCommandPalettePrivate(ZzCommandPalette *q);
    void setModel(QAbstractItemModel *model);
    void setQuery(const QString &query);
    void restoreFocus();
    void syncGeometry();
    ZzCommandPalette *const q_ptr;
    QPointer<QAbstractItemModel> sourceModel;
    QPointer<QWidget> previousFocus;
    QLineEdit *searchEdit = nullptr;
    QListView *resultView = nullptr;
    QSortFilterProxyModel *proxy = nullptr;
    QList<QMetaObject::Connection> modelConnections;
    QString query;
    bool opened = false;
};

} // namespace ZzFluentUI
