#pragma once

#include <ZzCore/ZzResult.h>

class QScreen;

namespace ZzBenchmarks {

class ZzPerformanceReporter;

/**
 * @brief 从当前 Linux 进程、显示和构建环境采集统一性能元数据。
 */
class ZzBenchmarkMetadata final
{
public:
    ZzBenchmarkMetadata() = delete;

    /**
     * @brief 校验环境并把完整元数据写入 reporter。
     * @param reporter 接收元数据且不转移所有权的报告器。
     * @param screen 当前 benchmark 所在屏幕的非拥有指针。
     * @return 成功或带具体错误码的失败结果。
     */
    [[nodiscard]] static ZzCore::ZzResult<void> populate(
        ZzPerformanceReporter &reporter,
        const QScreen *screen);
};

} // namespace ZzBenchmarks
