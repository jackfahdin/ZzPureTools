#pragma once

#include <cstddef>

#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <ZzPureTools/ZzPureToolsExport.h>

namespace ZzPureTools {

/** @brief 表示 trim 后非空、可持久化和哈希的工作区面板标识。 */
class ZZ_PURE_TOOLS_EXPORT ZzWorkspacePanelId final
{
public:
    /** @brief 构造无效的空标识。 */
    ZzWorkspacePanelId() = default;

    /**
     * @brief 构造并 trim 面板标识。
     * @param value 面板稳定标识；全空白会得到无效值。
     */
    explicit ZzWorkspacePanelId(QString value);

    /** @brief 返回标识是否非空。 */
    [[nodiscard]] bool isValid() const noexcept;

    /** @brief 返回规范化后的稳定字符串。 */
    [[nodiscard]] const QString &value() const noexcept;

    friend bool operator==(
        const ZzWorkspacePanelId &,
        const ZzWorkspacePanelId &) = default;

private:
    QString value_;
};

/** @brief 将工作区面板标识转发给 QString 的稳定进程内哈希。 */
[[nodiscard]] ZZ_PURE_TOOLS_EXPORT size_t qHash(
    const ZzWorkspacePanelId &id,
    size_t seed = 0) noexcept;

} // namespace ZzPureTools

Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspacePanelId)
