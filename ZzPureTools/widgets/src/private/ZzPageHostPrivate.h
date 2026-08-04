#pragma once

#include <list>
#include <map>
#include <memory>

#include <QtCore/QtGlobal>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzRouteId.h>

class QStackedWidget;
class QWidget;

namespace ZzPureTools {

class ZzPageHost;

/** @brief 保存一个已创建页面的策略和唯一实例。 */
struct ZzPageEntry final
{
    ZzPageLifetimePolicy policy = ZzPageLifetimePolicy::Recreatable;
    std::unique_ptr<ZzPageInstance> instance;
};

/** @brief 实现事务式页面切换和容量受限的可重建页面缓存。 */
class ZzPageHostPrivate final
{
public:
    /** @brief 创建页面堆栈和通用框架错误页。 */
    explicit ZzPageHostPrivate(ZzPageHost *host);

    /** @brief 在 QWidget 父子树销毁前清理全部页面实例。 */
    ~ZzPageHostPrivate();

    /** @brief 事务式激活指定页面。 */
    [[nodiscard]] ZzCore::ZzResult<void> activate(
        const ZzPageRegistration &registration);

    /** @brief 按策略离开当前页。 */
    void deactivateCurrent() noexcept;

    /** @brief 离开当前页并显示通用错误页。 */
    [[nodiscard]] ZzCore::ZzResult<void> showFrameworkError(
        ZzRouteId failedRoute);

    /** @brief 返回当前活动页或错误页路由。 */
    [[nodiscard]] ZzRouteId currentRoute() const;

    /** @brief 设置可重建页面缓存容量并立即缩容。 */
    [[nodiscard]] ZzCore::ZzResult<void> setRecreatableCapacity(
        qsizetype capacity);

private:
    /** @brief 检查调用是否位于宿主所属线程。 */
    [[nodiscard]] bool isOwnerThread() const noexcept;

    /** @brief 校验路由、策略和 factory。 */
    [[nodiscard]] ZzCore::ZzResult<void> validateRegistration(
        const ZzPageRegistration &registration) const;

    /** @brief 捕获 factory 异常并清理本次新增的页面父对象子节点。 */
    [[nodiscard]] ZzCore::ZzResult<std::unique_ptr<ZzPageInstance>>
    createPage(const ZzPageRegistration &registration);

    /** @brief 执行不含线程检查的当前页面离开流程。 */
    void deactivateCurrentUnchecked() noexcept;

    /** @brief 从可重建页面 LRU 中移除指定路由。 */
    void removeFromRecreatableLru(const QString &routeKey) noexcept;

    /** @brief 驱逐超过当前容量的最旧非活动可重建页面。 */
    void evictRecreatablePages() noexcept;

    ZzPageHost *const q_ptr;
    QStackedWidget *stack;
    QWidget *frameworkErrorWidget;
    std::map<QString, ZzPageEntry> pages;
    std::list<QString> recreatableLru;
    ZzRouteId activeRoute;
    qsizetype recreatableCapacity = 3;
    bool showingFrameworkError = false;
};

} // namespace ZzPureTools
