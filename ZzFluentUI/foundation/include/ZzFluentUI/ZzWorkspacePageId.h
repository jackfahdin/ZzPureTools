#pragma once

#include <cstddef>

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringView>
#include <QtCore/QUuid>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 表示工作区页面的稳定 UUID 标识。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzWorkspacePageId final
{
public:
    /** @brief 构造无效的页面标识。 */
    ZzWorkspacePageId() = default;

    /**
     * @brief 创建新的随机页面标识。
     * @return 新生成的有效页面标识。
     */
    [[nodiscard]] static ZzWorkspacePageId create();

    /**
     * @brief 从规范 UUID 字符串解析页面标识。
     * @param value 接受带或不带花括号的规范 UUID。
     * @return 输入无效时返回无效标识。
     */
    [[nodiscard]] static ZzWorkspacePageId fromString(QStringView value);

    /**
     * @brief 返回标识是否有效。
     * @return UUID 非空时返回 true。
     */
    [[nodiscard]] bool isValid() const noexcept;

    /**
     * @brief 返回不带花括号的小写规范 UUID 字符串。
     * @return 页面标识字符串。
     */
    [[nodiscard]] QString toString() const;

    /** @brief 比较两个页面标识是否相等。 */
    friend bool operator==(
        const ZzWorkspacePageId &,
        const ZzWorkspacePageId &) = default;

private:
    friend ZZ_FLUENT_FOUNDATION_EXPORT std::size_t qHash(
        const ZzWorkspacePageId &,
        std::size_t) noexcept;

    explicit ZzWorkspacePageId(QUuid value) noexcept;

    QUuid value_;
};

/**
 * @brief 返回页面标识的 Qt 哈希值。
 * @param id 页面标识。
 * @param seed 哈希种子。
 * @return 可供 Qt 哈希容器使用的哈希值。
 */
[[nodiscard]] ZZ_FLUENT_FOUNDATION_EXPORT std::size_t qHash(
    const ZzWorkspacePageId &id,
    std::size_t seed = 0) noexcept;

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzWorkspacePageId)
