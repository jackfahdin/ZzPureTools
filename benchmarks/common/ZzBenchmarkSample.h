#pragma once

#include <QtCore/QString>

namespace ZzBenchmarks {

/**
 * @brief 保存一次 benchmark 指标采样。
 */
struct ZzBenchmarkSample final
{
    QString metric;
    QString unit;
    double value = 0.0;
};

} // namespace ZzBenchmarks
