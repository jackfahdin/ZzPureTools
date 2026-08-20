#include "ZzCommandPalettePrivate.h"

#include <QtCore/QAbstractItemModel>
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
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (parent.isValid() || query.isEmpty()) return !parent.isValid();
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        const QString name = index.data(Qt::DisplayRole).toString().toCaseFolded();
        if (name.contains(query)) return true;
        const QStringList keywords = index.data(static_cast<int>(ZzCommandItemRole::Keywords)).toStringList();
        for (const QString &keyword : keywords) {
            if (keyword.toCaseFolded().contains(query)) return true;
        }
        return false;
    }
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        return rank(left) < rank(right);
    }
private:
    [[nodiscard]] std::tuple<int, int, int> rank(const QModelIndex &index) const
    {
        const QString name = index.data(Qt::DisplayRole).toString().toCaseFolded();
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
        const int priority = index.data(static_cast<int>(ZzCommandItemRole::Priority)).toInt();
        return {match, -priority, index.row()};
    }
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
    sourceModel = model;
    proxy->setSourceModel(model);
    proxy->sort(0);
    if (model != nullptr) {
        QObject::connect(model, &QAbstractItemModel::dataChanged, q_ptr,
            [this](const QModelIndex &, const QModelIndex &, const QList<int> &) {
                proxy->invalidate();
                proxy->sort(0);
            });
        QObject::connect(model, &QAbstractItemModel::modelReset, q_ptr, [this] {
            proxy->invalidate();
            proxy->sort(0);
        });
        QObject::connect(model, &QObject::destroyed, q_ptr, [this] {
            sourceModel = nullptr;
            proxy->setSourceModel(nullptr);
            Q_EMIT q_ptr->modelChanged(nullptr);
        });
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
