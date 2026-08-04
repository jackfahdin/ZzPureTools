#pragma once

#include <QtCore/QStringView>
#include <QtCore/QVariant>

#include <ZzCore/ZzCoreExport.h>
#include <ZzCore/ZzResult.h>

namespace ZzCore {

/**
 * @brief 可注入的键值设置存储接口。
 *
 * 具体实现负责定义线程归属和持久化时机；所有预期失败都通过 ZzResult 返回。
 */
class ZZ_CORE_EXPORT ZzSettingsStore
{
public:
    /** @brief 释放设置存储。 */
    virtual ~ZzSettingsStore() = default;

    /**
     * @brief 读取设置值。
     * @param key 非空设置键。
     * @param defaultValue 键不存在时返回的值。
     * @return 成功值，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] virtual ZzResult<QVariant> read(
        QStringView key,
        const QVariant &defaultValue = {}) const = 0;

    /**
     * @brief 写入设置值。
     * @param key 非空设置键。
     * @param value 要保存的值。
     * @return 成功状态，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] virtual ZzResult<void> write(
        QStringView key,
        const QVariant &value) = 0;

    /**
     * @brief 删除设置键。
     * @param key 非空设置键。
     * @return 成功状态，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] virtual ZzResult<void> remove(QStringView key) = 0;

    /**
     * @brief 把待处理修改同步到持久化后端。
     * @return 成功状态，或线程与 I/O 错误。
     */
    [[nodiscard]] virtual ZzResult<void> sync() = 0;
};

} // namespace ZzCore
