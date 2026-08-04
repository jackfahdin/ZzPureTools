#pragma once

#include <memory>

#include <QtCore/QString>

#include <ZzCore/ZzSettingsStore.h>

namespace ZzCore {

class ZzQtSettingsStorePrivate;

/**
 * @brief 使用显式 INI 文件实现线程归属明确的设置存储。
 * @note 对象只能在构造线程访问和销毁；所有可报告失败都通过 ZzResult 返回。
 */
class ZZ_CORE_EXPORT ZzQtSettingsStore final : public ZzSettingsStore
{
public:
    /**
     * @brief 创建使用指定 INI 文件的设置存储。
     * @param filePath 设置文件路径，不能为空。
     */
    explicit ZzQtSettingsStore(const QString &filePath);

    /** @brief 释放设置后端；必须在构造线程调用。 */
    ~ZzQtSettingsStore() override;

    ZzQtSettingsStore(const ZzQtSettingsStore &) = delete;
    ZzQtSettingsStore &operator=(const ZzQtSettingsStore &) = delete;
    ZzQtSettingsStore(ZzQtSettingsStore &&) = delete;
    ZzQtSettingsStore &operator=(ZzQtSettingsStore &&) = delete;

    /**
     * @brief 读取设置值。
     * @param key 非空设置键。
     * @param defaultValue 键不存在时返回的值。
     * @return 成功值，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<QVariant> read(
        QStringView key,
        const QVariant &defaultValue = {}) const override;

    /**
     * @brief 写入设置值。
     * @param key 非空设置键。
     * @param value 要保存的值。
     * @return 成功状态，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<void> write(
        QStringView key,
        const QVariant &value) override;

    /**
     * @brief 删除指定设置键。
     * @param key 非空设置键。
     * @return 成功状态，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<void> remove(QStringView key) override;

    /**
     * @brief 把待处理修改同步到 INI 文件。
     * @return 成功状态，或线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<void> sync() override;

private:
    std::unique_ptr<ZzQtSettingsStorePrivate> d_ptr;
};

} // namespace ZzCore
