#include "ZzRatingControlPrivate.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QtMath>
#include <QtGui/QAccessibleValueChangeEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QAccessibleWidget>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzIconFont.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzRatingControl.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

constexpr int zzRatingSpacing = 4;
constexpr qreal zzContentInset = 2.0;

/** @brief 把字体字形渲染为非 Fluent style 使用的单色回退 pixmap。 */
[[nodiscard]] QPixmap zzRenderFallbackStar(
    int logicalExtent,
    qreal devicePixelRatio,
    const QColor &color)
{
    const qreal effectiveDpr = devicePixelRatio > 0.0
        ? devicePixelRatio
        : 1.0;
    const int physicalExtent = std::max(
        1,
        qCeil(static_cast<qreal>(logicalExtent) * effectiveDpr));
    QPixmap pixmap(physicalExtent, physicalExtent);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(effectiveDpr);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setPen(color);
    painter.setFont(ZzIconFont::font(logicalExtent));
    const char32_t glyph = static_cast<char32_t>(ZzFontIcon::Star);
    painter.drawText(
        QRect(0, 0, logicalExtent, logicalExtent),
        Qt::AlignCenter,
        QString::fromUcs4(&glyph, 1));
    return pixmap;
}

#if QT_CONFIG(accessibility)

/** @brief 为评分控件提供浮点值范围和 Slider 语义。 */
class ZzAccessibleRatingControl final
    : public QAccessibleWidget
    , public QAccessibleValueInterface
{
public:
    /** @brief 绑定仍由 QWidget 生命周期管理的评分控件。 */
    explicit ZzAccessibleRatingControl(ZzRatingControl *control)
        : QAccessibleWidget(control, QAccessible::Slider)
        , control_(control)
    {
    }

    /** @brief 暴露 QAccessibleValueInterface，其他接口委托 Qt。 */
    void *interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ValueInterface) {
            return static_cast<QAccessibleValueInterface *>(this);
        }
        return QAccessibleWidget::interface_cast(type);
    }

    /** @brief 返回 qreal 当前评分。 */
    [[nodiscard]] QVariant currentValue() const override
    {
        return control_ != nullptr
            ? QVariant::fromValue(control_->rating())
            : QVariant{};
    }

    /** @brief 在可编辑且启用时通过公开 setter 写入有限浮点值。 */
    void setCurrentValue(const QVariant &value) override
    {
        if (control_ == nullptr
            || !control_->isEnabled()
            || control_->isReadOnly()) {
            return;
        }
        bool valid = false;
        const qreal candidate = value.toDouble(&valid);
        if (valid && std::isfinite(candidate)) {
            control_->setRating(candidate);
        }
    }

    /** @brief 返回 qreal 最大评分。 */
    [[nodiscard]] QVariant maximumValue() const override
    {
        return control_ != nullptr
            ? QVariant::fromValue(
                static_cast<qreal>(control_->maximumRating()))
            : QVariant{};
    }

    /** @brief 返回固定下界 0.0。 */
    [[nodiscard]] QVariant minimumValue() const override
    {
        return QVariant::fromValue(0.0);
    }

    /** @brief 返回当前精度对应的 qreal 最小步长。 */
    [[nodiscard]] QVariant minimumStepSize() const override
    {
        if (control_ == nullptr) {
            return QVariant{};
        }
        return QVariant::fromValue(
            control_->precision() == ZzRatingPrecision::Half
                ? 0.5
                : 1.0);
    }

    /** @brief 补充只读状态，保留 Qt 计算的焦点和禁用状态。 */
    [[nodiscard]] QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        if (control_ != nullptr && control_->isReadOnly()) {
            result.readOnly = true;
        }
        return result;
    }

    /** @brief 返回名称、当前值和本地化范围说明。 */
    [[nodiscard]] QString text(QAccessible::Text type) const override
    {
        if (control_ == nullptr) {
            return {};
        }
        const QLocale locale = control_->locale();
        QString current = locale.toString(control_->rating(), 'f',
            control_->precision() == ZzRatingPrecision::Half ? 1 : 0);
        const QString maximum = locale.toString(control_->maximumRating());
        if (type == QAccessible::Value) {
            return current;
        }
        if (type == QAccessible::Description
            && control_->accessibleDescription().isEmpty()) {
            return QCoreApplication::translate(
                       "ZzRatingControl",
                       "当前值 %1，最大值 %2")
                .arg(current, maximum);
        }
        return QAccessibleWidget::text(type);
    }

private:
    ZzRatingControl *const control_;
};

