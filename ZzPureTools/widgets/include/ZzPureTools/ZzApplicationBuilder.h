#pragma once

#include <memory>

#include <QtCore/QString>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureToolsExport.h>
#include <ZzPureTools/ZzRouteId.h>

namespace ZzPureTools {

class ZzApplicationBuilderPrivate;
class ZzPureApplication;

/**
 * @brief 收集模块、页面、导航和翻译配置并一次性构建应用。
 *
 * build() 无论成功或失败都会冻结当前 Builder。失败后只能用全新 Builder 重试，
 * 成功提交过的应用永久拒绝第二次构建。
 */
class ZZ_PURE_TOOLS_EXPORT ZzApplicationBuilder final
{
public:
    /** @brief 创建尚未冻结的空应用构建器。 */
    ZzApplicationBuilder();

    /** @brief 销毁尚未转移的模块和配置。 */
    ~ZzApplicationBuilder();

    /** @brief 禁止复制拥有模块的构建器。 */
    ZzApplicationBuilder(const ZzApplicationBuilder &) = delete;

    /** @brief 禁止复制赋值拥有模块的构建器。 */
    ZzApplicationBuilder &operator=(const ZzApplicationBuilder &) = delete;

    /** @brief 转移构建器及其全部尚未提交配置。 */
    ZzApplicationBuilder(ZzApplicationBuilder &&other) noexcept;

    /** @brief 转移赋值构建器及其全部尚未提交配置。 */
    ZzApplicationBuilder &operator=(
        ZzApplicationBuilder &&other) noexcept;

    /** @brief 增加一个由最终运行时独占的非空应用模块。 */
    [[nodiscard]] ZzCore::ZzResult<void> addModule(
        std::unique_ptr<ZzApplicationModule> module);

    /** @brief 增加一个拥有值的延迟页面注册项。 */
    [[nodiscard]] ZzCore::ZzResult<void> addPage(
        ZzPageRegistration registration);

    /** @brief 增加一个不持有页面实例的导航展示节点。 */
    [[nodiscard]] ZzCore::ZzResult<void> addNavigationNode(
        ZzNavigationNode node);

    /** @brief 设置唯一首窗首次导航路由。 */
    [[nodiscard]] ZzCore::ZzResult<void> setInitialRoute(
        ZzRouteId routeId);

    /** @brief 增加一个构建期加载、应用关闭时卸载的 translator 资源。 */
    [[nodiscard]] ZzCore::ZzResult<void> addTranslatorResource(
        QString resourcePath);

    /**
     * @brief 以两阶段 staging/commit 构建模块、翻译和首窗。
     * @param application 尚未成功构建且未关闭的唯一应用。
     * @return 构建成功，或验证、模块、翻译及窗口装配错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> build(
        ZzPureApplication &application);

    /** @brief 查询当前构建器是否已执行 build() 或已被移动。 */
    [[nodiscard]] bool isFrozen() const noexcept;

private:
    std::unique_ptr<ZzApplicationBuilderPrivate> d_ptr;
};

} // namespace ZzPureTools
