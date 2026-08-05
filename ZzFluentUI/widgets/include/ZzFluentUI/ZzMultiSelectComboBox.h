#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTypeInfo>
#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QComboBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

QT_BEGIN_NAMESPACE
class QEvent;
class QKeyEvent;
class QWheelEvent;
QT_END_NAMESPACE

namespace ZzFluentUI {

/** @brief 保存多选组合框中一条选项的值语义快照。 */
struct ZZ_FLUENT_UI_EXPORT ZzMultiSelectOption final
{
    /** @brief 调用方提供或由控件生成的唯一稳定键。 */
    QString key;

    /** @brief 在关闭面板和 popup 中展示的文本。 */
    QString text;

    /** @brief 可为空的选项装饰图标。 */
    QIcon icon;

    /** @brief 由调用方解释的值语义载荷。 */
    QVariant data;

    /** @brief 是否允许用户通过 popup 切换该选项。 */
    bool enabled = true;

    /** @brief 该选项当前是否被选中。 */
    bool selected = false;

    /** @brief 按全部公开字段和图标缓存身份比较两条选项快照。 */
    friend bool operator==(
        const ZzMultiSelectOption &left,
        const ZzMultiSelectOption &right)
    {
        return left.key == right.key
            && left.text == right.text
            && left.icon.cacheKey() == right.icon.cacheKey()
            && left.data == right.data
            && left.enabled == right.enabled
            && left.selected == right.selected;
    }
};

class ZzMultiSelectComboBoxPrivate;

/**
 * @brief 以稳定键和值模型维护多项选择的 Fluent 组合框。
 *
 * 选项模型中的 selected 字段是唯一选择真值。QComboBox 只负责关闭面板、
 * popup 生命周期和焦点语义，内部只读编辑器只展示由模型派生的摘要。
 */
class ZZ_FLUENT_UI_EXPORT ZzMultiSelectComboBox final : public QComboBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzMultiSelectComboBox)
    Q_PROPERTY(int optionCount READ optionCount NOTIFY optionsChanged)
    Q_PROPERTY(int selectionCount READ selectionCount
                   NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedText READ selectedText
                   NOTIFY selectionChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText
                   WRITE setPlaceholderText)

