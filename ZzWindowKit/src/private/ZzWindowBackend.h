#pragma once

#include <ZzCore/ZzResult.h>

#include <ZzWindowKit/ZzWindowApplyState.h>
#include <ZzWindowKit/ZzWindowBackdrop.h>
#include <ZzWindowKit/ZzWindowCapability.h>
#include <ZzWindowKit/ZzWindowChromeConfiguration.h>
#include <ZzWindowKit/ZzWindowColorScheme.h>

class QPoint;
class QWidget;

namespace ZzWindowKit {

/**
 * @brief 隔离具体无边框实现的私有后端接口。
 */
class ZzWindowBackend
{
public:
    /** @brief 销毁私有后端。 */
    virtual ~ZzWindowBackend() = default;

    /**
     * @brief 绑定顶层窗口。
     * @param window 非拥有的顶层窗口。
     * @return 成功状态或后端错误。
     */
    [[nodiscard]] virtual ZzCore::ZzResult<void> attach(QWidget *window) = 0;

    /**
     * @brief 完整重绑标题栏、系统按钮和交互控件。
     * @param configuration 已由 facade 验证的完整配置。
     * @return 成功状态或后端错误。
     */
    [[nodiscard]] virtual ZzCore::ZzResult<void> configureChrome(
        const ZzWindowChromeConfiguration &configuration) = 0;

    /**
     * @brief 获取绑定时确定的保守能力集合。
     * @return 平台能力快照。
     */
    [[nodiscard]] virtual ZzWindowCapabilities capabilities() const noexcept = 0;

    /**
     * @brief 请求窗口背景材质。
     * @param backdrop 背景材质。
     * @return 应用状态或后端错误。
     */
    [[nodiscard]] virtual ZzCore::ZzResult<ZzWindowApplyState> setBackdrop(
        ZzWindowBackdrop backdrop) = 0;

    /**
     * @brief 请求窗口材质颜色模式。
     * @param colorScheme 颜色模式。
     * @return 应用状态或后端错误。
     */
    [[nodiscard]] virtual ZzCore::ZzResult<ZzWindowApplyState> setColorScheme(
        ZzWindowColorScheme colorScheme) = 0;

    /**
     * @brief 请求在全局坐标显示系统菜单。
     * @param globalPosition 全局屏幕坐标。
     * @return 请求已提交时成功，否则返回错误。
     */
    [[nodiscard]] virtual ZzCore::ZzResult<void> showSystemMenu(
        const QPoint &globalPosition) = 0;
};

} // namespace ZzWindowKit
