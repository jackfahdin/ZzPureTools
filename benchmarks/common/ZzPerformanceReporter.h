#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QString>

#include "ZzBenchmarkSample.h"

#include <ZzCore/ZzResult.h>

namespace ZzBenchmarks {

/**
 * @brief 聚合性能样本并生成固定 schema 的 JSON 报告。
 */
class ZzPerformanceReporter final
{
public:
    /**
     * @brief 设置唯一场景名称。
     * @param scenario 非空的场景名称。
     */
    void setScenario(QString scenario);

    /**
     * @brief 记录未进入正式统计的预热迭代数。
     * @param count 非负预热次数。
     */
    void setWarmupIterations(qsizetype count);

    /**
     * @brief 添加一个有限数值样本。
     * @param sample 指标名、单位和值组成的样本。
     */
    void addSample(ZzBenchmarkSample sample);

    /**
     * @brief 添加环境指纹字段。
     * @param key schema 字段名。
     * @param value 保持 JSON 类型的字段值。
     */
    void addEnvironmentMetadata(
        const QString &key,
        const QJsonValue &value);

    /**
     * @brief 添加构建指纹字段。
     * @param key schema 字段名。
     * @param value 保持 JSON 类型的字段值。
     */
    void addBuildMetadata(
        const QString &key,
        const QJsonValue &value);

    /**
     * @brief 校验当前状态并生成报告对象。
     * @return 固定 schema 报告或参数错误。
     */
    [[nodiscard]] ZzCore::ZzResult<QJsonObject> report() const;

    /**
     * @brief 将已校验报告原子写入指定路径。
     * @param path 输出文件路径。
     * @return 成功或输入输出错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> write(const QString &path) const;

private:
    QString scenario_;
    qsizetype warmupIterations_ = 0;
    QList<ZzBenchmarkSample> samples_;
    QJsonObject environment_;
    QJsonObject build_;
};

} // namespace ZzBenchmarks
