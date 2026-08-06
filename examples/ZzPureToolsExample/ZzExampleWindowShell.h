#pragma once

#include <memory>

#include <QtCore/QObject>

#include <ZzCore/ZzResult.h>

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

class QEvent;

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleWindowShellPrivate;

/**
 * @brief 为单个应用窗口装配命令栏、活动 Dock 和状态栏。
 *
 * 每次 attach() 创建一个由窗口 QObject 子树独占的实例；对象不持有页面业务状态。
 */
class ZzExampleWindowShell final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 为尚未显示且已具备导航控制器的窗口装配独立壳层。
     * @param window 当前窗口。
     * @param context 非空跨窗口共享上下文。
     * @param application 创建新窗口和访问应用主题的宿主。
     * @param closeGuardEnabled 是否安装交互式关闭守卫。
     * @return 装配成功，或输入及控件创建状态错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<void> attach(
        ZzPureTools::ZzApplicationWindow &window,
        std::shared_ptr<ZzExampleApplicationContext> context,
        ZzPureTools::ZzPureApplication &application,
        bool closeGuardEnabled = true);

    /**
     * @brief 返回指定窗口已经完成装配的示例壳层。
     * @param window 应用窗口。
     * @return 窗口直接拥有的壳层，尚未装配时返回 nullptr。
     */
    [[nodiscard]] static ZzExampleWindowShell *attachedTo(
        ZzPureTools::ZzApplicationWindow &window) noexcept;

    /** @brief 返回当前窗口活动 Dock 是否可见。 */
    [[nodiscard]] bool isActivityDockVisible() const noexcept;

    /**
     * @brief 设置当前窗口活动 Dock 可见性。
     * @param visible 是否显示。
     */
    void setActivityDockVisible(bool visible);

    /** @brief 释放私有观察状态，Qt 子控件由窗口父子树销毁。 */
    ~ZzExampleWindowShell() override;

    /** @brief 禁止复制已绑定窗口 QObject 子树的壳层。 */
    ZzExampleWindowShell(const ZzExampleWindowShell &) = delete;

    /** @brief 禁止复制赋值已绑定窗口 QObject 子树的壳层。 */
    ZzExampleWindowShell &operator=(const ZzExampleWindowShell &) = delete;

    /** @brief 禁止移动已连接窗口信号的壳层。 */
    ZzExampleWindowShell(ZzExampleWindowShell &&) = delete;

    /** @brief 禁止移动赋值已连接窗口信号的壳层。 */
    ZzExampleWindowShell &operator=(ZzExampleWindowShell &&) = delete;

Q_SIGNALS:
    /** @brief 活动 Dock 可见性实际变化后发出。 */
    void activityDockVisibilityChanged(bool visible);

protected:
    /** @brief 将当前窗口关闭事件交给逐窗口关闭守卫。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    ZzExampleWindowShell(
        ZzPureTools::ZzApplicationWindow &window,
        std::shared_ptr<ZzExampleApplicationContext> context,
        ZzPureTools::ZzPureApplication &application,
        bool closeGuardEnabled);

    std::unique_ptr<ZzExampleWindowShellPrivate> d_ptr;
};

} // namespace ZzExample
