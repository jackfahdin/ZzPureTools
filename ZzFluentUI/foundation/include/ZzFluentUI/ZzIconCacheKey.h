#pragma once

#include <cstddef>

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QtTypes>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 标识一个与主题和设备像素比绑定的图标缓存项。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzIconCacheKey final
{
public:
    /**
     * @brief 创建覆盖全部渲染输入的缓存键。
     * @param resourceId 稳定资源标识。
     * @param mirrored 是否水平镜像。
     * @param logicalSize 逻辑像素尺寸。
     * @param dprBucket 百分之一单位的 DPR 桶。
     * @param rgba 图标着色 RGBA 值。
     * @param themeRevision 主题快照 revision。
     * @param sourceKind 图标来源的稳定整数表示。
     * @param glyph 字体图标 Unicode 码点；SVG 使用零。
     * @param originalColor 是否保留 SVG 原始颜色。
     */
    ZzIconCacheKey(
        QString resourceId,
        bool mirrored,
        QSize logicalSize,
        quint16 dprBucket,
        quint32 rgba,
        quint64 themeRevision,
        quint8 sourceKind = 0,
        quint32 glyph = 0,
        bool originalColor = false);

    /** @brief 返回稳定资源标识。 */
    [[nodiscard]] const QString &resourceId() const noexcept;

    /** @brief 返回是否水平镜像。 */
    [[nodiscard]] bool mirrored() const noexcept;

    /** @brief 返回逻辑像素尺寸。 */
    [[nodiscard]] QSize logicalSize() const noexcept;

    /** @brief 返回百分之一单位的 DPR 桶。 */
    [[nodiscard]] quint16 dprBucket() const noexcept;

    /** @brief 返回图标着色 RGBA 值。 */
    [[nodiscard]] quint32 rgba() const noexcept;

    /** @brief 返回主题快照 revision。 */
    [[nodiscard]] quint64 themeRevision() const noexcept;

    /** @brief 返回图标来源的稳定整数表示。 */
    [[nodiscard]] quint8 sourceKind() const noexcept;

    /** @brief 返回字体字形码点；SVG 返回零。 */
    [[nodiscard]] quint32 glyph() const noexcept;

    /** @brief 返回是否保留 SVG 原始颜色。 */
    [[nodiscard]] bool originalColor() const noexcept;

    /** @brief 比较全部渲染输入是否相同。 */
    friend bool operator==(
        const ZzIconCacheKey &,
        const ZzIconCacheKey &) = default;

private:
    QString resourceId_;
    bool mirrored_;
    QSize logicalSize_;
    quint16 dprBucket_;
    quint32 rgba_;
    quint64 themeRevision_;
    quint8 sourceKind_;
    quint32 glyph_;
    bool originalColor_;
};

/**
 * @brief 为 Qt 哈希容器计算完整缓存键哈希。
 * @param key 图标缓存键。
 * @param seed 调用方提供的初始种子。
 * @return 覆盖全部键字段的哈希值。
 */
[[nodiscard]] ZZ_FLUENT_FOUNDATION_EXPORT std::size_t qHash(
    const ZzIconCacheKey &key,
    std::size_t seed = 0) noexcept;

} // namespace ZzFluentUI
