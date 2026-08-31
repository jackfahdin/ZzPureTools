#pragma once

#include <memory>

#include <QtCore/QObject>

#include <ZzCore/ZzResult.h>

#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowApplyState.h>
#include <ZzWindowKit/ZzWindowBackdrop.h>
#include <ZzWindowKit/ZzWindowCapability.h>
#include <ZzWindowKit/ZzWindowChromeConfiguration.h>
#include <ZzWindowKit/ZzWindowColorScheme.h>
#include <ZzWindowKit/ZzWindowKitExport.h>

class QPoint;
class QWidget;

namespace ZzWindowKit {

class ZzWindowAgentPrivate;
class ZzWindowAgentTestAccess;
class ZzWindowBackend;

/**
 * @brief 管理单个顶层 QWidget 的无边框后端和完整标题栏绑定。
 *
 * 对象及其全部公开方法只能在对象所属 GUI 线程使用。宿主窗口和标题栏控件均为
 * 非拥有引用；宿主销毁后代理进入 Invalidated。capabilities() 是保守快照，具体
 * 效果是否生效应以 setter 返回值为准。
 */
class ZZ_WINDOWKIT_EXPORT ZzWindowAgent final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzWindowAgent)

public:
    /**
     * @brief 创建使用默认无边框后端的窗口代理。
     * @param parent 可选 QObject 所有者，不影响宿主窗口所有权。
     */
    explicit ZzWindowAgent(QObject *parent = nullptr);

    /** @brief 销毁代理及其独占后端，不销毁宿主窗口或标题栏控件。 */
    ~ZzWindowAgent() override;

    /**
     * @brief 单次绑定一个顶层 QWidget。
     * @param window 非拥有的顶层窗口，必须与代理位于同一线程。
     * @return 绑定成功，或参数、状态、线程及后端错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> attach(QWidget *window);

    /**
     * @brief 完整绑定标题栏、系统按钮和非拖动交互控件。
     * @param configuration 所有非空控件必须属于当前标题栏和宿主窗口。
     * @return 配置成功，或参数、状态、线程及后端错误。
     *
     * 每次调用都会替换完整配置；更换 titleBar 时不得依赖旧按钮或旧交互区域。
     */
    [[nodiscard]] ZzCore::ZzResult<void> configureChrome(
        const ZzWindowChromeConfiguration &configuration);

    /**
     * @brief 获取当前生命周期状态。
     * @return 当前代理状态。
     */
    [[nodiscard]] ZzWindowAgentState state() const noexcept;

    /**
     * @brief 获取后端在绑定后确定的保守能力快照。
     * @return 已绑定时返回后端能力，否则返回空集合。
     */
    [[nodiscard]] ZzWindowCapabilities capabilities() const noexcept;

    /**
     * @brief 请求平台背景材质。
     * @param backdrop 背景材质。
     * @return 已应用、延后或不支持，后端失败通过外层错误返回。
     */
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setBackdrop(
        ZzWindowBackdrop backdrop);

    /**
     * @brief 请求原生材质颜色模式。
     * @param colorScheme 颜色模式。
     * @return 已应用、延后或不支持，后端失败通过外层错误返回。
     */
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setColorScheme(
        ZzWindowColorScheme colorScheme);

    /**
     * @brief 设置已绑定顶层窗口的置顶状态。
     * @param alwaysOnTop 是否保持窗口位于其他普通窗口之上。
     * @return 成功应用或状态、线程及后端错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> setAlwaysOnTop(bool alwaysOnTop);

    /**
     * @brief 请求在指定全局坐标显示系统窗口菜单。
     * @param globalPosition 屏幕全局坐标。
     * @return 请求已提交时成功，否则返回状态或后端错误。
     *
     * Linux 窗口管理器或 Wayland compositor 仍可能忽略已成功提交的请求。
     */
    [[nodiscard]] ZzCore::ZzResult<void> showSystemMenu(
        const QPoint &globalPosition);

private:
    friend class ZzWindowAgentTestAccess;

    explicit ZzWindowAgent(
        std::unique_ptr<ZzWindowBackend> backend,
        QObject *parent);

    std::unique_ptr<ZzWindowAgentPrivate> d_ptr;
};

} // namespace ZzWindowKit