/** @brief 将评分接口所有权转移给 Qt 无障碍缓存。 */
[[nodiscard]] QAccessible::Id zzRegisterAccessibleRatingControl(
    ZzRatingControl *control)
{
    // Qt 缓存接管指针，并由 ZzRatingControlPrivate 析构时按 Id 删除。
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,clang-analyzer-cplusplus.NewDeleteLeaks)
    return QAccessible::registerAccessibleInterface(
        new ZzAccessibleRatingControl(control));
}

#endif

} // namespace

ZzRatingControlPrivate::ZzRatingControlPrivate(ZzRatingControl *q)
    : q_ptr(q)
    , theme(q)
{
    Q_ASSERT(q_ptr != nullptr);
#if QT_CONFIG(accessibility)
    accessibleInterfaceId = zzRegisterAccessibleRatingControl(q_ptr);
#endif
}

ZzRatingControlPrivate::~ZzRatingControlPrivate()
{
#if QT_CONFIG(accessibility)
    if (accessibleInterfaceId != 0) {
        QAccessible::deleteAccessibleInterface(accessibleInterfaceId);
    }
#endif
}

qreal ZzRatingControlPrivate::stepSize() const noexcept
{
    return precision == ZzRatingPrecision::Half ? 0.5 : 1.0;
}

qreal ZzRatingControlPrivate::normalized(qreal value) const noexcept
{
    if (!std::isfinite(value)) {
        return rating;
    }
    const qreal bounded = std::clamp(
        value,
        0.0,
        static_cast<qreal>(maximumRating));
    const qreal step = stepSize();
    return std::clamp(
        std::round(bounded / step) * step,
        0.0,
        static_cast<qreal>(maximumRating));
}

bool ZzRatingControlPrivate::setRating(qreal value)
{
    const qreal next = normalized(value);
    if (qFuzzyCompare(rating + 1.0, next + 1.0)) {
        return false;
    }
    rating = next;
    q_ptr->update();
    Q_EMIT q_ptr->ratingChanged(next);
    notifyAccessibleValueChanged(next);
    return true;
}

QRect ZzRatingControlPrivate::ratingRect() const
{
    const int extent = qCeil(
        theme.snapshot()->metric(ZzMetricToken::RatingGlyphExtent));
    const int width = (maximumRating * extent)
        + ((maximumRating - 1) * zzRatingSpacing);
    const QRect available = q_ptr->rect().adjusted(
        qCeil(zzContentInset),
        qCeil(zzContentInset),
        -qCeil(zzContentInset),
        -qCeil(zzContentInset));
    return QRect(
        available.center().x() - (width / 2),
        available.center().y() - (extent / 2),
        width,
        extent);
}

QRect ZzRatingControlPrivate::glyphRect(int index) const
{
    if (index < 0 || index >= maximumRating) {
        return {};
    }
    const QRect bounds = ratingRect();
    const int extent = bounds.height();
    const QRect logical(
        bounds.left() + (index * (extent + zzRatingSpacing)),
        bounds.top(),
        extent,
        extent);
    return QStyle::visualRect(
        q_ptr->layoutDirection(), bounds, logical);
}

qreal ZzRatingControlPrivate::ratingAt(
    const QPointF &position) const noexcept
{
    const QRect bounds = ratingRect();
    if (bounds.isEmpty()) {
        return rating;
    }
    qreal logicalX = position.x() - bounds.left();
    if (q_ptr->layoutDirection() == Qt::RightToLeft) {
        logicalX = static_cast<qreal>(bounds.right()) - position.x();
    }
    logicalX = std::clamp(
        logicalX,
        0.0,
        static_cast<qreal>(bounds.width() - 1));
    const int extent = bounds.height();
    const int pitch = extent + zzRatingSpacing;
    const int index = std::clamp(
        static_cast<int>(logicalX) / pitch,
        0,
        maximumRating - 1);
    if (precision == ZzRatingPrecision::Whole) {
        return static_cast<qreal>(index + 1);
    }
    const qreal within = logicalX - static_cast<qreal>(index * pitch);
    return static_cast<qreal>(index)
        + (within < (static_cast<qreal>(extent) / 2.0) ? 0.5 : 1.0);
}

void ZzRatingControlPrivate::updatePreview(const QPointF &position)
{
    const qreal next = ratingAt(position);
    if (previewActive
        && qFuzzyCompare(previewRating + 1.0, next + 1.0)) {
        return;
    }
    previewRating = next;
    previewActive = true;
    q_ptr->update();
}

