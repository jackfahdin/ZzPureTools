#pragma once

#include <QtCore/QString>

#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>

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

    /** @brief 可选分区标题使用的翻译上下文；必须与源文案同时设置。 */
    QString sectionTranslationContext = {};

    /** @brief 可选分区标题使用的翻译源文案；必须与上下文同时设置。 */
    QString sectionSourceText = {};

    /** @brief 最多八个 UTF-16 code unit 的可选短徽标文本。 */
    QString badgeText = {};

    /** @brief 指定节点位于主导航区或固定页脚区。 */
    ZzFluentUI::ZzNavigationPlacement placement =
        ZzFluentUI::ZzNavigationPlacement::Primary;
};

} // namespace ZzPureTools
