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
    quint64 themeRevision)
    : resourceId_(std::move(resourceId))
    , mirrored_(mirrored)
    , logicalSize_(logicalSize)
    , dprBucket_(dprBucket)
    , rgba_(rgba)
    , themeRevision_(themeRevision)
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
        key.themeRevision());
}

} // namespace ZzFluentUI
