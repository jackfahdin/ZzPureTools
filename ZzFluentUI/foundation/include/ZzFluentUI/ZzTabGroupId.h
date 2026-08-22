#pragma once

#include <cstddef>

#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 表示 trim 后非空、可比较和可哈希的标签组稳定标识。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzTabGroupId final
{
public:
    /** @brief 构造无效的空标识。 */
    ZzTabGroupId() = default;

    /**
     * @brief 构造并 trim 标签组标识。
     * @param value 标签组稳定标识；全空白会得到无效值。
     */
    explicit ZzTabGroupId(QString value);

    /**
     * @brief 返回标识是否非空。
     * @return 规范化值非空时返回 true。
     */
    [[nodiscard]] bool isValid() const noexcept;

    /**
     * @brief 返回规范化后的稳定字符串。
     * @return 去除首尾空白后的标识值。
     */
    [[nodiscard]] const QString &value() const noexcept;

    friend bool operator==(
        const ZzTabGroupId &,
        const ZzTabGroupId &) = default;

private:
    QString value_;
};

/**
 * @brief 将标签组标识转发给 QString 的稳定进程内哈希。
 * @param id 标签组标识。
 * @param seed 哈希种子。
 * @return 可供 Qt 哈希容器使用的哈希值。
 */
[[nodiscard]] ZZ_FLUENT_FOUNDATION_EXPORT size_t qHash(
    const ZzTabGroupId &id,
    size_t seed = 0) noexcept;

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzTabGroupId)
