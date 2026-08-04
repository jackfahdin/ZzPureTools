#pragma once

#include <QtCore/QString>

#include <ZzFluentUI/ZzIconDescriptor.h>

#include <ZzPureTools/ZzRouteId.h>

namespace ZzPureTools {

/** @brief 描述导航列表中的强类型路由和可重新翻译展示信息。 */
struct ZzNavigationNode final
{
    /** @brief 指向页面注册项的稳定强类型路由。 */
    ZzRouteId routeId;

    /** @brief 传递给 QCoreApplication::translate() 的非空上下文。 */
    QString titleTranslationContext;

    /** @brief 传递给 QCoreApplication::translate() 的非空源文案。 */
    QString titleSourceText;

    /** @brief 由当前前端样式解释和绘制的图标描述。 */
    ZzFluentUI::ZzIconDescriptor icon;
};

} // namespace ZzPureTools
