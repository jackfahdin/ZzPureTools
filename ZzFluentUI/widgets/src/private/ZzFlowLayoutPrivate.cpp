#include "ZzFlowLayoutPrivate.h"

#include <algorithm>
#include <limits>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLayoutItem>
#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFlowLayout.h>

namespace ZzFluentUI {

namespace {

/** @brief 把 64 位布局计算安全收敛到 Qt 布局允许的范围。 */
int zzSaturatedLayoutValue(qint64 value) noexcept
{
    return static_cast<int>(std::clamp<qint64>(
        value,
        0,
        QLAYOUTSIZE_MAX));
}

/** @brief 把 64 位布局位置安全收敛到 QRect 可表达的坐标。 */
int zzSaturatedLayoutCoordinate(qint64 value) noexcept
{
    return static_cast<int>(std::clamp<qint64>(
        value,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()));
}

/** @brief 返回不小于零且上下界顺序稳定的首选轴向尺寸。 */
int zzBoundedItemExtent(int preferred, int minimum, int maximum) noexcept
{
    const int lower = std::max(0, minimum);
    const int upper = std::max(lower, maximum);
    return std::clamp(std::max(0, preferred), lower, upper);
}

/** @brief 返回 item 的有效控件类型集合。 */
QSizePolicy::ControlTypes zzControlTypes(QLayoutItem *item) noexcept
{
    const QSizePolicy::ControlTypes result = item->controlTypes();
    return result == QSizePolicy::ControlTypes{}
        ? QSizePolicy::ControlTypes(QSizePolicy::DefaultType)
        : result;
}

} // namespace

ZzFlowLayoutPrivate::ZzFlowLayoutPrivate(ZzFlowLayout *q) noexcept
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

ZzFlowLayoutPrivate::~ZzFlowLayoutPrivate()
{
    while (!items.isEmpty()) {
        delete items.takeLast();
    }
}

int ZzFlowLayoutPrivate::doLayout(
    const QRect &rect,
    bool applyGeometry) const
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    q_ptr->getContentsMargins(&left, &top, &right, &bottom);

    const int contentWidth = std::max(
        0,
        rect.width() - left - right);
    const QRect contentRect(
        rect.x() + left,
        rect.y() + top,
        contentWidth,
        std::max(0, rect.height() - top - bottom));
    const int horizontalMetric = styleSpacing(Qt::Horizontal);
    const int verticalMetric = styleSpacing(Qt::Vertical);
    QWidget *const parent = q_ptr->parentWidget();
    const Qt::LayoutDirection direction = parent != nullptr
        ? parent->layoutDirection()
        : QApplication::layoutDirection();

    lineScratch.clear();
    lineScratch.reserve(static_cast<std::size_t>(items.size()));
    int lineWidth = 0;
    int lineHeight = 0;
    int lineTop = contentRect.y();
    bool hasEffectiveItem = false;
    QSizePolicy::ControlTypes previousLineLastTypes;

    const auto flushLine = [&]() {
        if (lineScratch.empty()) {
            return;
        }
        if (applyGeometry) {
            for (const ZzFlowItemPlacement &placement : lineScratch) {
                const Qt::Alignment verticalAlignment =
                    placement.item->alignment()
                    & Qt::AlignVertical_Mask;
                int verticalOffset = 0;
                if (verticalAlignment == Qt::AlignBottom) {
                    verticalOffset = lineHeight - placement.size.height();
                } else if (verticalAlignment == Qt::AlignVCenter) {
                    verticalOffset =
                        (lineHeight - placement.size.height()) / 2;
                }
                const QRect logicalRect(
                    contentRect.x() + placement.logicalX,
                    lineTop + verticalOffset,
                    placement.size.width(),
                    placement.size.height());
                placement.item->setGeometry(QStyle::visualRect(
                    direction,
                    contentRect,
                    logicalRect));
            }
        }
        previousLineLastTypes = lineScratch.back().controlTypes;
        lineScratch.clear();
    };

    for (QLayoutItem *const item : items) {
        if (item == nullptr
            || (item->widget() != nullptr && item->isEmpty())) {
            continue;
        }
        const QSizePolicy::ControlTypes currentTypes =
            zzControlTypes(item);
        const QSize size = itemSize(item, contentWidth);
        int leadingSpacing = lineScratch.empty()
            ? 0
            : adjacentSpacing(
                  Qt::Horizontal,
                  horizontalMetric,
                  lineScratch.back().controlTypes,
                  currentTypes);
        const qint64 proposedRight = static_cast<qint64>(lineWidth)
            + leadingSpacing + size.width();
        if (!lineScratch.empty() && proposedRight > contentWidth) {
            flushLine();
            const int rowSpacing = adjacentSpacing(
                Qt::Vertical,
                verticalMetric,
                previousLineLastTypes,
                currentTypes);
            lineTop = zzSaturatedLayoutCoordinate(
                static_cast<qint64>(lineTop)
                + lineHeight + rowSpacing);
            lineWidth = 0;
            lineHeight = 0;
            leadingSpacing = 0;
        }

        lineWidth = zzSaturatedLayoutValue(
            static_cast<qint64>(lineWidth) + leadingSpacing);
        lineScratch.push_back({item, size, lineWidth, currentTypes});
        lineWidth = zzSaturatedLayoutValue(
            static_cast<qint64>(lineWidth) + size.width());
        lineHeight = std::max(lineHeight, size.height());
        hasEffectiveItem = true;
    }

