#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzMessageSeverity.h>

class QEnterEvent;
class QEvent;
class QHideEvent;
class QKeyEvent;
class QShowEvent;

namespace ZzFluentUI {

class ZzMessageBarPrivate;

/** @brief 展示文本、严重性和关闭意图，不拥有业务状态。 */
class ZZ_FLUENT_UI_EXPORT ZzMessageBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzMessageBar)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(
        ZzMessageSeverity severity
        READ severity
        WRITE setSeverity
        NOTIFY severityChanged)
    Q_PROPERTY(
        bool closable
        READ isClosable
        WRITE setClosable
        NOTIFY closableChanged)
    Q_PROPERTY(
        int timeoutMilliseconds
        READ timeoutMilliseconds
        WRITE setTimeoutMilliseconds
        NOTIFY timeoutMillisecondsChanged)

public:
    /**
     * @brief 创建持续显示、可关闭的信息消息条。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzMessageBar(QWidget *parent = nullptr);

    /** @brief 销毁私有状态，子控件和定时器由 QObject parent 释放。 */
    ~ZzMessageBar() override;

    /** @brief 返回当前展示文本。 */
    [[nodiscard]] QString text() const;

    /**
     * @brief 更新展示和无障碍文本。
     * @param text 可本地化的纯展示文本。
     */
    void setText(QString text);

    /** @brief 返回当前纯展示严重性。 */
    [[nodiscard]] ZzMessageSeverity severity() const noexcept;

    /**
     * @brief 更新严重性图标，不执行任何业务动作。
     * @param severity 新严重性。
     */
    void setSeverity(ZzMessageSeverity severity);

    /** @brief 返回用户是否可以通过按钮或 Escape 请求关闭。 */
    [[nodiscard]] bool isClosable() const noexcept;

    /**
     * @brief 设置是否展示用户关闭入口。
     * @param closable 为 true 时显示关闭按钮并处理 Escape。
     */
    void setClosable(bool closable);

    /** @brief 返回自动关闭意图的完整超时，0 表示持续显示。 */
    [[nodiscard]] int timeoutMilliseconds() const noexcept;

    /**
     * @brief 设置自动关闭意图超时并从完整时长重新计时。
     * @param milliseconds 毫秒数；负值收敛为 0。
     */
    void setTimeoutMilliseconds(int milliseconds);

Q_SIGNALS:
    /** @brief 展示文本实际变化后发出。 */
    void textChanged(const QString &text);

    /** @brief 严重性实际变化后发出。 */
    void severityChanged(ZzMessageSeverity severity);

    /** @brief 用户关闭入口可用性实际变化后发出。 */
    void closableChanged(bool closable);

    /** @brief 完整超时实际变化后发出。 */
    void timeoutMillisecondsChanged(int milliseconds);

    /** @brief 请求宿主关闭；控件不会隐藏或删除自己。 */
    void closeRequested();

protected:
    /** @brief 在语言或视觉环境变化时刷新纯展示内容。 */
    void changeEvent(QEvent *event) override;

    /** @brief 可关闭时把非自动重复 Escape 转换为一次关闭意图。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 鼠标进入时保存剩余超时并暂停计时。 */
    void enterEvent(QEnterEvent *event) override;

    /** @brief 鼠标离开时从剩余超时继续计时。 */
    void leaveEvent(QEvent *event) override;

    /** @brief 隐藏前暂停计时，隐藏期间不发关闭意图。 */
    void hideEvent(QHideEvent *event) override;

    /** @brief 重新显示时清除已发送状态并从完整超时计时。 */
    void showEvent(QShowEvent *event) override;

private:
    std::unique_ptr<ZzMessageBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
