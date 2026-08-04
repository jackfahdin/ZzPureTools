#pragma once

#include <ZzCore/ZzResult.h>

#include <ZzWindowKit/ZzWindowKitExport.h>

namespace ZzWindowKit {

/**
 * @brief 在应用对象创建前设置无边框窗口所需的进程级 Qt 属性。
 *
 * 本类型无状态且不可实例化。prepare() 应由应用生命周期线程在构造
 * QCoreApplication 或 QApplication 之前调用。
 */
class ZZ_WINDOWKIT_EXPORT ZzWindowKitBootstrap final
{
public:
    /** @brief 禁止构造无状态初始化工具。 */
    ZzWindowKitBootstrap() = delete;

    /**
     * @brief 设置无边框 QWidget 所需的应用创建前属性。
     * @return 提前调用时成功；应用对象已经存在时返回 InvalidState。
     *
     * 在应用对象创建前重复调用保持成功且不会产生额外状态。
     */
    [[nodiscard]] static ZzCore::ZzResult<void> prepare();
};

} // namespace ZzWindowKit
