#include "ZzExampleActivityModel.h"

#include <algorithm>
#include <limits>

#include "ZzExampleActivityModelPrivate.h"

namespace ZzExample {

ZzExampleActivityModel::ZzExampleActivityModel(
    qsizetype capacity,
    QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(std::make_unique<ZzExampleActivityModelPrivate>(
          std::clamp<qsizetype>(
              capacity,
              1,
              std::numeric_limits<int>::max())))
{
}

ZzExampleActivityModel::~ZzExampleActivityModel() = default;

int ZzExampleActivityModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid()
        ? 0
        : static_cast<int>(d_ptr->rows.size());
}

QVariant ZzExampleActivityModel::data(
    const QModelIndex &index,
    int role) const
{
    if (!index.isValid() || index.parent().isValid()
        || index.column() != 0 || index.row() < 0
        || index.row() >= d_ptr->rows.size()) {
        return {};
    }
    if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole
        || role == Qt::ToolTipRole) {
        return d_ptr->rows.at(index.row());
    }
    return {};
}

void ZzExampleActivityModel::append(QString text)
{
    text = text.simplified();
    if (text.isEmpty()) {
        return;
    }
    if (d_ptr->rows.size() >= d_ptr->capacity) {
        beginRemoveRows(QModelIndex(), 0, 0);
        d_ptr->rows.removeFirst();
        endRemoveRows();
    }
    const int row = static_cast<int>(d_ptr->rows.size());
    beginInsertRows(QModelIndex(), row, row);
    d_ptr->rows.append(std::move(text));
    endInsertRows();
}

void ZzExampleActivityModel::clear()
{
    if (d_ptr->rows.isEmpty()) {
        return;
    }
    beginResetModel();
    d_ptr->rows.clear();
    endResetModel();
}

} // namespace ZzExample
