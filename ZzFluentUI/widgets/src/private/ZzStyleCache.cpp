#include "ZzStyleCache.h"

#include <utility>

#include <QtCore/QtGlobal>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzStyleCache::ZzStyleCache(int maximumIconBytes)
    : icons_(qMax(0, maximumIconBytes))
{
}

void ZzStyleCache::rebuildVisuals(const ZzThemeSnapshot &snapshot)
{
    const QBrush stroke(snapshot.color(ZzColorToken::ControlStroke));
    visuals_[0] = {
        snapshot.color(ZzColorToken::ControlFill),
        stroke};
    visuals_[1] = {
        snapshot.color(ZzColorToken::ControlFillHover),
        stroke};
    visuals_[2] = {
        snapshot.color(ZzColorToken::ControlFillPressed),
        stroke};
    visuals_[3] = {
        snapshot.color(ZzColorToken::ControlFillDisabled),
        stroke};
}

const ZzStyleVisual &ZzStyleCache::visual(
    std::size_t stateIndex) const noexcept
{
    if (stateIndex >= visuals_.size()) {
        Q_ASSERT(false);
        return visuals_[0];
    }
    return visuals_[stateIndex];
}

const QPixmap *ZzStyleCache::icon(
    const ZzIconCacheKey &key) const noexcept
{
    return icons_.object(key);
}

void ZzStyleCache::insertIcon(
    const ZzIconCacheKey &key,
    QPixmap pixmap)
{
    const int bytes = iconCost(pixmap.size());
    if (bytes <= 0) {
        return;
    }
    icons_.insert(
        key,
        new QPixmap(std::move(pixmap)),
        bytes);
}

bool ZzStyleCache::canCacheIcon(QSize physicalSize) const noexcept
{
    return iconCost(physicalSize) > 0;
}

void ZzStyleCache::clearIcons() noexcept
{
    icons_.clear();
}

int ZzStyleCache::iconBytes() const noexcept
{
    return static_cast<int>(icons_.totalCost());
}

int ZzStyleCache::iconCost(QSize physicalSize) const noexcept
{
    const int width = physicalSize.width();
    const int height = physicalSize.height();
    const int maximum = static_cast<int>(icons_.maxCost());
    if (width <= 0 || height <= 0 || maximum <= 0) {
        return 0;
    }

    const qint64 bytesPerRow = static_cast<qint64>(width) * 4;
    if (bytesPerRow > maximum
        || height > maximum / bytesPerRow) {
        return 0;
    }
    return static_cast<int>(
        bytesPerRow * static_cast<qint64>(height));
}

} // namespace ZzFluentUI