public:
    /** @brief 公开只读选项模型中的稳定扩展 role。 */
    enum DataRole {
        /** @brief 返回当前选项唯一键的 role。 */
        KeyRole = Qt::UserRole + 1
    };
    Q_ENUM(DataRole)

    /**
     * @brief 创建拥有私有值模型和标准 QListView popup 的多选组合框。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzMultiSelectComboBox(QWidget *parent = nullptr);

    /** @brief 销毁私有装配状态和由 Qt 管理的控件对象树。 */
    ~ZzMultiSelectComboBox() override;

    /**
     * @brief 一次性替换全部选项并规范化空键或重复键。
     * @param options 新的值语义选项快照。
     */
    void setOptions(QList<ZzMultiSelectOption> options);

    /** @brief 返回包含最终唯一键的全部选项副本。 */
    [[nodiscard]] QList<ZzMultiSelectOption> options() const;

    /** @brief 返回当前选项数量。 */
    [[nodiscard]] int optionCount() const noexcept;

    /**
     * @brief 追加一条自动生成唯一键的选项。
     * @param text 展示文本。
     * @param payload 调用方值语义载荷。
     * @param icon 可为空的装饰图标。
     * @param selected 是否初始选中。
     * @return 实际写入模型的唯一键；达到模型容量上限时返回空值。
     */
    [[nodiscard]] QString addOption(
        QString text,
        QVariant payload = {},
        QIcon icon = {},
        bool selected = false);

    /**
     * @brief 追加选项并在需要时替换空键或重复键。
     * @param option 待追加的值语义快照。
     * @return 实际写入模型的唯一键；达到模型容量上限时返回空值。
     */
    [[nodiscard]] QString addOption(ZzMultiSelectOption option);

    /**
     * @brief 按唯一键删除一条选项。
     * @param key 要删除的稳定键。
     * @return 找到并删除时返回 true。
     */
    [[nodiscard]] bool removeOption(const QString &key);

    /**
     * @brief 按模型顺序删除一条选项。
     * @param index 从零开始的行号。
     * @return 行号有效并完成删除时返回 true。
     */
    [[nodiscard]] bool removeOptionAt(int index);

    /** @brief 清空全部选项；模型已空时不发变化信号。 */
    void clearOptions();

    /**
     * @brief 按唯一键改变一条选项的选择状态。
     * @param key 目标选项的稳定键。
     * @param selected 新选择状态。
     * @return 找到选项且状态实际改变时返回 true。
     */
    [[nodiscard]] bool setOptionSelected(
        const QString &key,
        bool selected);

    /**
     * @brief 按模型行改变一条选项的选择状态。
     * @param index 从零开始的行号。
     * @param selected 新选择状态。
     * @return 行号有效且状态实际改变时返回 true。
     */
    [[nodiscard]] bool setOptionSelectedAt(int index, bool selected);

    /**
     * @brief 以稳定键集合精确替换当前选择。
     * @param keys 可含重复或未知值；未知值被忽略。
     */
    void setSelectedKeys(QStringList keys);

    /**
     * @brief 以模型行集合精确替换当前选择。
     * @param indexes 可含重复、负数或越界值；非法值被忽略。
     */
    void setSelectedIndexes(QList<int> indexes);

    /** @brief 选中全部 enabled 选项，不改变 disabled 选项状态。 */
    void selectAll();

    /** @brief 清除全部选择，包括已选中的 disabled 选项。 */
    void clearSelection();

    /** @brief 按模型顺序返回全部已选选项副本。 */
    [[nodiscard]] QList<ZzMultiSelectOption> selectedOptions() const;

    /** @brief 按模型顺序返回全部已选稳定键。 */
    [[nodiscard]] QStringList selectedKeys() const;

    /** @brief 按模型顺序返回全部已选行号。 */
    [[nodiscard]] QList<int> selectedIndexes() const;

    /** @brief 返回当前已选选项数量。 */
    [[nodiscard]] int selectionCount() const noexcept;

    /** @brief 返回以逗号和空格连接的派生展示摘要。 */
    [[nodiscard]] QString selectedText() const;

    /**
     * @brief 同步设置 QComboBox 与内部只读编辑器的占位文字。
     * @param text 无选择时展示的文字。
     */
    void setPlaceholderText(const QString &text);

    /** @brief 打开标准 QComboBox popup 并恢复多选事件过滤顺序。 */
    void showPopup() override;

Q_SIGNALS:
    /** @brief 选项发生有效 reset、insert、remove 或 clear 后发出。 */
    void optionsChanged();

    /** @brief 选择快照或已选项展示内容实际改变后发出。 */
    void selectionChanged();

    /**
     * @brief 用户通过鼠标或键盘切换一条 enabled 选项后发出。
     * @param option 切换完成后的完整值快照。
     * @param selected 切换完成后的选择状态。
     */
    void optionToggled(
        const ZzMultiSelectOption &option,
        bool selected);

protected:
    /** @brief 截获 popup 切换事件并保留 Qt 的标准关闭路径。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /** @brief 在关闭状态下用标准按键打开 popup，禁止改变单选 index。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 忽略 combo 本体滚轮，避免静默改变选择。 */
    void wheelEvent(QWheelEvent *event) override;

private:
    friend class ZzMultiSelectComboBoxPrivate;

    using QComboBox::addItem;
    using QComboBox::addItems;
    using QComboBox::clear;
    using QComboBox::insertItem;
    using QComboBox::insertItems;
    using QComboBox::removeItem;
    using QComboBox::setCompleter;
    using QComboBox::setCurrentIndex;
    using QComboBox::setCurrentText;
    using QComboBox::setDuplicatesEnabled;
    using QComboBox::setEditable;
    using QComboBox::setEditText;
    using QComboBox::setInsertPolicy;
    using QComboBox::setItemData;
    using QComboBox::setItemDelegate;
    using QComboBox::setLineEdit;
    using QComboBox::setModel;
    using QComboBox::setModelColumn;
    using QComboBox::setRootModelIndex;
    using QComboBox::setValidator;
    using QComboBox::setView;

    std::unique_ptr<ZzMultiSelectComboBoxPrivate> d_ptr;
};

} // namespace ZzFluentUI

Q_DECLARE_TYPEINFO(
    ZzFluentUI::ZzMultiSelectOption,
    Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzFluentUI::ZzMultiSelectOption)