void ZzRatingControlPrivate::clearPreview()
{
    if (!previewActive) {
        return;
    }
    previewActive = false;
    previewRating = rating;
    q_ptr->update();
}

void ZzRatingControlPrivate::paint(QPainter *painter)
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    ensurePixmaps();
    if (emptyStar.isNull() || filledStar.isNull()) {
        return;
    }
    const qreal visualRating = previewActive ? previewRating : rating;
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (int index = 0; index < maximumRating; ++index) {
        const QRect cell = glyphRect(index);
        painter->drawPixmap(cell.topLeft(), emptyStar);
        const qreal fill = std::clamp(
            visualRating - static_cast<qreal>(index),
            0.0,
            1.0);
        if (qFuzzyIsNull(fill)) {
            continue;
        }
        painter->save();
        QRectF clip(cell);
        clip.setWidth(clip.width() * fill);
        if (q_ptr->layoutDirection() == Qt::RightToLeft) {
            clip.moveRight(cell.right() + 1.0);
        }
        painter->setClipRect(clip);
        painter->drawPixmap(cell.topLeft(), filledStar);
        painter->restore();
    }
    painter->restore();

    bool showFocus = q_ptr->hasFocus();
    if (const auto *fluentStyle =
            qobject_cast<const ZzFluentStyle *>(q_ptr->style())) {
        showFocus = showFocus
            && fluentStyle->isFocusVisualVisible(q_ptr);
    }
    if (showFocus) {
        const auto snapshot = theme.snapshot();
        ZzFluentPainter::drawFocusRing(
            painter,
            QRectF(q_ptr->rect()).adjusted(1.0, 1.0, -1.0, -1.0),
            *snapshot,
            q_ptr->devicePixelRatioF());
    }
}

void ZzRatingControlPrivate::refreshTheme()
{
    theme.refreshFallback();
    emptyStar = {};
    filledStar = {};
    cachedThemeRevision = std::numeric_limits<quint64>::max();
    cachedDevicePixelRatio = 0.0;
    cachedGlyphExtent = 0;
    cachedColorGroup = QPalette::NColorGroups;
    q_ptr->updateGeometry();
    q_ptr->update();
}

void ZzRatingControlPrivate::notifyAccessibleValueChanged(
    qreal value) const
{
#if QT_CONFIG(accessibility)
    QAccessibleValueChangeEvent event(q_ptr, QVariant::fromValue(value));
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(value)
#endif
}

void ZzRatingControlPrivate::ensurePixmaps()
{
    const auto snapshot = theme.snapshot();
    const int extent = std::max(
        1,
        qCeil(snapshot->metric(ZzMetricToken::RatingGlyphExtent)));
    const qreal dpr = q_ptr->devicePixelRatioF();
    const QPalette::ColorGroup group = q_ptr->isEnabled()
        ? QPalette::Normal
        : QPalette::Disabled;
    if (!emptyStar.isNull()
        && !filledStar.isNull()
        && cachedThemeRevision == snapshot->revision()
        && qFuzzyCompare(cachedDevicePixelRatio, dpr)
        && cachedGlyphExtent == extent
        && cachedColorGroup == group) {
        return;
    }

    const QColor emptyColor = q_ptr->isEnabled()
        ? snapshot->color(ZzColorToken::TextSecondary)
        : q_ptr->palette().color(QPalette::Disabled, QPalette::Text);
    const QColor filledColor = q_ptr->isEnabled()
        ? snapshot->color(ZzColorToken::Accent)
        : q_ptr->palette().color(QPalette::Disabled, QPalette::Highlight);
    if (auto *fluentStyle = qobject_cast<ZzFluentStyle *>(q_ptr->style())) {
        const ZzIconDescriptor descriptor =
            ZzIconDescriptor::fromFontIcon(ZzFontIcon::Star);
        const QSize logicalSize(extent, extent);
        emptyStar = fluentStyle->iconPixmap(
            descriptor,
            logicalSize,
            dpr,
            emptyColor,
            q_ptr->layoutDirection());
        filledStar = fluentStyle->iconPixmap(
            descriptor,
            logicalSize,
            dpr,
            filledColor,
            q_ptr->layoutDirection());
    } else {
        emptyStar = zzRenderFallbackStar(extent, dpr, emptyColor);
        filledStar = zzRenderFallbackStar(extent, dpr, filledColor);
    }
    cachedThemeRevision = snapshot->revision();
    cachedDevicePixelRatio = dpr;
    cachedGlyphExtent = extent;
    cachedColorGroup = group;
}

} // namespace ZzFluentUI
