#include "ZzMultiSelectComboBoxPrivate.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QEvent>
#include <QtCore/QModelIndex>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QUuid>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>

#include <ZzFluentUI/ZzFluentItemDelegate.h>

namespace ZzFluentUI {

namespace {

/** @brief 生成当前集合内唯一、无花括号的选项键。 */
QString zzUniqueMultiSelectKey(
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

/** @brief 只保存 ZzMultiSelectOption 值快照的单列列表模型。 */
class ZzMultiSelectOptionModel final : public QAbstractListModel
{
public:
    /** @brief 创建空选项模型。 */
    explicit ZzMultiSelectOptionModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    /** @brief 返回顶层选项行数，有效 parent 没有子项。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(items_.size());
    }

    /** @brief 按标准展示、选择和稳定键 role 返回当前行数据。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!isValidRow(index.row()) || !index.isValid()
            || index.column() != 0) {
            return {};
        }
        const ZzMultiSelectOption &option = items_.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return option.text;
        case Qt::DecorationRole:
            return option.icon;
        case Qt::CheckStateRole:
            return option.selected ? Qt::Checked : Qt::Unchecked;
        case Qt::UserRole:
            return option.data;
        case ZzMultiSelectComboBox::KeyRole:
            return option.key;
        default:
            return {};
        }
    }

    /** @brief 返回 enabled 行的 selectable 与 user-checkable 标志。 */
    [[nodiscard]] Qt::ItemFlags flags(
        const QModelIndex &index) const override
    {
        if (!index.isValid() || !isValidRow(index.row())) {
            return Qt::NoItemFlags;
        }
        Qt::ItemFlags result = Qt::ItemIsSelectable;
        if (items_.at(index.row()).enabled) {
            result |= Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
        }
        return result;
    }

    /** @brief 允许标准 view 通过 CheckStateRole 切换一行选择。 */
    bool setData(
        const QModelIndex &index,
        const QVariant &value,
        int role) override
    {
        if (role != Qt::CheckStateRole || !index.isValid()) {
            return false;
        }
        const int checkState = value.toInt();
        if (checkState != Qt::Checked && checkState != Qt::Unchecked) {
            return false;
        }
        return setSelectedAt(index.row(), checkState == Qt::Checked);
    }

    /** @brief 规范化键后一次 reset 全部选项。 */
    [[nodiscard]] bool setItems(QList<ZzMultiSelectOption> items)
    {
        const qsizetype maximumRows =
            static_cast<qsizetype>(std::numeric_limits<int>::max());
        if (items.size() > maximumRows) {
            items.resize(maximumRows);
        }
        QSet<QString> usedKeys;
        usedKeys.reserve(items.size());
        for (ZzMultiSelectOption &option : items) {
            option.key = zzUniqueMultiSelectKey(option.key, &usedKeys);
        }
        if (items_ == items) {
            return false;
        }
        beginResetModel();
        items_ = std::move(items);
        endResetModel();
        return true;
    }

    /** @brief 返回全部选项副本。 */
    [[nodiscard]] QList<ZzMultiSelectOption> items() const
    {
        return items_;
    }

    /** @brief 追加选项并返回实际唯一键。 */
    [[nodiscard]] QString append(ZzMultiSelectOption option)
    {
        if (items_.size()
            >= static_cast<qsizetype>(std::numeric_limits<int>::max())) {
            return {};
        }
        QSet<QString> usedKeys;
        usedKeys.reserve(items_.size() + 1);
        for (const ZzMultiSelectOption &existing : std::as_const(items_)) {
            usedKeys.insert(existing.key);
        }
        option.key = zzUniqueMultiSelectKey(option.key, &usedKeys);
        const QString key = option.key;
        const int row = rowCount();
        beginInsertRows(QModelIndex(), row, row);
        items_.append(std::move(option));
        endInsertRows();
        return key;
    }

    /** @brief 按唯一键删除选项。 */
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

    /** @brief 按模型行删除选项。 */
    [[nodiscard]] bool removeAt(int row)
    {
        if (!isValidRow(row)) {
            return false;
        }
        beginRemoveRows(QModelIndex(), row, row);
        items_.removeAt(row);
        endRemoveRows();
        return true;
    }

    /** @brief 清空非空模型。 */
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

    /** @brief 按唯一键改变一行选择。 */
    [[nodiscard]] bool setSelectedByKey(
        const QString &key,
        bool selected)
    {
        if (key.isEmpty()) {
            return false;
        }
        for (int row = 0; row < rowCount(); ++row) {
            if (items_.at(row).key == key) {
                return setSelectedAt(row, selected);
            }
        }
        return false;
    }

    /** @brief 按模型行改变选择并发出单行 role 更新。 */
    [[nodiscard]] bool setSelectedAt(int row, bool selected)
    {
        if (!isValidRow(row) || items_.at(row).selected == selected) {
            return false;
        }
        items_[row].selected = selected;
        const QModelIndex changed = index(row, 0);
        Q_EMIT dataChanged(changed, changed, {Qt::CheckStateRole});
        return true;
    }

    /** @brief 按稳定键集合精确替换全部选择。 */
    [[nodiscard]] bool setSelectedKeys(const QStringList &keys)
    {
        QSet<QString> requested;
        requested.reserve(keys.size());
        for (const QString &key : keys) {
            if (!key.isEmpty()) {
                requested.insert(key);
            }
        }
        return applySelection([&requested](
                                  const ZzMultiSelectOption &option,
                                  int) {
            return requested.contains(option.key);
        });
    }

    /** @brief 按模型行集合精确替换全部选择。 */
    [[nodiscard]] bool setSelectedIndexes(const QList<int> &indexes)
    {
        QSet<int> requested;
        requested.reserve(indexes.size());
        for (const int index : indexes) {
            if (isValidRow(index)) {
                requested.insert(index);
            }
        }
        return applySelection([&requested](
                                  const ZzMultiSelectOption &,
                                  int row) {
            return requested.contains(row);
        });
    }

    /** @brief 选中全部 enabled 选项并保留 disabled 项原状态。 */
    [[nodiscard]] bool selectAllEnabled()
    {
        return applySelection([](
                                  const ZzMultiSelectOption &option,
                                  int) {
            return option.enabled ? true : option.selected;
        });
    }

    /** @brief 清除全部选择。 */
    [[nodiscard]] bool clearSelection()
    {
        return applySelection([](const ZzMultiSelectOption &, int) {
            return false;
        });
    }

    /** @brief 返回模型顺序下全部已选选项。 */
    [[nodiscard]] QList<ZzMultiSelectOption> selectedItems() const
    {
        QList<ZzMultiSelectOption> selected;
        selected.reserve(items_.size());
        for (const ZzMultiSelectOption &option : items_) {
            if (option.selected) {
                selected.append(option);
            }
        }
        return selected;
    }

    /** @brief 返回模型顺序下全部已选稳定键。 */
    [[nodiscard]] QStringList selectedKeys() const
    {
        QStringList keys;
        keys.reserve(items_.size());
        for (const ZzMultiSelectOption &option : items_) {
            if (option.selected) {
                keys.append(option.key);
            }
        }
        return keys;
    }

    /** @brief 返回模型顺序下全部已选行。 */
    [[nodiscard]] QList<int> selectedIndexes() const
    {
        QList<int> indexes;
        indexes.reserve(items_.size());
        for (int row = 0; row < rowCount(); ++row) {
            if (items_.at(row).selected) {
                indexes.append(row);
            }
        }
        return indexes;
    }

    /** @brief 返回已选项数量。 */
    [[nodiscard]] int selectionCount() const noexcept
    {
        int result = 0;
        for (const ZzMultiSelectOption &option : items_) {
            result += option.selected ? 1 : 0;
        }
        return result;
    }

    /** @brief 返回有效行快照，非法行返回空选项。 */
    [[nodiscard]] ZzMultiSelectOption itemAt(int row) const
    {
        return isValidRow(row) ? items_.at(row) : ZzMultiSelectOption{};
    }

private:
    /** @brief 返回行号是否位于当前模型范围。 */
    [[nodiscard]] bool isValidRow(int row) const noexcept
    {
        return row >= 0 && row < rowCount();
    }

    /** @brief 一次扫描应用选择谓词并合并 dataChanged。 */
    template<typename Predicate>
    [[nodiscard]] bool applySelection(Predicate predicate)
    {
        int firstChanged = rowCount();
        int lastChanged = -1;
        for (int row = 0; row < rowCount(); ++row) {
            const bool selected = predicate(items_.at(row), row);
            if (items_.at(row).selected == selected) {
                continue;
            }
            items_[row].selected = selected;
            firstChanged = std::min(firstChanged, row);
            lastChanged = row;
        }
        if (lastChanged < firstChanged) {
            return false;
        }
        Q_EMIT dataChanged(
            index(firstChanged, 0),
            index(lastChanged, 0),
            {Qt::CheckStateRole});
        return true;
    }

    QList<ZzMultiSelectOption> items_;
};

ZzMultiSelectComboBoxPrivate::ZzMultiSelectComboBoxPrivate(
    ZzMultiSelectComboBox *q)
    : q_ptr(q)
    , model(new ZzMultiSelectOptionModel(q))
    , view(new QListView(q))
    , delegate(new ZzFluentItemDelegate(view))
{
    Q_ASSERT(q_ptr != nullptr);
    q_ptr->QComboBox::setModel(model);
    q_ptr->QComboBox::setView(view);
    q_ptr->QComboBox::setEditable(true);
    editor = q_ptr->lineEdit();
    Q_ASSERT(editor != nullptr);

    editor->setReadOnly(true);
    editor->setFrame(false);
    editor->setFocusPolicy(Qt::NoFocus);
    editor->setCursorPosition(0);

    view->setItemDelegate(delegate);
    view->setSelectionMode(QAbstractItemView::NoSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setUniformItemSizes(true);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setTextElideMode(Qt::ElideRight);

    q_ptr->setMaxVisibleItems(8);
    q_ptr->setFocusPolicy(Qt::StrongFocus);
    q_ptr->QComboBox::setCurrentIndex(-1);
    QObject::connect(
        model,
        &QAbstractItemModel::dataChanged,
        q_ptr,
        [this](
            const QModelIndex &,
            const QModelIndex &,
            const QList<int> &roles) {
            if (roles.isEmpty() || roles.contains(Qt::CheckStateRole)) {
                refreshSummary();
                Q_EMIT q_ptr->selectionChanged();
            }
        });
    QObject::connect(
        q_ptr,
        qOverload<int>(&QComboBox::currentIndexChanged),
        q_ptr,
        [this](int index) {
            if (index != -1) {
                refreshSummary();
            }
        });
    installEventFilters();
}

bool ZzMultiSelectComboBoxPrivate::setOptions(
    QList<ZzMultiSelectOption> options)
{
    return model->setItems(std::move(options));
}

QList<ZzMultiSelectOption> ZzMultiSelectComboBoxPrivate::options() const
{
    return model->items();
}

int ZzMultiSelectComboBoxPrivate::optionCount() const noexcept
{
    return model->rowCount();
}

QString ZzMultiSelectComboBoxPrivate::addOption(
    ZzMultiSelectOption option)
{
    return model->append(std::move(option));
}

bool ZzMultiSelectComboBoxPrivate::removeOption(const QString &key)
{
    return model->removeByKey(key);
}

bool ZzMultiSelectComboBoxPrivate::removeOptionAt(int index)
{
    return model->removeAt(index);
}

bool ZzMultiSelectComboBoxPrivate::clearOptions()
{
    return model->clear();
}

bool ZzMultiSelectComboBoxPrivate::setOptionSelected(
    const QString &key,
    bool selected)
{
    return model->setSelectedByKey(key, selected);
}

bool ZzMultiSelectComboBoxPrivate::setOptionSelectedAt(
    int index,
    bool selected)
{
    return model->setSelectedAt(index, selected);
}

bool ZzMultiSelectComboBoxPrivate::setSelectedKeys(
    const QStringList &keys)
{
    return model->setSelectedKeys(keys);
}

bool ZzMultiSelectComboBoxPrivate::setSelectedIndexes(
    const QList<int> &indexes)
{
    return model->setSelectedIndexes(indexes);
}

bool ZzMultiSelectComboBoxPrivate::selectAll()
{
    return model->selectAllEnabled();
}

bool ZzMultiSelectComboBoxPrivate::clearSelection()
{
    return model->clearSelection();
}

QList<ZzMultiSelectOption>
ZzMultiSelectComboBoxPrivate::selectedOptions() const
{
    return model->selectedItems();
}

QStringList ZzMultiSelectComboBoxPrivate::selectedKeys() const
{
    return model->selectedKeys();
}

QList<int> ZzMultiSelectComboBoxPrivate::selectedIndexes() const
{
    return model->selectedIndexes();
}

int ZzMultiSelectComboBoxPrivate::selectionCount() const noexcept
{
    return model->selectionCount();
}

ZzMultiSelectOption ZzMultiSelectComboBoxPrivate::optionAt(int index) const
{
    return model->itemAt(index);
}

void ZzMultiSelectComboBoxPrivate::refreshSummary()
{
    QStringList texts;
    const QList<ZzMultiSelectOption> selected = model->selectedItems();
    texts.reserve(selected.size());
    for (const ZzMultiSelectOption &option : selected) {
        texts.append(option.text);
    }
    const QString summary = texts.join(QStringLiteral(", "));
    q_ptr->QComboBox::setCurrentIndex(-1);
    if (editor->text() != summary) {
        const QSignalBlocker blocker(editor);
        editor->setText(summary);
    }
    editor->setCursorPosition(0);
    editor->deselect();
    q_ptr->update();
}

void ZzMultiSelectComboBoxPrivate::installEventFilters()
{
    for (QObject *watched : {static_cast<QObject *>(view),
                             static_cast<QObject *>(view->viewport()),
                             static_cast<QObject *>(editor)}) {
        watched->removeEventFilter(q_ptr);
        watched->installEventFilter(q_ptr);
    }
}

bool ZzMultiSelectComboBoxPrivate::handleEvent(
    QObject *watched,
    QEvent *event)
{
    if (event == nullptr) {
        return false;
    }
    if (watched == editor && event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && q_ptr->isEnabled()) {
            q_ptr->showPopup();
            return true;
        }
    }
    if (watched == view && event->type() == QEvent::ShortcutOverride) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Space
            || key->key() == Qt::Key_Return
            || key->key() == Qt::Key_Enter) {
            event->accept();
            return true;
        }
    }
    if (watched == view
        && (event->type() == QEvent::KeyPress
            || event->type() == QEvent::KeyRelease)) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Space
            || key->key() == Qt::Key_Return
            || key->key() == Qt::Key_Enter) {
            if (event->type() == QEvent::KeyPress) {
                keepPopupOpen = true;
                (void)toggleFromUser(view->currentIndex().row());
            } else {
                keepPopupOpen = false;
            }
            return true;
        }
        if (event->type() == QEvent::KeyPress
            && (key->key() == Qt::Key_Tab
                || key->key() == Qt::Key_Backtab)) {
            q_ptr->hidePopup();
            return false;
        }
    }
    if (watched != view->viewport()) {
        return false;
    }
    if (event->type() != QEvent::MouseButtonPress
        && event->type() != QEvent::MouseButtonRelease
        && event->type() != QEvent::MouseButtonDblClick) {
        return false;
    }
    const auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton) {
        return false;
    }
    const QModelIndex index = view->indexAt(mouse->position().toPoint());
    if (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonDblClick) {
        pressedIndex = index;
        if (index.isValid()) {
            view->setCurrentIndex(index);
        }
        return true;
    }

    const QPersistentModelIndex pressed = pressedIndex;
    pressedIndex = QPersistentModelIndex();
    if (pressed.isValid() && pressed == index) {
        (void)toggleFromUser(index.row());
    }
    return true;
}

bool ZzMultiSelectComboBoxPrivate::toggleFromUser(int index)
{
    const ZzMultiSelectOption before = model->itemAt(index);
    if (before.key.isEmpty() || !before.enabled
        || !model->setSelectedAt(index, !before.selected)) {
        return false;
    }
    const ZzMultiSelectOption after = model->itemAt(index);
    Q_EMIT q_ptr->optionToggled(after, after.selected);
    return true;
}

} // namespace ZzFluentUI
