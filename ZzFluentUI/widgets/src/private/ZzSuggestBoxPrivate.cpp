#include "ZzSuggestBoxPrivate.h"

#include <limits>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QModelIndex>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>

#include <ZzFluentUI/ZzFluentItemDelegate.h>

namespace ZzFluentUI {

namespace {

constexpr int ZzSuggestionKeyRole = Qt::UserRole + 1;
constexpr int ZzSuggestionPayloadRole = Qt::UserRole + 2;
constexpr int ZzSuggestionEnabledRole = Qt::UserRole + 3;

/** @brief 生成当前集合内唯一、无花括号的建议键。 */
QString zzUniqueSuggestionKey(
    const QString &requested,
    QSet<QString> *usedKeys)
{
    Q_ASSERT(usedKeys != nullptr);
    if (!requested.isEmpty() && !usedKeys->contains(requested)) {
        usedKeys->insert(requested);
        return requested;
    }

    QString generated;
    do {
        generated = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } while (usedKeys->contains(generated));
    usedKeys->insert(generated);
    return generated;
}

} // namespace

/** @brief 只保存 ZzSuggestion 值快照的轻量单列列表模型。 */
class ZzSuggestionListModel final : public QAbstractListModel
{
public:
    /** @brief 创建空建议模型。 */
    explicit ZzSuggestionListModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    /** @brief 返回顶层建议行数，有效 parent 没有子项。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(items_.size());
    }

    /** @brief 按标准展示 role 和私有载荷 role 返回当前行数据。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.column() != 0
            || index.row() < 0 || index.row() >= rowCount()) {
            return {};
        }
        const ZzSuggestion &item = items_.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return item.text;
        case Qt::DecorationRole:
            return item.icon;
        case ZzSuggestionKeyRole:
            return item.key;
        case ZzSuggestionPayloadRole:
            return item.data;
        case ZzSuggestionEnabledRole:
            return item.enabled;
        default:
            return {};
        }
    }

    /** @brief 返回当前行是否允许被 completer 选中和激活。 */
    [[nodiscard]] Qt::ItemFlags flags(
        const QModelIndex &index) const override
    {
        Qt::ItemFlags result = QAbstractListModel::flags(index);
        if (!index.isValid() || index.row() < 0
            || index.row() >= rowCount()
            || !items_.at(index.row()).enabled) {
            result.setFlag(Qt::ItemIsEnabled, false);
            result.setFlag(Qt::ItemIsSelectable, false);
        }
        return result;
    }

    /** @brief 一次 reset 全部值，并规范化每一条唯一键。 */
    void setItems(QList<ZzSuggestion> items)
    {
        const qsizetype maximumRows =
            static_cast<qsizetype>(std::numeric_limits<int>::max());
        if (items.size() > maximumRows) {
            items.resize(maximumRows);
        }
        QSet<QString> usedKeys;
        usedKeys.reserve(items.size());
        for (ZzSuggestion &item : items) {
            item.key = zzUniqueSuggestionKey(item.key, &usedKeys);
        }
        beginResetModel();
        items_ = std::move(items);
        endResetModel();
    }

    /** @brief 返回当前值集合副本。 */
    [[nodiscard]] QList<ZzSuggestion> items() const
    {
        return items_;
    }

    /** @brief 追加单条值并返回实际唯一键。 */
    [[nodiscard]] QString append(ZzSuggestion item)
    {
        if (items_.size()
            >= static_cast<qsizetype>(std::numeric_limits<int>::max())) {
            return {};
        }
        QSet<QString> usedKeys;
        usedKeys.reserve(items_.size() + 1);
        for (const ZzSuggestion &existing : std::as_const(items_)) {
            usedKeys.insert(existing.key);
        }
        item.key = zzUniqueSuggestionKey(item.key, &usedKeys);
        const int row = rowCount();
        beginInsertRows(QModelIndex(), row, row);
        items_.append(std::move(item));
        endInsertRows();
        return items_.constLast().key;
    }

