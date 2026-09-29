#include <ZzFluentUI/ZzProgressRing.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QtCore/QEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QPainter>
#include <QtGui/QShowEvent>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStyleOptionProgressBar>

#include "private/ZzProgressRingPrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int zzDefaultExtent = 120;
constexpr int zzMinimumExtent = 48;
constexpr int zzMaximumRingWidth = 64;
constexpr int zzIndeterminateSpanDegrees = 90;
constexpr int zzFullCircleDegrees = 360;
constexpr int zzQtAngleScale = 16;
/** @brief 圆环外围留白与文字到内圆的安全间距，单位逻辑像素。 */
constexpr int zzRingOuterMargin = 2;
constexpr int zzRingTextGap = 3;

/** @brief 返回适合当前控件状态的 palette 颜色组。 */
QPalette::ColorGroup zzProgressColorGroup(
    const QStyleOptionProgressBar &option)
{
    if ((option.state & QStyle::State_Enabled) == 0) {
        return QPalette::Disabled;
    }
    return (option.state & QStyle::State_Active) != 0
        ? QPalette::Active
        : QPalette::Inactive;
}

/** @brief 按固定通道比例把前景收敛到指定背景色。 */
QColor zzBlendProgressColor(
    const QColor &foreground,
    const QColor &background,
    float foregroundRatio) noexcept
{
    const float ratio = std::clamp(foregroundRatio, 0.0F, 1.0F);
    const float backgroundRatio = 1.0F - ratio;
    return QColor::fromRgbF(
        (foreground.redF() * ratio)
            + (background.redF() * backgroundRatio),
        (foreground.greenF() * ratio)
            + (background.greenF() * backgroundRatio),
        (foreground.blueF() * ratio)
            + (background.blueF() * backgroundRatio),
        (foreground.alphaF() * ratio)
            + (background.alphaF() * backgroundRatio));
}

/** @brief 使用 64 位差值计算进度比例，退化范围不执行除法。 */
qreal zzProgressRatio(const ZzProgressRing *ring) noexcept
{
    const qint64 minimum = ring->minimum();
    const qint64 maximum = ring->maximum();
    const qint64 value = ring->value();
    if (maximum <= minimum) {
        return value >= maximum ? 1.0 : 0.0;
    }
    const qreal numerator = static_cast<qreal>(value - minimum);
    const qreal denominator = static_cast<qreal>(maximum - minimum);
    return std::clamp(numerator / denominator, 0.0, 1.0);
}

} // namespace

