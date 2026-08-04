#pragma once

#include <map>

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QtGlobal>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzRouteId.h>

class QParallelAnimationGroup;
class QVariantAnimation;

namespace ZzPureTools {

class ZzNavigationController;
class ZzNavigationModel;
class ZzPageHost;

/** @brief 实现路由注册、页面失败处理、动画复用和窗口级有界历史。 */
class ZzNavigationControllerPrivate final
{
public:
    /** @brief 绑定非拥有的导航模型和页面宿主。 */
    ZzNavigationControllerPrivate(
        ZzNavigationController *controller,
        ZzNavigationModel *navigationModel,
        ZzPageHost *pageHost);

    /** @brief 停止仍在运行的页面过渡。 */
    ~ZzNavigationControllerPrivate();

    /** @brief 一次性校验并保存页面注册表。 */
    [[nodiscard]] ZzCore::ZzResult<void> setRegistrations(
        QList<ZzPageRegistration> registrations);

    /** @brief 导航到强类型路由并维护错误页和历史。 */
    [[nodiscard]] ZzCore::ZzResult<void> navigate(ZzRouteId routeId);

    /** @brief 成功激活历史目标后移除最后一项。 */
    [[nodiscard]] ZzCore::ZzResult<void> goBack();

    /** @brief 查询当前窗口是否可以回退。 */
    [[nodiscard]] bool canGoBack() const noexcept;

    /** @brief 返回页面宿主当前路由。 */
    [[nodiscard]] ZzRouteId currentRoute() const;

    /** @brief 设置并立即裁剪历史容量。 */
    [[nodiscard]] ZzCore::ZzResult<void> setHistoryCapacity(
        qsizetype capacity);

private:
    /** @brief 验证对象、观察值和调用线程仍有效。 */
    [[nodiscard]] ZzCore::ZzResult<void> validateOperation() const;

    /** @brief 向历史尾部追加路由并裁剪最旧项。 */
    void appendHistory(const ZzRouteId &routeId);

    /** @brief 停止并复用唯一页面过渡动画组。 */
    void restartTransition();

    /** @brief 记录技术错误并发出导航失败信号。 */
    void reportNavigationFailure(const ZzCore::ZzError &error);

    ZzNavigationController *const q_ptr;
    QPointer<ZzNavigationModel> model;
    QPointer<ZzPageHost> host;
    std::map<QString, ZzPageRegistration> registrations;
    QList<ZzRouteId> history;
    qsizetype historyCapacity = 100;
    QParallelAnimationGroup *transition;
    QVariantAnimation *transitionProgress;
    bool registrationsSet = false;
    bool showingFrameworkError = false;
};

} // namespace ZzPureTools
