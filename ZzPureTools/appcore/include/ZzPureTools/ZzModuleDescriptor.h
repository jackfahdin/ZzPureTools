#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include <ZzPureTools/ZzAppCoreExport.h>
#include <ZzPureTools/ZzModuleId.h>

namespace ZzPureTools {

/** @brief 描述模块的稳定身份、版本和直接依赖。 */
struct ZZ_APP_CORE_EXPORT ZzModuleDescriptor final
{
    /** @brief 非空且全局唯一的模块标识。 */
    ZzModuleId id;

    /** @brief 由模块实现提供的展示或诊断版本。 */
    QString version;

    /** @brief 必须先于当前模块启动的直接依赖。 */
    QList<ZzModuleId> dependencies;
};

} // namespace ZzPureTools