ZzProgressRing::ZzProgressRing(QWidget *parent)
    : QProgressBar(parent)
    , d_ptr(std::make_unique<ZzProgressRingPrivate>(this))
{
    setValue(0);
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

ZzProgressRing::~ZzProgressRing() = default;

int ZzProgressRing::ringWidth() const noexcept
{
    return d_ptr->ringWidth;
}

void ZzProgressRing::setRingWidth(int width)
{
    const int bounded = std::clamp(width, 1, zzMaximumRingWidth);
    if (d_ptr->ringWidth == bounded) {
        return;
    }
    d_ptr->ringWidth = bounded;
    updateGeometry();
    update();
    Q_EMIT ringWidthChanged(bounded);
}

int ZzProgressRing::indeterminateDuration() const noexcept
{
    return d_ptr->indeterminateDuration;
}

void ZzProgressRing::setIndeterminateDuration(int milliseconds)
{
    constexpr int minimumDuration = 200;
    constexpr int maximumDuration = 60000;
    const int bounded = std::clamp(milliseconds, minimumDuration, maximumDuration);
    if (d_ptr->indeterminateDuration == bounded) {
        return;
    }
    d_ptr->setIndeterminateDuration(bounded);
    Q_EMIT indeterminateDurationChanged(bounded);
}

QSize ZzProgressRing::sizeHint() const
{
    const QSize minimum = minimumSizeHint();
    return QSize(
        std::max(zzDefaultExtent, minimum.width()),
        std::max(zzDefaultExtent, minimum.height()));
}

QSize ZzProgressRing::minimumSizeHint() const
{
    const int extent = std::max(
        zzMinimumExtent,
        (2 * d_ptr->ringWidth) + (4 * zzRingOuterMargin));
    return QSize(extent, extent);
}

void ZzProgressRing::setRange(int minimum, int maximum)
{
    QProgressBar::setRange(minimum, maximum);
    d_ptr->syncAnimation();
}

void ZzProgressRing::setMinimum(int minimum)
{
    QProgressBar::setMinimum(minimum);
    d_ptr->syncAnimation();
}

void ZzProgressRing::setMaximum(int maximum)
{
    QProgressBar::setMaximum(maximum);
    d_ptr->syncAnimation();
}

void ZzProgressRing::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    d_ptr->syncAnimation();
    QStyleOptionProgressBar option;
    initStyleOption(&option);

    const QRectF content(contentsRect());
    const qreal extent = std::min(content.width(), content.height());
    if (extent <= 2 * zzRingOuterMargin) {
        return;
    }
    const QRectF square(
        content.center().x() - (extent / 2.0),
        content.center().y() - (extent / 2.0),
        extent,
        extent);
    const qreal maximumPenWidth = std::max(1.0, (extent / 2.0) - zzRingOuterMargin);
    const qreal penWidth = std::min(
        static_cast<qreal>(d_ptr->ringWidth),
        maximumPenWidth);
    const qreal inset = (penWidth / 2.0) + zzRingOuterMargin;
    const QRectF ringRect = square.adjusted(inset, inset, -inset, -inset);
    if (ringRect.width() <= 0.0 || ringRect.height() <= 0.0) {
        return;
    }

    const QPalette::ColorGroup group = zzProgressColorGroup(option);
    QColor trackColor = option.palette.color(group, QPalette::Mid);
    QColor progressColor = option.palette.color(
        group,
        QPalette::Highlight);
    if (group == QPalette::Disabled) {
        const QColor background = option.palette.color(
            QPalette::Disabled,
            QPalette::Window);
        trackColor = zzBlendProgressColor(trackColor, background, 0.55F);
        progressColor = zzBlendProgressColor(
            progressColor,
            trackColor,
            0.60F);
    }
    QPen pen(trackColor, penWidth, Qt::SolidLine, Qt::RoundCap);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(pen);
    painter.drawEllipse(ringRect);

    const int direction = invertedAppearance() ? 1 : -1;
    pen.setColor(progressColor);
    painter.setPen(pen);
    if (d_ptr->isIndeterminate()) {
        const int phaseAngle = static_cast<int>(std::lround(
            d_ptr->phase
            * zzFullCircleDegrees
            * zzQtAngleScale));
        const int startAngle = (90 * zzQtAngleScale)
            + (direction * phaseAngle);
        painter.drawArc(
            ringRect,
            startAngle,
            direction * zzIndeterminateSpanDegrees * zzQtAngleScale);
    } else {
        const qreal ratio = zzProgressRatio(this);
        if (qFuzzyCompare(ratio, 1.0)) {
            painter.drawEllipse(ringRect);
        } else if (!qFuzzyIsNull(ratio)) {
            const int spanAngle = static_cast<int>(std::lround(
                ratio
                * zzFullCircleDegrees
                * zzQtAngleScale));
            painter.drawArc(
                ringRect,
                90 * zzQtAngleScale,
                direction * spanAngle);
        }
    }

    if (!d_ptr->isIndeterminate()
        && option.textVisible
        && !option.text.isEmpty()) {
        // 内接正方形保证文字的四角也位于内圆中，不会压住圆环。
        const qreal innerRadius = std::max(0.0,
            (ringRect.width() - penWidth) / 2.0 - zzRingTextGap);
        const qreal side = innerRadius * std::numbers::sqrt2_v<qreal>;
        QRectF textBounds(0.0, 0.0, side, side);
        textBounds.moveCenter(ringRect.center());
        const QRect textRect = textBounds.toRect();
        const QFont valueFont = d_ptr->valueFont();
        const QFontMetrics metrics(valueFont);
        if (!textRect.isEmpty() && metrics.height() <= textRect.height()) {
            const QString visibleText = metrics.elidedText(
                option.text,
                Qt::ElideRight,
                textRect.width());
            painter.setFont(valueFont);
            painter.setClipRect(textRect, Qt::IntersectClip);
            style()->drawItemText(
                &painter,
                textRect,
                Qt::AlignCenter | Qt::TextSingleLine,
                option.palette,
                isEnabled(),
                visibleText,
                QPalette::Text);
        }
    }
}

void ZzProgressRing::changeEvent(QEvent *event)
{
    QProgressBar::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::EnabledChange:
    case QEvent::StyleChange:
        d_ptr->syncAnimation();
        update();
        break;
    case QEvent::FontChange:
        updateGeometry();
        update();
        break;
    case QEvent::PaletteChange:
        update();
        break;
    default:
        break;
    }
}

void ZzProgressRing::showEvent(QShowEvent *event)
{
    QProgressBar::showEvent(event);
    d_ptr->syncAnimation();
}

void ZzProgressRing::hideEvent(QHideEvent *event)
{
    d_ptr->stopAnimation();
    QProgressBar::hideEvent(event);
}

} // namespace ZzFluentUI
