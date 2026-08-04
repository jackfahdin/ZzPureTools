#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QTypeInfo>

namespace ZzFluentUI {

/** @brief 描述可由 Widgets 或未来 Quick 前端渲染的图标资源。 */
struct ZzIconDescriptor final
{
    /** @brief Qt 资源路径或前端可识别的稳定资源标识。 */
    QString resourceId;

    /** @brief 从右到左布局中是否水平镜像。 */
    bool mirroredInRightToLeft = false;
};

} // namespace ZzFluentUI

Q_DECLARE_TYPEINFO(ZzFluentUI::ZzIconDescriptor, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzFluentUI::ZzIconDescriptor)
