#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QtGlobal>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureToolsExport.h>
#include <ZzPureTools/ZzRouteId.h>

namespace ZzPureTools {

class ZzNavigationControllerPrivate;
class ZzNavigationModel;
class ZzPageHost;

/**
 * @brief 按强类型路由协调导航模型、页面宿主和窗口级有界历史。
 *
 * model 和 pageHost 是非拥有观察值，生命周期必须覆盖 controller。全部方法只能
 * 在 controller 所属 GUI 线程调用。
 */
class ZZ_PURE_TOOLS_EXPORT ZzNavigationController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzNavigationController)

public:
    /**
     * @brief 创建窗口级导航控制器。
     * @param model 非空且同线程的只读导航模型。
     * @param pageHost 非空且同线程的页面宿主。
     * @param parent 可选 QObject 父对象。
     */
    ZzNavigationController(
        ZzNavigationModel *model,
        ZzPageHost *pageHost,
        QObject *parent = nullptr);

    /** @brief 销毁窗口级导航状态。 */
    ~ZzNavigationController() override;

    /**
     * @brief 一次性设置当前窗口可导航的页面注册表。
     * @param registrations 拥有值的页面注册项。
     * @return 设置成功，或重复调用、无效路由、factory 及重复错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> setRegistrations(
        QList<ZzPageRegistration> registrations);

    /**
     * @brief 按路由激活页面并维护历史或框架错误页。
     * @param routeId 目标页面强类型路由。
     * @return 导航成功，或未注册路由和原始页面 factory 错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> navigate(
        const ZzRouteId &routeId);

    /**
     * @brief 激活最近历史页面，成功后才移除该历史项。
     * @return 回退成功，或无历史、注册状态及页面创建错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> goBack();

    /** @brief 查询当前窗口是否存在可回退历史。 */
    [[nodiscard]] bool canGoBack() const noexcept;

    /**
     * @brief 激活最近一次被回退的页面，成功后才更新双向历史。
     * @return 前进成功，或无前进历史、注册状态及页面创建错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> goForward();

    /** @brief 查询当前窗口是否存在可前进历史。 */
    [[nodiscard]] bool canGoForward() const noexcept;

    /** @brief 返回页面宿主当前正常页或框架错误页路由。 */
    [[nodiscard]] ZzRouteId currentRoute() const;

    /**
     * @brief 设置窗口级历史容量并立即裁剪最旧项。
     * @param capacity 大于或等于零的路由数量；零禁用历史。
     * @return 设置成功，或参数及线程状态错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> setHistoryCapacity(
        qsizetype capacity);

Q_SIGNALS:
    /** @brief 当前正常页面或框架错误页路由发生变化。 */
    void currentRouteChanged(const ZzRouteId &routeId);

    /** @brief 页面导航失败并携带不面向最终用户的技术错误。 */
    void navigationFailed(const ZzCore::ZzError &error);

    /**
     * @brief back 或 forward 的可用状态实际变化后发出。
     * @param canGoBack 当前是否允许回退。
     * @param canGoForward 当前是否允许前进。
     */
    void historyStateChanged(bool canGoBack, bool canGoForward);

private:
    friend class ZzNavigationControllerPrivate;
    std::unique_ptr<ZzNavigationControllerPrivate> d_ptr;
};

} // namespace ZzPureTools
