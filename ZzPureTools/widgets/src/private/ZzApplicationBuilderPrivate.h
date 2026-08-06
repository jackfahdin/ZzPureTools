#pragma once

#include <memory>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWindowSetupCallback.h>

namespace ZzPureTools {

class ZzPureApplication;

/** @brief 实现应用配置收集、完整验证和两阶段构建。 */
class ZzApplicationBuilderPrivate final
{
public:
    /** @brief 增加非空模块。 */
    [[nodiscard]] ZzCore::ZzResult<void> addModule(
        std::unique_ptr<ZzApplicationModule> module);

    /** @brief 增加页面注册值。 */
    [[nodiscard]] ZzCore::ZzResult<void> addPage(
        ZzPageRegistration registration);

    /** @brief 增加导航节点值。 */
    [[nodiscard]] ZzCore::ZzResult<void> addNavigationNode(
        ZzNavigationNode node);

    /** @brief 设置唯一初始路由。 */
    [[nodiscard]] ZzCore::ZzResult<void> setInitialRoute(
        ZzRouteId routeId);

    /** @brief 增加 translator 资源路径。 */
    [[nodiscard]] ZzCore::ZzResult<void> addTranslatorResource(
        QString resourcePath);

    /** @brief 设置唯一窗口装配回调。 */
    [[nodiscard]] ZzCore::ZzResult<void> setWindowSetupCallback(
        ZzWindowSetupCallback callback);

    /** @brief 冻结并以 staging/commit 构建应用。 */
    [[nodiscard]] ZzCore::ZzResult<void> build(
        ZzPureApplication &application);

    /** @brief 查询是否已经冻结。 */
    [[nodiscard]] bool isFrozen() const noexcept;

private:
    std::vector<std::unique_ptr<ZzApplicationModule>> modules_;
    QList<ZzPageRegistration> pages_;
    QList<ZzNavigationNode> nodes_;
    QStringList translatorResources_;
    ZzWindowSetupCallback windowSetupCallback_;
    ZzRouteId initialRoute_;
    bool initialRouteSet_ = false;
    bool windowSetupCallbackSet_ = false;
    bool frozen_ = false;
};

} // namespace ZzPureTools
