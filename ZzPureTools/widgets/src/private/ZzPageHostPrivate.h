#pragma once

#include <list>
#include <map>
#include <memory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>

namespace ZzPureTools {

/** @brief 保存已创建页面及其生命周期策略。 */
struct ZzPageEntry final
{
    ZzPageLifetimePolicy policy = ZzPageLifetimePolicy::Recreatable;
    std::unique_ptr<ZzPageInstance> instance;
    QMetaObject::Connection destroyed;
};

/** @brief 保护同步页面事务，管理稳定 QWidget 宿主和容量受限缓存。 */
class ZzPageHostPrivate final : public QObject
{
public:
    using Mode = ZzPageHost::ZzTransitionMode;
    using Direction = ZzPageHost::ZzTransitionDirection;
    /** @brief 创建固定页面宿主和通用错误页。 */
    explicit ZzPageHostPrivate(ZzPageHost *owner);
    /** @brief 在 QWidget 子树销毁前清理展示对象。 */
    ~ZzPageHostPrivate() override;
    /** @brief 创建目标成功后同步提交页面。 */
    ZzCore::ZzResult<void> activate(const ZzPageRegistration &registration, Direction direction);
    /** @brief 重入时不执行的幂等离开操作。 */
    void deactivateCurrent() noexcept;
    /** @brief 直接显示框架错误页。 */
    ZzCore::ZzResult<void> showFrameworkError(ZzRouteId route);
    /** @brief 设置缓存容量并立即驱逐溢出页。 */
    ZzCore::ZzResult<void> setRecreatableCapacity(qsizetype capacity);
    /** @brief 校验所属线程及同步事务占用。 */
    ZzCore::ZzResult<void> validateOperation() const;
    /** @brief 检查线程，查询操作不受事务锁限制。 */
    bool isOwnerThread() const noexcept;
    /** @brief 查询有效的活动页面。 */
    QWidget *currentView() const;
    /** @brief 同步显隐与几何，所有回调后检查存活。 */
    void synchronizePages();
    /** @brief 聚合页面建议尺寸。 */
    QSize hint(bool minimum) const;
    /** @brief 观察尺寸请求及页面父对象变化。 */
    bool eventFilter(QObject *watched, QEvent *event) override;
    /** @brief 结束视觉状态，恢复最新逻辑页。 */
    void settle();

    ZzPageHost *const q;
    QWidget *host;
    QWidget *errorView;
    std::map<QString, ZzPageEntry> pages;
    std::list<QString> lru;
    ZzRouteId activeRoute;
    qsizetype capacity = 3;
    Mode mode = Mode::None;
    int duration = -1;
    QSet<QString> disabledRoutes;
    bool busy = false;
    bool destroying = false;
    bool showingError = false;

private:
    /** @brief 校验注册并保留 factory 失败时的原页面。 */
    ZzCore::ZzResult<std::unique_ptr<ZzPageInstance>> createPage(const ZzPageRegistration &registration);
    /** @brief 先摘下记录，再调用可重入的销毁操作。 */
    void erasePage(const QString &key);
    /** @brief 已持有事务锁时按策略离开旧页。 */
    void leave();
    /** @brief 驱逐 LRU 尾部以外的最早页面。 */
    void evict();
    /** @brief 移除外部销毁或迁出的页面记录。 */
    void pageLost(const QString &key);
};

} // namespace ZzPureTools
