#pragma once

#include <cstddef>

#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <ZzPureTools/ZzPureToolsExport.h>

namespace ZzPureTools {

/** @brief 表示 trim 后非空、可哈希的固定工作区 Activity 标识。 */
class ZZ_PURE_TOOLS_EXPORT ZzWorkspaceActivityId final
{
public:
    /** @brief 构造无效的空标识。 */
    ZzWorkspaceActivityId() = default;

    /**
     * @brief 构造并 trim 固定 Activity 标识。
     * @param value Activity 稳定标识；全空白会得到无效值。
     */
    explicit ZzWorkspaceActivityId(QString value);

    /** @brief 返回标识是否非空。 */
    [[nodiscard]] bool isValid() const noexcept;

    /** @brief 返回规范化后的稳定字符串。 */
    [[nodiscard]] const QString &value() const noexcept;

    friend bool operator==(
        const ZzWorkspaceActivityId &,
        const ZzWorkspaceActivityId &) = default;

private:
    QString value_;
};

/** @brief 将固定 Activity 标识转发给 QString 的稳定进程内哈希。 */
[[nodiscard]] ZZ_PURE_TOOLS_EXPORT size_t qHash(
    const ZzWorkspaceActivityId &id,
    size_t seed = 0) noexcept;

} // namespace ZzPureTools

Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceActivityId)
