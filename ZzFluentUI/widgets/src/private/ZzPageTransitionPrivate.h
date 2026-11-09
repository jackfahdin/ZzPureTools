#pragma once

#include <functional>
#include <memory>
#include <QtCore/QRect>
#include <QtGui/QColor>
#include <ZzFluentUI/ZzFluentUIExport.h>

class QWidget;

namespace ZzFluentUI {

/** @brief 内部共享的非拥有页面播放器；捕获与逻辑切页相互独立。 */
class ZZ_FLUENT_UI_EXPORT ZzPageTransitionPrivate final
{
    Q_DISABLE_COPY_MOVE(ZzPageTransitionPrivate)
public:
    /** @brief 内部绘制模式，不构成 SDK 公共接口。 */
    enum class ZzMode { None, SlideHorizontal, SlideVertical, Fade };
    using Mode = ZzMode;
    /** @brief 保存一段不可变的捕获和播放参数。 */
    struct ZzSettings {
        QRect contentRect;
        qreal dpr = 1;
        QColor background;
        Mode mode = Mode::None;
        bool reverse = false;
        int duration = 0;
    };
    using Settings = ZzSettings;
    using Token = quint64;
    /** @brief 创建属于 owner 区域的单一覆盖层与动画。 */
    explicit ZzPageTransitionPrivate(QWidget *owner);
    /** @brief 停止捕获和动画，解除全部回调。 */
    ~ZzPageTransitionPrivate();
    /** @brief 注册自然播放完成后的回调；cancel 不调用它。 */
    void setFinishedCallback(std::function<void()> callback);
    /** @brief 注册捕获区间观察，空回调不产生计时开销。 */
    void setCaptureObserver(std::function<void(bool)> callback);
    /** @brief 清理旧段、验证预算并返回新的非零代次；失败返回零。 */
    Token beginCapture(const Settings &settings);
    /** @brief 捕获源图一次，不持有源 QWidget。 */
    bool captureSource(Token token, QWidget *view);
    /** @brief 捕获目标图一次并关闭本轮捕获观测。 */
    bool captureTarget(Token token, QWidget *view);
    /** @brief 显示静止首帧，供容器在启动计时前发送逻辑通知。 */
    bool present(Token token);
    /** @brief 安装两图并播放，不要求源页面继续存活。 */
    bool play(Token token);
    /** @brief 停止本段、释放图像并使代次失效。 */
    void cancel();
    /** @brief 查询播放状态。 */
    bool running() const noexcept;
    /** @brief 查询组件所有图像存储。 */
    qsizetype retainedBytes() const noexcept;
    /** @brief 保守识别普通 QWidget 可捕获子树。 */
    static bool canCapture(QWidget *view);
private:
    class ZzPrivate;
    std::shared_ptr<ZzPrivate> d;
};

} // namespace ZzFluentUI
