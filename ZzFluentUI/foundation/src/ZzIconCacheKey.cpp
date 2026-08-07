#include <ZzFluentUI/ZzIconCacheKey.h>

#include <utility>

#include <QtCore/QHashFunctions>

namespace ZzFluentUI {

ZzIconCacheKey::ZzIconCacheKey(
    QString resourceId,
    bool mirrored,
    QSize logicalSize,
    quint16 dprBucket,
    quint32 rgba,
    quint64 themeRevision,
    quint8 sourceKind,
    quint32 glyph,
    bool originalColor)
    : resourceId_(std::move(resourceId))
    , mirrored_(mirrored)
    , logicalSize_(logicalSize)
    , dprBucket_(dprBucket)
    , rgba_(rgba)
    , themeRevision_(themeRevision)
    , sourceKind_(sourceKind)
    , glyph_(glyph)
    , originalColor_(originalColor)
{
}

const QString &ZzIconCacheKey::resourceId() const noexcept
{
    return resourceId_;
}

bool ZzIconCacheKey::mirrored() const noexcept
{
    return mirrored_;
}

QSize ZzIconCacheKey::logicalSize() const noexcept
{
    return logicalSize_;
}

quint16 ZzIconCacheKey::dprBucket() const noexcept
{
    return dprBucket_;
}

quint32 ZzIconCacheKey::rgba() const noexcept
{
    return rgba_;
}

quint64 ZzIconCacheKey::themeRevision() const noexcept
{
    return themeRevision_;
}

quint8 ZzIconCacheKey::sourceKind() const noexcept
{
    return sourceKind_;
}

quint32 ZzIconCacheKey::glyph() const noexcept
{
    return glyph_;
}

bool ZzIconCacheKey::originalColor() const noexcept
{
    return originalColor_;
}

std::size_t qHash(const ZzIconCacheKey &key, std::size_t seed) noexcept
{
    return qHashMulti(
        seed,
        key.resourceId(),
        key.mirrored(),
        key.logicalSize().width(),
        key.logicalSize().height(),
        key.dprBucket(),
        key.rgba(),
        key.themeRevision(),
        key.sourceKind(),
        key.glyph(),
        key.originalColor());
}

} // namespace ZzFluentUI
