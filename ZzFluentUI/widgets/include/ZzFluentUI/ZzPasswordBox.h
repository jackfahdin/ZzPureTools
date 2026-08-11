#pragma once

#include <memory>

#include <QtWidgets/QLineEdit>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzPasswordRevealMode.h>

class QEvent;
class QFocusEvent;
class QResizeEvent;

namespace ZzFluentUI {

class ZzPasswordBoxPrivate;

/**
 * @brief 保留 QLineEdit 编辑语义并提供受控密码查看按钮。
 *
 * text 是唯一密码状态；输入法、光标、选择、撤销、validator 和剪贴板
 * 继续由 QLineEdit 管理。Peek 模式只改变纯展示 echoMode，不复制密码，
 * 也不访问认证或设置业务。
 */
class ZZ_FLUENT_UI_EXPORT ZzPasswordBox final : public QLineEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPasswordBox)
    Q_PROPERTY(
        ZzFluentUI::ZzPasswordRevealMode revealMode
        READ revealMode
        WRITE setRevealMode
        NOTIFY revealModeChanged)
    Q_PROPERTY(
        bool passwordVisible
        READ isPasswordVisible
        NOTIFY passwordVisibilityChanged)

public:
    /**
     * @brief 创建默认使用 Peek 策略的密码输入框。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzPasswordBox(QWidget *parent = nullptr);

    /** @brief 销毁固定查看按钮和私有展示状态。 */
    ~ZzPasswordBox() override;

    /**
     * @brief 返回当前密码显示策略。
     * @return Hidden、Peek 或 Visible。
     */
    [[nodiscard]] ZzPasswordRevealMode revealMode() const noexcept;

    /**
     * @brief 设置密码显示策略，重复值不发信号。
     * @param mode 新的显示策略。
     */
    void setRevealMode(ZzPasswordRevealMode mode);

    /**
     * @brief 返回当前是否正在以普通文本显示密码。
     * @return Visible 模式或 Peek 正在按住时返回 true。
     */
    [[nodiscard]] bool isPasswordVisible() const noexcept;

Q_SIGNALS:
    /**
     * @brief 密码显示策略实际变化后发出。
     * @param mode 新策略。
     */
    void revealModeChanged(ZzPasswordRevealMode mode);

    /**
     * @brief 当前明文可见状态实际变化后发出。
     * @param visible 当前是否显示明文。
     */
    void passwordVisibilityChanged(bool visible);

protected:
    /** @brief 处理窗口失活和 DPR 变化，并保持 Peek 安全终止。 */
    bool event(QEvent *event) override;

    /** @brief 在尺寸变化后更新逻辑 trailing 查看按钮和文本边距。 */
    void resizeEvent(QResizeEvent *event) override;

    /** @brief 在语言、主题、方向或启用状态变化后刷新展示。 */
    void changeEvent(QEvent *event) override;

    /** @brief 输入框失焦时立即结束临时明文显示。 */
    void focusOutEvent(QFocusEvent *event) override;

private:
    using QLineEdit::setEchoMode;

    std::unique_ptr<ZzPasswordBoxPrivate> d_ptr;
};

} // namespace ZzFluentUI