    /** @brief 按唯一键删除首个且唯一的匹配项。 */
    [[nodiscard]] bool removeByKey(const QString &key)
    {
        if (key.isEmpty()) {
            return false;
        }
        for (int row = 0; row < rowCount(); ++row) {
            if (items_.at(row).key == key) {
                return removeAt(row);
            }
        }
        return false;
    }

    /** @brief 按行号删除建议，拒绝负数和越界值。 */
    [[nodiscard]] bool removeAt(int row)
    {
        if (row < 0 || row >= rowCount()) {
            return false;
        }
        beginRemoveRows(QModelIndex(), row, row);
        items_.removeAt(row);
        endRemoveRows();
        return true;
    }

    /** @brief 清空非空模型并发送完整 reset 通知。 */
    [[nodiscard]] bool clear()
    {
        if (items_.isEmpty()) {
            return false;
        }
        beginResetModel();
        items_.clear();
        endResetModel();
        return true;
    }

private:
    QList<ZzSuggestion> items_;
};

ZzSuggestBoxPrivate::ZzSuggestBoxPrivate(ZzSuggestBox *q)
    : q_ptr(q)
    , model(new ZzSuggestionListModel(q))
    , completer(new QCompleter(model, q))
    , popup(new QListView)
    , delegate(new ZzFluentItemDelegate(popup))
{
    Q_ASSERT(q_ptr != nullptr);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setModelSorting(QCompleter::UnsortedModel);
    completer->setCompletionRole(Qt::EditRole);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setMaxVisibleItems(8);
    completer->setWrapAround(true);

    popup->setItemDelegate(delegate);
    popup->setSelectionMode(QAbstractItemView::SingleSelection);
    popup->setEditTriggers(QAbstractItemView::NoEditTriggers);
    popup->setUniformItemSizes(true);
    popup->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    popup->setTextElideMode(Qt::ElideRight);
    completer->setPopup(popup);

    q_ptr->setClearButtonEnabled(true);
}

void ZzSuggestBoxPrivate::setSuggestions(QList<ZzSuggestion> suggestions)
{
    model->setItems(std::move(suggestions));
    refreshVisiblePopup();
}

QList<ZzSuggestion> ZzSuggestBoxPrivate::suggestions() const
{
    return model->items();
}

int ZzSuggestBoxPrivate::suggestionCount() const noexcept
{
    return model->rowCount();
}

QString ZzSuggestBoxPrivate::addSuggestion(ZzSuggestion suggestion)
{
    const QString key = model->append(std::move(suggestion));
    refreshVisiblePopup();
    return key;
}

bool ZzSuggestBoxPrivate::removeSuggestion(const QString &key)
{
    const bool removed = model->removeByKey(key);
    if (removed) {
        refreshVisiblePopup();
    }
    return removed;
}

bool ZzSuggestBoxPrivate::removeSuggestionAt(int index)
{
    const bool removed = model->removeAt(index);
    if (removed) {
        refreshVisiblePopup();
    }
    return removed;
}

bool ZzSuggestBoxPrivate::clearSuggestions()
{
    const bool cleared = model->clear();
    if (cleared) {
        popup->hide();
    }
    return cleared;
}

ZzSuggestion ZzSuggestBoxPrivate::suggestionFromIndex(
    const QModelIndex &index)
{
    if (!index.isValid()) {
        return {};
    }
    return {
        index.data(ZzSuggestionKeyRole).toString(),
        index.data(Qt::EditRole).toString(),
        qvariant_cast<QIcon>(index.data(Qt::DecorationRole)),
        index.data(ZzSuggestionPayloadRole),
        index.data(ZzSuggestionEnabledRole).toBool()};
}

bool ZzSuggestBoxPrivate::isSupportedFilterMode(
    Qt::MatchFlag mode) noexcept
{
    return mode == Qt::MatchStartsWith
        || mode == Qt::MatchContains
        || mode == Qt::MatchEndsWith;
}

void ZzSuggestBoxPrivate::refreshVisiblePopup()
{
    if (!popup->isVisible()) {
        return;
    }
    completer->setCompletionPrefix(q_ptr->text());
    if (model->rowCount() == 0) {
        popup->hide();
        return;
    }
    completer->complete();
}

} // namespace ZzFluentUI
