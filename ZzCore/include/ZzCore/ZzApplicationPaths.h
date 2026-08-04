#pragma once

#include <memory>

#include <QtCore/QString>

#include <ZzCore/ZzCoreExport.h>
#include <ZzCore/ZzResult.h>

namespace ZzCore {

class ZzApplicationPathsPrivate;

/**
 * @brief 根据显式组织名和应用名生成跨平台应用目录。
 *
 * 本类不读取或修改 QCoreApplication 的进程级名称。对象具有深复制值语义，目录创建
 * 由调用者通过 ensureDirectories() 显式触发。
 */
class ZZ_CORE_EXPORT ZzApplicationPaths final
{
public:
    /**
     * @brief 构造应用目录集合。
     * @param organizationName 单段组织名，不得为空、`.`、`..` 或包含路径分隔符。
     * @param applicationName 单段应用名，不得为空、`.`、`..` 或包含路径分隔符。
     */
    ZzApplicationPaths(QString organizationName, QString applicationName);

    /**
     * @brief 深复制应用目录集合。
     * @param other 被复制的对象。
     */
    ZzApplicationPaths(const ZzApplicationPaths &other);

    /**
     * @brief 深复制赋值。
     * @param other 被复制的对象。
     * @return 当前对象引用。
     */
    ZzApplicationPaths &operator=(const ZzApplicationPaths &other);

    /** @brief 销毁应用目录集合。 */
    ~ZzApplicationPaths();

    /**
     * @brief 获取配置目录。
     * @return 规范化目录；构造参数非法时返回空字符串。
     */
    [[nodiscard]] QString configDirectory() const;

    /**
     * @brief 获取持久数据目录。
     * @return 规范化目录；构造参数非法时返回空字符串。
     */
    [[nodiscard]] QString dataDirectory() const;

    /**
     * @brief 获取缓存目录。
     * @return 规范化目录；构造参数非法时返回空字符串。
     */
    [[nodiscard]] QString cacheDirectory() const;

    /**
     * @brief 获取日志目录。
     * @return 数据目录下的 logs 子目录；构造参数非法时返回空字符串。
     */
    [[nodiscard]] QString logDirectory() const;

    /**
     * @brief 创建配置、数据、缓存和日志目录。
     * @return 全部目录可用时成功，否则返回参数或 I/O 错误及失败路径。
     */
    [[nodiscard]] ZzResult<void> ensureDirectories() const;

private:
    std::unique_ptr<ZzApplicationPathsPrivate> d_ptr;
};

} // namespace ZzCore
