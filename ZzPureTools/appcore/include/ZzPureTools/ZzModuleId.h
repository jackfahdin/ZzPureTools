#pragma once

#include <cstddef>

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QTypeInfo>

#include <ZzPureTools/ZzAppCoreExport.h>

namespace ZzPureTools {

/** @brief 保存稳定、拥有字符串的模块标识。 */
class ZZ_APP_CORE_EXPORT ZzModuleId final
{
public:
    /** @brief 创建无效的空模块标识。 */
    ZzModuleId() = default;

    /**
     * @brief 创建拥有修剪后文本的模块标识。
     * @param value 可移动的模块名称；纯空白值构造为无效标识。
     */
    explicit ZzModuleId(QString value);

    /** @brief 判断标识是否为非空、非纯空白值。 */
    [[nodiscard]] bool isValid() const noexcept;

    /** @brief 返回由当前值对象拥有的标识文本。 */
    [[nodiscard]] const QString &value() const noexcept;

    /** @brief 按完整标识文本比较两个模块标识。 */
    friend bool operator==(
        const ZzModuleId &,
        const ZzModuleId &) = default;

private:
    QString value_;
};

/**
 * @brief 为模块标识计算稳定的 Qt 哈希。
 * @param id 待哈希的拥有型模块标识。
 * @param seed 调用方提供的初始种子。
 * @return 完整标识文本的哈希值。
 */
[[nodiscard]] ZZ_APP_CORE_EXPORT std::size_t qHash(
    const ZzModuleId &id,
    std::size_t seed = 0) noexcept;

} // namespace ZzPureTools

Q_DECLARE_TYPEINFO(ZzPureTools::ZzModuleId, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzPureTools::ZzModuleId)
