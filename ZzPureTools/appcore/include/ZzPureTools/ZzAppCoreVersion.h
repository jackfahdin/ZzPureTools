#pragma once

#include <QtCore/QString>

#include <ZzPureTools/ZzAppCoreExport.h>

namespace ZzPureTools {

/**
 * @brief 提供 ZzAppCore 的运行时版本信息。
 */
class ZZ_APP_CORE_EXPORT ZzAppCoreVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求应用运行时存在。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzPureTools
