#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTypeInfo>
#include <QtWidgets/QPushButton>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/** @brief 保存滚轮选择器中一列的值语义配置和当前索引。 */
struct ZZ_FLUENT_UI_EXPORT ZzRollerColumn final
{
    /** @brief 调用方提供或由控件生成的唯一稳定列键。 */
    QString key;

    /** @brief 该列按逻辑顺序展示的全部文本项。 */
    QStringList items;

    /** @brief 当前逻辑行；空集合规范为 -1。 */
    int currentIndex = 0;

    /** @brief 用户越过首尾时是否循环选择。 */
    bool wrapping = true;

    /** @brief popup 中该列的最小逻辑像素宽度。 */
    int minimumWidth = 96;

    /** @brief 按全部公开字段比较两列值快照。 */
    friend bool operator==(
        const ZzRollerColumn &,
        const ZzRollerColumn &) = default;
};

class ZzRollerPickerPrivate;

/**
 * @brief 使用可回滚标准 popup 组合多列 ZzRoller 的 Fluent 选择器。
 *
 * popup 打开时保存唯一索引快照。确定提交当前草稿；取消、Escape、
 * 外部点击或窗口失活恢复快照。按钮文本始终由各列当前文本派生。
 */
class ZZ_FLUENT_UI_EXPORT ZzRollerPicker final : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzRollerPicker)
    Q_PROPERTY(int columnCount READ columnCount NOTIFY columnsChanged)
    Q_PROPERTY(QString currentText READ currentText
                   NOTIFY currentTextChanged)
    Q_PROPERTY(bool popupVisible READ isPopupVisible
                   NOTIFY popupVisibleChanged)

public:
    /**
     * @brief 创建拥有一个可复用标准 popup 的空多列选择器。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzRollerPicker(QWidget *parent = nullptr);

    /** @brief 销毁私有 popup、标准按钮和列滚轮。 */
    ~ZzRollerPicker() override;

    /**
     * @brief 一次性替换全部列并规范化键、宽度与索引。
     * @param columns 新的值语义列快照。
     */
    void setColumns(QList<ZzRollerColumn> columns);

    /** @brief 返回包含最终唯一键和实时索引的全部列副本。 */
    [[nodiscard]] QList<ZzRollerColumn> columns() const;

    /** @brief 返回当前列数量。 */
    [[nodiscard]] int columnCount() const noexcept;

    /**
     * @brief 在末尾追加一列并规范化其唯一键。
     * @param column 待追加的值语义列。
     * @return 实际写入的唯一列键。
     */
    [[nodiscard]] QString addColumn(ZzRollerColumn column);

    /**
     * @brief 在指定位置插入一列。
     * @param index 允许取 0 至 columnCount。
     * @param column 待插入的值语义列。
     * @return 位置有效并完成插入时返回 true。
     */
    [[nodiscard]] bool insertColumn(int index, ZzRollerColumn column);

    /**
     * @brief 按唯一键删除一列。
     * @param key 目标列的规范化键。
     * @return 找到并删除时返回 true。
     */
    [[nodiscard]] bool removeColumn(const QString &key);

    /**
     * @brief 按逻辑顺序删除一列。
     * @param index 从零开始的列号。
     * @return 列号有效并完成删除时返回 true。
     */
    [[nodiscard]] bool removeColumnAt(int index);

    /** @brief 清空全部列；集合已空时不发变化信号。 */
    void clearColumns();

    /**
     * @brief 替换一列的文本集合并收敛当前索引。
     * @param column 从零开始的列号。
     * @param items 新文本项快照。
     * @return 列号有效且集合实际变化时返回 true。
     */
    [[nodiscard]] bool setColumnItems(
        int column,
        QStringList items);

    /**
     * @brief 设置一列的当前逻辑行。
     * @param column 从零开始的列号。
     * @param index 该列中的有效行号。
     * @return 参数有效且索引实际变化时返回 true。
     */
    [[nodiscard]] bool setCurrentIndex(int column, int index);

    /**
     * @brief 原子应用各列索引前缀并忽略无效项。
     * @param indexes 按列顺序排列的索引；多余尾部忽略。
     */
    void setCurrentIndexes(const QList<int> &indexes);

    /**
     * @brief 返回一列的当前逻辑行。
     * @param column 从零开始的列号。
     * @return 有效列的实时索引，否则返回 -1。
     */
    [[nodiscard]] int currentIndex(int column) const noexcept;

    /** @brief 按逻辑列顺序返回全部实时索引。 */
    [[nodiscard]] QList<int> currentIndexes() const;

    /**
     * @brief 选择一列中第一条完全匹配的文本项。
     * @param column 从零开始的列号。
     * @param text 待匹配的完整文本。
     * @return 找到且索引实际变化时返回 true。
     */
    [[nodiscard]] bool setCurrentText(
        int column,
        const QString &text);

    /**
     * @brief 返回一列的当前文本。
     * @param column 从零开始的列号。
     * @return 列和行均有效时返回文本，否则返回空字符串。
     */
    [[nodiscard]] QString currentText(int column) const;

    /** @brief 按逻辑列顺序返回全部实时文本。 */
    [[nodiscard]] QStringList currentTexts() const;

    /** @brief 返回以斜杠分隔非空列文本的派生按钮摘要。 */
    [[nodiscard]] QString currentText() const;

    /** @brief 保存当前索引快照并显示已构造的标准 popup。 */
    void showPopup();

    /** @brief 提交可见 popup 草稿并发出一次接受信号。 */
    void acceptPopup();

    /** @brief 回滚可见 popup 草稿并发出一次取消信号。 */
    void cancelPopup();

    /** @brief 返回私有 popup 当前是否处于打开事务。 */
    [[nodiscard]] bool isPopupVisible() const noexcept;

Q_SIGNALS:
    /** @brief 列结构、配置或任一列文本集合实际变化后发出。 */
    void columnsChanged();

    /**
     * @brief 任一实时索引或对应文本实际变化后发出。
     * @param indexes 按逻辑列顺序排列的完整索引快照。
     * @param texts 按逻辑列顺序排列的完整文本快照。
     */
    void currentSelectionChanged(
        const QList<int> &indexes,
        const QStringList &texts);

    /**
     * @brief 派生按钮摘要实际变化后发出。
     * @param text 新的斜杠分隔摘要。
     */
    void currentTextChanged(const QString &text);

    /**
     * @brief 用户在 popup 中完成一列的有效选择后发出。
     * @param column 从零开始的逻辑列号。
     * @param index 最终逻辑行号。
     * @param text 最终展示文本。
     */
    void selectionActivated(
        int column,
        int index,
        const QString &text);

    /**
     * @brief 用户确认当前 popup 草稿后发出。
     * @param indexes 提交时的完整索引快照。
     * @param texts 提交时的完整文本快照。
     */
    void selectionAccepted(
        const QList<int> &indexes,
        const QStringList &texts);

    /** @brief popup 取消或外部关闭并完成必要回滚后发出。 */
    void selectionCanceled();

    /**
     * @brief popup 打开事务状态实际变化后发出。
     * @param visible 打开后为 true，关闭后为 false。
     */
    void popupVisibleChanged(bool visible);

private:
    friend class ZzRollerPickerPrivate;

    using QPushButton::setText;

    std::unique_ptr<ZzRollerPickerPrivate> d_ptr;
};

} // namespace ZzFluentUI

Q_DECLARE_TYPEINFO(
    ZzFluentUI::ZzRollerColumn,
    Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzFluentUI::ZzRollerColumn)
