#pragma once

#include <QtCore/QString>

#include <ZzCore/ZzCoreExport.h>

namespace ZzCore {

/**
 * @brief 提供 ZzCore 的运行时版本信息。
 */
class ZZ_CORE_EXPORT ZzCoreVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求预先初始化。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzCore
