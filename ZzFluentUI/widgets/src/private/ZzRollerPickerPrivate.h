#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <ZzFluentUI/ZzRollerPicker.h>

QT_BEGIN_NAMESPACE
class QDialogButtonBox;
class QHBoxLayout;
class QPushButton;
class QVBoxLayout;
class QWidget;
QT_END_NAMESPACE

namespace ZzFluentUI {

class ZzRoller;
class ZzRollerPickerPopup;

/** @brief 管理规范化列、固定 popup、快照回滚和屏幕内定位。 */
class ZzRollerPickerPrivate final
{
public:
    /**
     * @brief 一次创建 popup、列布局和标准确定取消按钮。
     * @param q 非空、非拥有的公开选择器。
     */
    explicit ZzRollerPickerPrivate(ZzRollerPicker *q);

    /** @brief 清理 popup 回调并同步销毁顶层子窗口。 */
    ~ZzRollerPickerPrivate();

    /** @brief 规范化并替换全部列；无变化时返回 false。 */
    [[nodiscard]] bool setColumns(QList<ZzRollerColumn> columns);

    /** @brief 插入一列并返回规范化后的唯一键。 */
    [[nodiscard]] QString insertColumn(
        int index,
        ZzRollerColumn column);

    /** @brief 删除有效逻辑列。 */
    [[nodiscard]] bool removeColumnAt(int index);

    /** @brief 替换有效列的文本集合。 */
    [[nodiscard]] bool setColumnItems(
        int column,
        QStringList items);

    /** @brief 应用有效单列索引。 */
    [[nodiscard]] bool setCurrentIndex(int column, int index);

    /** @brief 原子应用索引前缀；无变化时返回 false。 */
    [[nodiscard]] bool setCurrentIndexes(const QList<int> &indexes);

    /** @brief 返回实时索引快照。 */
    [[nodiscard]] QList<int> currentIndexes() const;

    /** @brief 返回实时文本快照。 */
    [[nodiscard]] QStringList currentTexts() const;

    /** @brief 返回斜杠分隔的非空文本摘要。 */
    [[nodiscard]] QString summaryText() const;

    /** @brief 打开固定 popup 并保存唯一快照。 */
    void showPopup();

    /** @brief 提交当前草稿并关闭 popup。 */
    void acceptPopup();

    /** @brief 回滚当前草稿并关闭 popup。 */
    void cancelPopup();

    /** @brief 处理窗口系统触发的 popup 外部关闭。 */
    void handleExternalHide();

    /** @brief 规范化列键、索引和宽度。 */
    [[nodiscard]] QList<ZzRollerColumn> normalizeColumns(
        QList<ZzRollerColumn> columns) const;

    /** @brief 按当前列快照重建私有 Roller 子树。 */
    void rebuildRollers();

    /** @brief 更新标准按钮图标和 popup 屏幕内几何。 */
    void preparePopupGeometry();

    /** @brief 从实时列生成按钮摘要并发送必要通知。 */
    void refreshSummary();

    /** @brief 原子恢复打开快照并发送至多一次选择变化。 */
    void restoreSnapshot();

    /** @brief 发送完整实时选择快照。 */
    void emitSelectionChanged();

    ZzRollerPicker *const q_ptr;
    QList<ZzRollerColumn> columns;
    QList<ZzRoller *> rollers;
    QList<int> openSnapshot;
    ZzRollerPickerPopup *popup = nullptr;
    QWidget *rollerHost = nullptr;
    QHBoxLayout *rollerLayout = nullptr;
    QVBoxLayout *popupLayout = nullptr;
    QDialogButtonBox *buttonBox = nullptr;
    QPushButton *okButton = nullptr;
    QPushButton *cancelButton = nullptr;
    bool popupActive = false;
    bool closingPopup = false;
    bool suppressSelectionSignals = false;
};

} // namespace ZzFluentUI
