#pragma once

#include <QtCore/QList>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <ZzFluentUI/ZzMultiSelectComboBox.h>

QT_BEGIN_NAMESPACE
class QEvent;
class QLineEdit;
class QListView;
QT_END_NAMESPACE

namespace ZzFluentUI {

class ZzFluentItemDelegate;
class ZzMultiSelectOptionModel;

/** @brief 管理多选值模型、关闭摘要和标准 popup 的一次性装配。 */
class ZzMultiSelectComboBoxPrivate final
{
public:
    /**
     * @brief 创建值模型、只读 editor、popup view 和 Fluent delegate。
     * @param q 非空、非拥有的公开多选组合框。
     */
    explicit ZzMultiSelectComboBoxPrivate(ZzMultiSelectComboBox *q);

    /** @brief 一次性替换全部选项；内容无变化时返回 false。 */
    [[nodiscard]] bool setOptions(QList<ZzMultiSelectOption> options);

    /** @brief 返回全部选项副本。 */
    [[nodiscard]] QList<ZzMultiSelectOption> options() const;

    /** @brief 返回当前选项行数。 */
    [[nodiscard]] int optionCount() const noexcept;

    /** @brief 追加选项并返回规范化后的唯一键。 */
    [[nodiscard]] QString addOption(ZzMultiSelectOption option);

    /** @brief 按唯一键删除选项。 */
    [[nodiscard]] bool removeOption(const QString &key);

    /** @brief 按模型行删除选项。 */
    [[nodiscard]] bool removeOptionAt(int index);

    /** @brief 清空非空选项集合。 */
    [[nodiscard]] bool clearOptions();

    /** @brief 按唯一键改变选择状态。 */
    [[nodiscard]] bool setOptionSelected(
        const QString &key,
        bool selected);

    /** @brief 按模型行改变选择状态。 */
    [[nodiscard]] bool setOptionSelectedAt(int index, bool selected);

    /** @brief 按稳定键集合精确替换选择。 */
    [[nodiscard]] bool setSelectedKeys(const QStringList &keys);

    /** @brief 按模型行集合精确替换选择。 */
    [[nodiscard]] bool setSelectedIndexes(const QList<int> &indexes);

    /** @brief 选中所有 enabled 选项。 */
    [[nodiscard]] bool selectAll();

    /** @brief 清除全部选择。 */
    [[nodiscard]] bool clearSelection();

    /** @brief 返回全部已选选项副本。 */
    [[nodiscard]] QList<ZzMultiSelectOption> selectedOptions() const;

    /** @brief 返回全部已选稳定键。 */
    [[nodiscard]] QStringList selectedKeys() const;

    /** @brief 返回全部已选模型行。 */
    [[nodiscard]] QList<int> selectedIndexes() const;

    /** @brief 返回已选项数量。 */
    [[nodiscard]] int selectionCount() const noexcept;

    /** @brief 返回指定模型行的选项副本。 */
    [[nodiscard]] ZzMultiSelectOption optionAt(int index) const;

    /** @brief 从唯一值模型重建只读 editor 摘要。 */
    void refreshSummary();

    /** @brief 安装或重新排序固定 event filter。 */
    void installEventFilters();

    /** @brief 处理 editor、view 与 viewport 的多选输入。 */
    [[nodiscard]] bool handleEvent(QObject *watched, QEvent *event);

    /** @brief 用户切换一行并发送公开意图信号。 */
    [[nodiscard]] bool toggleFromUser(int index);

    ZzMultiSelectComboBox *const q_ptr;
    ZzMultiSelectOptionModel *model = nullptr;
    QListView *view = nullptr;
    QLineEdit *editor = nullptr;
    ZzFluentItemDelegate *delegate = nullptr;
    QPersistentModelIndex pressedIndex;
};

} // namespace ZzFluentUI
