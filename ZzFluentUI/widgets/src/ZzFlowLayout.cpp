#include <ZzFluentUI/ZzFlowLayout.h>

#include <algorithm>

#include "private/ZzFlowLayoutPrivate.h"

namespace ZzFluentUI {

namespace {

/** @brief 把公开 spacing 输入规范为 style 跟随值或非负值。 */
int zzNormalizedFlowSpacing(int spacing) noexcept
{
    return std::max(-1, spacing);
}

} // namespace

ZzFlowLayout::ZzFlowLayout(QWidget *parent)
    : QLayout(parent)
    , d_ptr(std::make_unique<ZzFlowLayoutPrivate>(this))
{
}

ZzFlowLayout::ZzFlowLayout(
    int horizontalSpacing,
    int verticalSpacing,
    QWidget *parent)
    : ZzFlowLayout(parent)
{
    d_ptr->horizontalSpacing = zzNormalizedFlowSpacing(
        horizontalSpacing);
    d_ptr->verticalSpacing = zzNormalizedFlowSpacing(verticalSpacing);
}

ZzFlowLayout::~ZzFlowLayout() = default;

int ZzFlowLayout::horizontalSpacing() const noexcept
{
    return d_ptr->horizontalSpacing;
}

void ZzFlowLayout::setHorizontalSpacing(int spacing)
{
    const int normalized = zzNormalizedFlowSpacing(spacing);
    if (d_ptr->horizontalSpacing == normalized) {
        return;
    }
    d_ptr->horizontalSpacing = normalized;
    invalidate();
    Q_EMIT horizontalSpacingChanged(normalized);
}

int ZzFlowLayout::verticalSpacing() const noexcept
{
    return d_ptr->verticalSpacing;
}

void ZzFlowLayout::setVerticalSpacing(int spacing)
{
    const int normalized = zzNormalizedFlowSpacing(spacing);
    if (d_ptr->verticalSpacing == normalized) {
        return;
    }
    d_ptr->verticalSpacing = normalized;
    invalidate();
    Q_EMIT verticalSpacingChanged(normalized);
}

void ZzFlowLayout::addItem(QLayoutItem *item)
{
    Q_ASSERT(item != nullptr);
    if (item == nullptr) {
        return;
    }
    d_ptr->items.append(item);
    invalidate();
}

int ZzFlowLayout::count() const
{
    return static_cast<int>(d_ptr->items.size());
}

QLayoutItem *ZzFlowLayout::itemAt(int index) const
{
    if (index < 0 || index >= d_ptr->items.size()) {
        return nullptr;
    }
    return d_ptr->items.at(index);
}

QLayoutItem *ZzFlowLayout::takeAt(int index)
{
    if (index < 0 || index >= d_ptr->items.size()) {
        return nullptr;
    }
    QLayoutItem *const result = d_ptr->items.takeAt(index);
    invalidate();
    return result;
}

Qt::Orientations ZzFlowLayout::expandingDirections() const
{
    return {};
}

bool ZzFlowLayout::hasHeightForWidth() const
{
    return true;
}

int ZzFlowLayout::heightForWidth(int width) const
{
    const int normalizedWidth = std::max(0, width);
    if (d_ptr->cachedHeightGeneration == d_ptr->generation
        && d_ptr->cachedWidth == normalizedWidth) {
        return d_ptr->cachedHeight;
    }
    d_ptr->cachedHeight = d_ptr->doLayout(
        QRect(0, 0, normalizedWidth, 0),
        false);
    d_ptr->cachedWidth = normalizedWidth;
    d_ptr->cachedHeightGeneration = d_ptr->generation;
    return d_ptr->cachedHeight;
}

QSize ZzFlowLayout::minimumSize() const
{
    d_ptr->ensureSizeHints();
    return d_ptr->cachedMinimumSize;
}

QSize ZzFlowLayout::sizeHint() const
{
    d_ptr->ensureSizeHints();
    return d_ptr->cachedSizeHint;
}

void ZzFlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    if (d_ptr->appliedGeometryGeneration == d_ptr->generation
        && d_ptr->appliedGeometry == rect) {
        return;
    }
    static_cast<void>(d_ptr->doLayout(rect, true));
    d_ptr->appliedGeometry = rect;
    d_ptr->appliedGeometryGeneration = d_ptr->generation;
}

void ZzFlowLayout::invalidate()
{
    d_ptr->invalidateCaches();
    QLayout::invalidate();
}

} // namespace ZzFluentUI
