#pragma once

#include <memory>

#include <QtWidgets/QKeySequenceEdit>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QFocusEvent;
class QKeyEvent;

namespace ZzFluentUI {

class ZzKeyBinderPrivate;

/**
 * @brief 基于 Qt 跨平台键序列语义提供可取消的快捷键录制事务。
 *
 * keySequence 是唯一当前值，组合键解析、平台文本、输入结束时机和
 * 无障碍语义继续由 QKeySequenceEdit 管理。组件不注册全局快捷键，
 * 也不判断应用命令之间的冲突。
 */
class ZZ_FLUENT_UI_EXPORT ZzKeyBinder final : public QKeySequenceEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzKeyBinder)
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)

public:
    /**
     * @brief 创建空快捷键录制器。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzKeyBinder(QWidget *parent = nullptr);

    /**
     * @brief 创建带初始键序列的快捷键录制器。
     * @param sequence 初始跨平台 Qt 键序列。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzKeyBinder(
        const QKeySequence &sequence,
        QWidget *parent = nullptr);

    /** @brief 销毁录制事务的私有快照状态。 */
    ~ZzKeyBinder() override;

    /**
     * @brief 返回当前是否处于录制事务。
     * @return 已开始且尚未接受或取消时返回 true。
     */
    [[nodiscard]] bool isRecording() const noexcept;

public Q_SLOTS:
    /** @brief 保存当前值作为回滚点并开始录制。 */
    void startRecording();

    /** @brief 恢复录制前的键序列并取消当前事务。 */
    void cancelRecording();

Q_SIGNALS:
    /**
     * @brief 录制事务状态实际变化后发出。
     * @param recording 当前是否正在录制。
     */
    void recordingChanged(bool recording);

    /** @brief 当前录制事务被 Escape 或公开槽取消后发出。 */
    void recordingCanceled();

    /**
     * @brief Qt 完成输入或控件失焦并接受当前值后发出。
     * @param sequence 接受的跨平台 Qt 键序列。
     */
    void recordingAccepted(const QKeySequence &sequence);

protected:
    /** @brief 获得焦点时进入可回滚录制事务。 */
    void focusInEvent(QFocusEvent *event) override;

    /** @brief 失去焦点时让 Qt 完成输入并接受当前值。 */
    void focusOutEvent(QFocusEvent *event) override;

    /** @brief 处理 Escape 回滚与 Backspace 清空，其余按键交给 Qt。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 语言变化后刷新提示和无障碍说明。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzKeyBinderPrivate> d_ptr;
};

} // namespace ZzFluentUI
