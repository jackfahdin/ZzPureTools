#pragma once

#include <QtCore/QString>

#include <ZzWindowKit/ZzWindowKitExport.h>

namespace ZzWindowKit {

/**
 * @brief 提供 ZzWindowKit 的运行时版本信息。
 */
class ZZ_WINDOWKIT_EXPORT ZzWindowKitVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求窗口系统初始化。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzWindowKit
