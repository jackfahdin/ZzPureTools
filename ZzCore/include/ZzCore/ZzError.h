#pragma once

#include <memory>

#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <ZzCore/ZzCoreExport.h>
#include <ZzCore/ZzErrorCode.h>

namespace ZzCore {

class ZzErrorPrivate;

/**
 * @brief 保存技术错误信息，不直接携带最终用户文案。
 *
 * 对象具有深复制值语义。移动后的对象仍可析构、赋值、复制和安全查询，此时按无错误
 * 处理。
 */
class ZZ_CORE_EXPORT ZzError final
{
public:
    /** @brief 构造无错误值。 */
    ZzError();

    /**
     * @brief 构造错误值。
     * @param code 稳定错误码；技术信息非空时不得为 None。
     * @param technicalMessage 面向开发者和日志的技术信息。
     * @param context 可选的结构化来源、参数或路径上下文。
     */
    ZzError(
        ZzErrorCode code,
        QString technicalMessage,
        QString context = {});

    /**
     * @brief 深复制错误值。
     * @param other 被复制的错误值。
     */
    ZzError(const ZzError &other);

    /**
     * @brief 移动错误值。
     * @param other 被移动的错误值。
     */
    ZzError(ZzError &&other) noexcept;

    /**
     * @brief 深复制赋值并提供强异常安全保证。
     * @param other 被复制的错误值。
     * @return 当前对象引用。
     */
    ZzError &operator=(const ZzError &other);

    /**
     * @brief 移动赋值。
     * @param other 被移动的错误值。
     * @return 当前对象引用。
     */
    ZzError &operator=(ZzError &&other) noexcept;

    /** @brief 销毁错误值。 */
    ~ZzError();

    /**
     * @brief 查询对象是否表示失败。
     * @return 错误码不为 None 时返回 true。
     */
    [[nodiscard]] bool isError() const noexcept;

    /**
     * @brief 获取稳定错误码。
     * @return 错误码；移动后的对象返回 None。
     */
    [[nodiscard]] ZzErrorCode code() const noexcept;

    /**
     * @brief 获取技术错误信息。
     * @return 技术信息副本；移动后的对象返回空字符串。
     */
    [[nodiscard]] QString technicalMessage() const;

    /**
     * @brief 获取错误上下文。
     * @return 上下文副本；移动后的对象返回空字符串。
     */
    [[nodiscard]] QString context() const;

private:
    std::unique_ptr<ZzErrorPrivate> d_ptr;
};

} // namespace ZzCore

Q_DECLARE_METATYPE(ZzCore::ZzError)
