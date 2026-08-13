#pragma once

#include <array>

#include <QtCore/QMargins>
#include <QtCore/QMetaObject>

#include <ZzFluentUI/ZzPasswordRevealMode.h>

#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

class ZzIconButton;
class ZzPasswordBox;

/** @brief 持有 PasswordBox 固定查看按钮和派生展示状态。 */
class ZzPasswordBoxPrivate final
{
public:
    /**
     * @brief 创建并连接唯一查看按钮。
     * @param q 非空、非拥有的公开密码输入框。
     */
    explicit ZzPasswordBoxPrivate(ZzPasswordBox *q);

    /** @brief 在公开控件进入基类析构前断开所有捕获私有状态的回调。 */
    ~ZzPasswordBoxPrivate();

    /** @brief 按当前主题、语言、模式和方向刷新展示。 */
    void refreshPresentation();

    /** @brief 重建非 Fluent 回退快照并刷新展示。 */
    void refreshTheme();

    /** @brief 设置模式并同步 echoMode 和按钮。 */
    void setRevealMode(ZzPasswordRevealMode mode);

    /** @brief 在 Peek 模式下开始临时显示密码。 */
    void beginPeek();

    /** @brief 结束临时显示并恢复当前策略终态。 */
    void endPeek();

    /** @brief 同步按钮可见性、焦点和文本安全边距。 */
    void syncButtonGeometry();

    /** @brief 返回当前派生明文可见状态。 */
    [[nodiscard]] bool isPasswordVisible() const noexcept;

    ZzPasswordBox *const q_ptr;
    ZzWidgetTheme theme;
    ZzIconButton *const revealButton;
    QMargins baseTextMargins;
    ZzPasswordRevealMode revealMode = ZzPasswordRevealMode::Peek;
    bool peekActive = false;

private:
    static constexpr std::size_t zzCallbackConnectionCount = 5;

    /** @brief 同步 QLineEdit echoMode 并只发一次可见性信号。 */
    void applyVisibility(bool wasVisible);

    /** @brief 返回 Peek 按钮当前是否应该显示。 */
    [[nodiscard]] bool shouldShowButton() const noexcept;

    std::array<QMetaObject::Connection, zzCallbackConnectionCount>
        callbackConnections;
};

} // namespace ZzFluentUI
