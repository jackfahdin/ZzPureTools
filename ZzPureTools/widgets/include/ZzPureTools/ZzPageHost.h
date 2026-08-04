#pragma once

#include <memory>

#include <QtCore/QtGlobal>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureToolsExport.h>
#include <ZzPureTools/ZzRouteId.h>

namespace ZzPureTools {

class ZzPageHostPrivate;

/**
 * @brief 托管延迟创建页面、当前页面切换和容量受限缓存。
 *
 * 全部方法只能在宿主所属 GUI 线程调用。页面 factory 失败不会改变当前可见页面、
 * 当前路由或缓存状态。
 */
class ZZ_PURE_TOOLS_EXPORT ZzPageHost final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPageHost)

public:
    /**
     * @brief 创建默认保留三个非活动 Recreatable 页面的宿主。
     * @param parent 可选 QWidget 父对象。
     */
    explicit ZzPageHost(QWidget *parent = nullptr);

    /** @brief 在 QWidget 子对象树销毁前按页面契约清理全部实例。 */
    ~ZzPageHost() override;

    /**
     * @brief 事务式创建或复用并激活指定注册页面。
     * @param registration 路由、生命周期策略和非空 factory。
     * @return 激活成功，或参数、线程及 factory 错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> activate(
        const ZzPageRegistration &registration);

    /** @brief 按当前页面策略幂等离开页面并清空活动路由。 */
    void deactivateCurrent() noexcept;

    /**
     * @brief 离开当前页并显示不包含技术细节的框架错误页。
     * @param failedRoute 创建失败的目标路由。
     * @return 显示成功，或参数及线程错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> showFrameworkError(
        ZzRouteId failedRoute);

    /**
     * @brief 返回当前页面或框架错误页对应的路由。
     * @return 当前路由；没有活动页或跨线程查询时返回无效路由。
     */
    [[nodiscard]] ZzRouteId currentRoute() const;

    /**
     * @brief 设置非活动 Recreatable 页面缓存容量。
     * @param capacity 大于或等于零的页面数；零表示离开即回收。
     * @return 设置成功，或参数及线程错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> setRecreatableCapacity(
        qsizetype capacity);

private:
    std::unique_ptr<ZzPageHostPrivate> d_ptr;
};

} // namespace ZzPureTools
