#pragma once

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 初始化由静态库或动态库携带的图标资源。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzIconAssets final
{
public:
    ZzIconAssets() = delete;

    /**
     * @brief 确保字体和 SVG Qt Resource 已注册到当前进程。
     * @return 关键字体和 SVG 资源均可访问时返回 true。
     * @note 可重复调用；首次调用后只读取缓存结果。
     */
    [[nodiscard]] static bool ensureInitialized();
};

} // namespace ZzFluentUI
