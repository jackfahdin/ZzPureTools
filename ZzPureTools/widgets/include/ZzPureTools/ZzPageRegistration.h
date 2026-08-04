#pragma once

#include <functional>
#include <memory>

#include <QtWidgets/QWidget>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzRouteId.h>

namespace ZzPureTools {

/**
 * @brief 延迟创建一个页面实例。
 * @param pageParent factory 创建 View 时必须使用的唯一 QWidget 父对象。
 * @return 完整页面实例，或不向宿主遗留子对象的失败结果。
 */
using ZzPageFactory = std::function<ZzCore::ZzResult<
    std::unique_ptr<ZzPageInstance>>(QWidget *pageParent)>;

/** @brief 描述强类型页面路由、生命周期策略和延迟 factory。 */
struct ZzPageRegistration final
{
    /** @brief 非空且在应用窗口内唯一的页面路由。 */
    ZzRouteId routeId;

    /** @brief 页面离开活动状态后的实例保留策略。 */
    ZzPageLifetimePolicy lifetime = ZzPageLifetimePolicy::Recreatable;

    /** @brief 仅在首次访问或缓存回收后调用的页面 factory。 */
    ZzPageFactory factory;
};

} // namespace ZzPureTools