    flushLine();
    lineScratch.clear();
    if (!hasEffectiveItem) {
        return zzSaturatedLayoutValue(
            static_cast<qint64>(top) + bottom);
    }
    return zzSaturatedLayoutValue(
        static_cast<qint64>(lineTop) - rect.y()
        + lineHeight + bottom);
}

void ZzFlowLayoutPrivate::invalidateCaches() noexcept
{
    if (generation == std::numeric_limits<std::uint64_t>::max()) {
        generation = 1;
    } else {
        ++generation;
    }
    cachedWidth = -1;
    cachedHeightGeneration = 0;
    cachedSizeGeneration = 0;
    appliedGeometryGeneration = 0;
}

void ZzFlowLayoutPrivate::ensureSizeHints() const
{
    if (cachedSizeGeneration == generation) {
        return;
    }

    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    q_ptr->getContentsMargins(&left, &top, &right, &bottom);
    const int horizontalMetric = styleSpacing(Qt::Horizontal);
    qint64 preferredWidth = 0;
    int preferredHeight = 0;
    int minimumWidth = 0;
    int minimumHeight = 0;
    bool hasPrevious = false;
    QSizePolicy::ControlTypes previousTypes;

    for (QLayoutItem *const item : items) {
        if (item == nullptr
            || (item->widget() != nullptr && item->isEmpty())) {
            continue;
        }
        const QSize minimum = item->minimumSize().expandedTo(QSize(0, 0));
        const QSizePolicy::ControlTypes currentTypes =
            zzControlTypes(item);
        const QSize preferred = itemSize(item, QLAYOUTSIZE_MAX);
        if (hasPrevious) {
            preferredWidth += adjacentSpacing(
                Qt::Horizontal,
                horizontalMetric,
                previousTypes,
                currentTypes);
        }
        preferredWidth = std::min<qint64>(
            QLAYOUTSIZE_MAX,
            preferredWidth + preferred.width());
        preferredHeight = std::max(
            preferredHeight,
            preferred.height());
        minimumWidth = std::max(minimumWidth, minimum.width());
        minimumHeight = std::max(minimumHeight, minimum.height());
        previousTypes = currentTypes;
        hasPrevious = true;
    }

    cachedMinimumSize = QSize(
        zzSaturatedLayoutValue(
            static_cast<qint64>(minimumWidth) + left + right),
        zzSaturatedLayoutValue(
            static_cast<qint64>(minimumHeight) + top + bottom));
    cachedSizeHint = QSize(
        zzSaturatedLayoutValue(preferredWidth + left + right),
        zzSaturatedLayoutValue(
            static_cast<qint64>(preferredHeight) + top + bottom));
    cachedSizeGeneration = generation;
}

int ZzFlowLayoutPrivate::styleSpacing(Qt::Orientation orientation) const
{
    const int configured = orientation == Qt::Horizontal
        ? horizontalSpacing
        : verticalSpacing;
    if (configured >= 0) {
        return configured;
    }
    QWidget *const parent = q_ptr->parentWidget();
    QStyle *const style = parent != nullptr
        ? parent->style()
        : QApplication::style();
    if (style == nullptr) {
        return 0;
    }
    const QStyle::PixelMetric metric = orientation == Qt::Horizontal
        ? QStyle::PM_LayoutHorizontalSpacing
        : QStyle::PM_LayoutVerticalSpacing;
    return style->pixelMetric(metric, nullptr, parent);
}

int ZzFlowLayoutPrivate::adjacentSpacing(
    Qt::Orientation orientation,
    int styleMetric,
    QSizePolicy::ControlTypes first,
    QSizePolicy::ControlTypes second) const
{
    if (styleMetric >= 0) {
        return styleMetric;
    }
    QWidget *const parent = q_ptr->parentWidget();
    QStyle *const style = parent != nullptr
        ? parent->style()
        : QApplication::style();
    if (style == nullptr) {
        return 0;
    }
    return std::max(
        0,
        style->combinedLayoutSpacing(
            first,
            second,
            orientation,
            nullptr,
            parent));
}

QSize ZzFlowLayoutPrivate::itemSize(
    QLayoutItem *item,
    int availableWidth) const
{
    Q_ASSERT(item != nullptr);
    const QSize preferred = item->sizeHint();
    const QSize minimum = item->minimumSize();
    const QSize maximum = item->maximumSize();
    int width = zzBoundedItemExtent(
        preferred.width(),
        minimum.width(),
        maximum.width());
    const int normalizedAvailable = std::max(0, availableWidth);
    if (width > normalizedAvailable
        && std::max(0, minimum.width()) <= normalizedAvailable) {
        width = normalizedAvailable;
    }

    const int preferredHeight = item->hasHeightForWidth()
        ? item->heightForWidth(width)
        : preferred.height();
    const int height = zzBoundedItemExtent(
        preferredHeight,
        minimum.height(),
        maximum.height());
    return {width, height};
}

} // namespace ZzFluentUI
