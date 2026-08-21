#include "ZzCommandPalettePrivate.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QHash>
#include <QtCore/QSignalBlocker>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzCommandItemRole.h>
#include <ZzFluentUI/ZzCommandPalette.h>

namespace ZzFluentUI {

namespace {

class ZzCommandFilterProxy final : public QSortFilterProxyModel
{
public:
    QString query;
    void clearCache() const { cache.clear(); }
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (parent.isValid() || query.isEmpty()) return !parent.isValid();
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        const ZzCachedData &data = cached(index);
        if (data.name.contains(query)) return true;
        for (const QString &keyword : data.keywords)
            if (keyword.contains(query)) return true;
        return false;
    }
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        return rank(left) < rank(right);
    }
private:
    struct ZzCachedData {
        QString name;
        QStringList keywords;
        int priority = 0;
    };
    [[nodiscard]] const ZzCachedData &cached(const QModelIndex &index) const
    {
        const int row = index.row();
        const auto found = cache.constFind(row);
        if (found != cache.constEnd()) return found.value();
        ZzCachedData data;
        data.name = index.data(Qt::DisplayRole).toString().toCaseFolded();
        const QStringList values = index.data(static_cast<int>(ZzCommandItemRole::Keywords)).toStringList();
        data.keywords.reserve(values.size());
        for (const QString &value : values) data.keywords.push_back(value.toCaseFolded());
        data.priority = index.data(static_cast<int>(ZzCommandItemRole::Priority)).toInt();
        return cache.insert(row, std::move(data)).value();
    }
    [[nodiscard]] std::tuple<int, int, int> rank(const QModelIndex &index) const
    {
        const ZzCachedData &data = cached(index);
        const QString &name = data.name;
        int match = 5;
        if (query.isEmpty()) match = 0;
        else if (name == query) match = 0;
        else if (name.startsWith(query)) match = 1;
        else {
            const QStringList tokens = name.split(u' ', Qt::SkipEmptyParts);
            for (const QString &token : tokens) if (token.startsWith(query)) { match = 2; break; }
            if (match == 5 && name.contains(query)) match = 3;
            if (match == 5) match = 4;
        }
        return {match, -data.priority, index.row()};
    }
    mutable QHash<int, ZzCachedData> cache;
};

} // namespace

ZzCommandPalettePrivate::ZzCommandPalettePrivate(ZzCommandPalette *q)
    : q_ptr(q)
{
    q_ptr->setWindowFlags(Qt::Widget);
    q_ptr->setVisible(false);
    auto *layout = new QVBoxLayout(q_ptr);
    searchEdit = new QLineEdit(q_ptr);
    searchEdit->setPlaceholderText(QObject::tr("Search commands"));
    resultView = new QListView(q_ptr);
    resultView->setUniformItemSizes(true);
    proxy = new ZzCommandFilterProxy;
    proxy->setParent(q_ptr);
    proxy->setDynamicSortFilter(false);
    resultView->setModel(proxy);
    layout->addWidget(searchEdit);
    layout->addWidget(resultView, 1);
    searchEdit->installEventFilter(q_ptr);
    resultView->installEventFilter(q_ptr);
    q_ptr->installEventFilter(q_ptr);
    QObject::connect(searchEdit, &QLineEdit::textChanged, q_ptr, [this](const QString &text) { setQuery(text); });
    QObject::connect(resultView, &QListView::activated, q_ptr, [q] { static_cast<void>(q->activateCurrent()); });
}

void ZzCommandPalettePrivate::setModel(QAbstractItemModel *model)
{
    if (sourceModel == model) return;
    for (const QMetaObject::Connection &connection : modelConnections) {
        QObject::disconnect(connection);
    }
    modelConnections.clear();
    sourceModel = model;
    proxy->setSourceModel(model);
    static_cast<ZzCommandFilterProxy *>(proxy)->clearCache();
    proxy->sort(0);
    if (model != nullptr) {
        const auto refresh = [this, model] {
            if (sourceModel.data() != model) {
                return;
            }
                static_cast<ZzCommandFilterProxy *>(proxy)->clearCache();
                proxy->invalidate();
                proxy->sort(0);
        };
        modelConnections.append(QObject::connect(
            model, &QAbstractItemModel::dataChanged, q_ptr,
            [refresh](const QModelIndex &, const QModelIndex &, const QList<int> &) {
                refresh();
            }));
        modelConnections.append(QObject::connect(
            model, &QAbstractItemModel::rowsInserted, q_ptr,
            [refresh](const QModelIndex &, int, int) { refresh(); }));
        modelConnections.append(QObject::connect(
            model, &QAbstractItemModel::rowsRemoved, q_ptr,
            [refresh](const QModelIndex &, int, int) { refresh(); }));
        modelConnections.append(QObject::connect(
            model, &QAbstractItemModel::layoutChanged, q_ptr,
            [refresh] { refresh(); }));
        modelConnections.append(QObject::connect(
            model, &QAbstractItemModel::modelReset, q_ptr,
            [refresh] { refresh(); }));
        modelConnections.append(QObject::connect(
            model, &QObject::destroyed, q_ptr, [this] {
                sourceModel = nullptr;
                proxy->setSourceModel(nullptr);
                modelConnections.clear();
                Q_EMIT q_ptr->modelChanged(nullptr);
            }));
    }
    Q_EMIT q_ptr->modelChanged(model);
}

void ZzCommandPalettePrivate::setQuery(const QString &value)
{
    const QString limited = value.left(512);
    if (query == limited) return;
    if (searchEdit->text() != limited) {
        const QSignalBlocker blocker(searchEdit);
        searchEdit->setText(limited);
    }
    query = limited;
    static_cast<ZzCommandFilterProxy *>(proxy)->query = query.toCaseFolded();
    proxy->invalidate();
    proxy->sort(0);
    if (proxy->rowCount() > 0) resultView->setCurrentIndex(proxy->index(0, 0));
    Q_EMIT q_ptr->queryChanged(query);
}

void ZzCommandPalettePrivate::restoreFocus()
{
    if (previousFocus != nullptr && previousFocus->isVisible()) previousFocus->setFocus();
    previousFocus = nullptr;
}

void ZzCommandPalettePrivate::syncGeometry()
{
    if (q_ptr->parentWidget() != nullptr) {
        q_ptr->setGeometry(q_ptr->parentWidget()->rect());
        q_ptr->parentWidget()->installEventFilter(q_ptr);
    }
}

} // namespace ZzFluentUI
