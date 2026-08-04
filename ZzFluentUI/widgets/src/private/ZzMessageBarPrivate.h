#pragma once

#include <QtCore/QString>

#include <ZzFluentUI/ZzMessageSeverity.h>

class QLabel;
class QTimer;
class QToolButton;

namespace ZzFluentUI {

class ZzMessageBar;

/** @brief 持有消息展示状态和单一超时定时器的非拥有子控件指针。 */
class ZzMessageBarPrivate final
{
public:
    /** @brief 创建展示子控件并绑定一次性关闭源。 */
    explicit ZzMessageBarPrivate(ZzMessageBar *q);

    /** @brief 刷新图标、文本、按钮翻译和无障碍内容。 */
    void refreshPresentation();

    /** @brief 从完整超时重新开始可见且未 hover 的计时。 */
    void restartTimer();

    /** @brief 保存当前剩余时间并停止计时。 */
    void pauseTimer() noexcept;

    /** @brief 从保存的剩余时间继续计时。 */
    void resumeTimer();

    /** @brief 幂等停止计时并向宿主发出一次关闭意图。 */
    void requestClose();

    ZzMessageBar *const q_ptr;
    QLabel *iconLabel = nullptr;
    QLabel *textLabel = nullptr;
    QToolButton *closeButton = nullptr;
    QTimer *timer = nullptr;
    QString text;
    ZzMessageSeverity severity = ZzMessageSeverity::Information;
    int timeoutMilliseconds = 0;
    int remainingMilliseconds = 0;
    bool closable = true;
    bool closePending = false;
    bool hovered = false;
};

} // namespace ZzFluentUI
