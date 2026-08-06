#pragma once

#include <memory>

#include <QtCore/QString>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzRouteId.h>

class QWidget;

namespace ZzExample {

class ZzExampleApplicationContext;

/** @brief 创建可逐批替换且不直接访问业务服务的初始页面 View。 */
class ZzExamplePageFactory final
{
public:
    /** @brief 禁止实例化无状态页面工厂。 */
    ZzExamplePageFactory() = delete;

    /**
     * @brief 创建携带只读展示快照的轻量占位页面。
     * @param routeId 页面稳定路由。
     * @param title 已解析的默认页面标题。
     * @param context 非空应用上下文，仅用于生成 ViewModel 初始快照。
     * @param pageParent View 必须使用的非空 Qt 父对象。
     * @return 完整页面实例，或输入与所有权错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<std::unique_ptr<
        ZzPureTools::ZzPageInstance>> createPlaceholder(
        const ZzPureTools::ZzRouteId &routeId,
        QString title,
        const std::shared_ptr<ZzExampleApplicationContext> &context,
        QWidget *pageParent);
};

} // namespace ZzExample
