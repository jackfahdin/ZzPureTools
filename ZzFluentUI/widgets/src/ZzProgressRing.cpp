#include <ZzFluentUI/ZzProgressRing.h>

#include <algorithm>
#include <cmath>

#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QHideEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
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

ZzProgressRing::~ZzProgressRing()
{
    if (d_ptr->centerWidget) d_ptr->centerWidget->removeEventFilter(this);
}

int ZzProgressRing::ringWidth() const noexcept
{
    return qRound(d_ptr->thickness);
}

void ZzProgressRing::setRingWidth(int width)
{
    setThickness(width);
}

qreal ZzProgressRing::thickness() const noexcept
{
    return d_ptr->thickness;
}

void ZzProgressRing::setThickness(qreal thickness)
{
    if (!std::isfinite(thickness)) {
        return;
    }
    const qreal bounded = std::clamp(thickness, 1.0, qreal(zzMaximumRingWidth));
    if (d_ptr->thickness == bounded) {
        return;
    }
    d_ptr->thickness = bounded;
    QPointer<ZzProgressRing> guard(this);
    updateGeometry();
    update();
    updateCenterWidgetGeometry();
    if (!guard || d_ptr->thickness != bounded) {
        return;
    }
    Q_EMIT thicknessChanged(bounded);
    if (guard && d_ptr->notifiedRingWidth != ringWidth()) {
        // Recursive fractional setters may share a rounded width. Compare with
        // the last published integer, not the intermediate setter's input state.
        const int width = ringWidth();
        d_ptr->notifiedRingWidth = width;
        Q_EMIT ringWidthChanged(width);
    }
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
        qCeil(2 * d_ptr->thickness) + (4 * zzRingOuterMargin));
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
    QPointer<ZzProgressRing> guard(this);
    updateCenterWidgetGeometry();
    if (!guard) {
        return;
    }
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
        d_ptr->thickness,
        maximumPenWidth);
    const qreal inset = (penWidth / 2.0) + zzRingOuterMargin;
    const QRectF ringRect = square.adjusted(inset, inset, -inset, -inset);
    if (ringRect.width() <= 0.0 || ringRect.height() <= 0.0) {
        return;
    }

    const QPalette::ColorGroup group = zzProgressColorGroup(option);
    QColor trackColor = d_ptr->trackColor.isValid() ? d_ptr->trackColor
        : option.palette.color(group, QPalette::Mid);
    QColor progressColor = d_ptr->ringColor.isValid() ? d_ptr->ringColor
        : option.palette.color(group, QPalette::Accent);
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

    if (option.textVisible && !d_ptr->centerWidget) {
        d_ptr->drawText(painter, centerContentRect());
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
    updateCenterWidgetGeometry();
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

#define ZZ_RING_PROPERTY(Type, Getter, Setter, Member) \
    Type ZzProgressRing::Getter() const { return d_ptr->Member; } \
    void ZzProgressRing::Setter(Type value) \
    { \
        if (d_ptr->Member == value) return; \
        d_ptr->Member = value; \
        update(); \
        Q_EMIT Getter##Changed(value); \
    }

ZZ_RING_PROPERTY(QString, title, setTitle, title)
ZZ_RING_PROPERTY(QFont, titleFont, setTitleFont, customTitleFont)
ZZ_RING_PROPERTY(QFont, valueFont, setValueFont, customValueFont)
ZZ_RING_PROPERTY(QColor, titleColor, setTitleColor, titleColor)
ZZ_RING_PROPERTY(QColor, valueColor, setValueColor, valueColor)
ZZ_RING_PROPERTY(QColor, ringColor, setRingColor, ringColor)
ZZ_RING_PROPERTY(QColor, trackColor, setTrackColor, trackColor)
#undef ZZ_RING_PROPERTY

int ZzProgressRing::textSpacing() const noexcept { return d_ptr->textSpacing; }

void ZzProgressRing::setTextSpacing(int spacing)
{
    spacing = std::clamp(spacing, 0, 100);
    if (d_ptr->textSpacing == spacing) return;
    d_ptr->textSpacing = spacing;
    update();
    Q_EMIT textSpacingChanged(spacing);
}

QRectF ZzProgressRing::centerContentRect() const
{
    const QRectF content(contentsRect());
    const qreal side = std::min(content.width(), content.height());
    const qreal inset = std::min(side / 2.0, d_ptr->thickness + 6.0);
    QRectF result(0, 0, std::max(0.0, side - 2 * inset), std::max(0.0, side - 2 * inset));
    result.moveCenter(content.center());
    return result;
}

QWidget *ZzProgressRing::centerWidget() const noexcept { return d_ptr->centerWidget.data(); }

void ZzProgressRing::releaseCenterWidget()
{
    QObject::disconnect(d_ptr->centerDestroyedConnection);
    if (d_ptr->centerWidget) d_ptr->centerWidget->removeEventFilter(this);
    d_ptr->centerWidget = nullptr;
    const quint64 revision = ++d_ptr->centerRevision;
    update();
    // setParent and QObject::destroyed both run before Qt has finished updating
    // the parent/child relationship. Wait before slots can reparent the child
    // or delete the ring, and suppress a notification superseded by a new center.
    QTimer::singleShot(0, this, [this, revision] {
        if (d_ptr->centerRevision == revision) Q_EMIT centerWidgetChanged(nullptr);
    });
}

void ZzProgressRing::setCenterWidget(QWidget *widget)
{
    if (widget == this || widget == centerWidget()
        || (widget && widget->isAncestorOf(this))
        || (widget && centerWidget() && centerWidget()->isAncestorOf(widget))) return;

    QPointer<ZzProgressRing> guard(this);
    QPointer<QWidget> candidate(widget);
    QPointer<QWidget> previous = d_ptr->centerWidget;
    QObject::disconnect(d_ptr->centerDestroyedConnection);
    if (previous) previous->removeEventFilter(this);
    d_ptr->centerWidget = nullptr;
    const quint64 revision = ++d_ptr->centerRevision;
    delete previous.data();
    if (!guard || d_ptr->centerRevision != revision) return;

    d_ptr->centerWidget = candidate;
    if (candidate) {
        candidate->installEventFilter(this);
        d_ptr->centerDestroyedConnection = connect(candidate, &QObject::destroyed, this,
            [this, revision] {
                if (d_ptr->centerRevision == revision) releaseCenterWidget();
            });
        candidate->setParent(this);
        if (!guard || d_ptr->centerRevision != revision) return;
        updateCenterWidgetGeometry();
        if (!guard || d_ptr->centerRevision != revision) return;
    }
    update();
    Q_EMIT centerWidgetChanged(d_ptr->centerWidget.data());
}

QWidget *ZzProgressRing::takeCenterWidget()
{
    QPointer<QWidget> widget = d_ptr->centerWidget;
    if (!widget) return nullptr;
    QPointer<ZzProgressRing> guard(this);
    QObject::disconnect(d_ptr->centerDestroyedConnection);
    widget->removeEventFilter(this);
    d_ptr->centerWidget = nullptr;
    const quint64 revision = ++d_ptr->centerRevision;
    widget->hide();
    if (guard && widget && d_ptr->centerWidget != widget
        && widget->parentWidget() == this) widget->setParent(nullptr);
    if (guard && d_ptr->centerRevision == revision) {
        update();
        Q_EMIT centerWidgetChanged(nullptr);
    }
    // Reentrant slots may delete or re-adopt the released widget.
    return widget && !widget->parentWidget() ? widget.data() : nullptr;
}

void ZzProgressRing::updateCenterWidgetGeometry()
{
    QPointer<QWidget> widget = d_ptr->centerWidget;
    if (!widget) return;
    QPointer<ZzProgressRing> guard(this);
    const quint64 revision = d_ptr->centerRevision;
    const QRectF content = centerContentRect();
    const int left = qCeil(content.left());
    const int top = qCeil(content.top());
    widget->setGeometry(left, top,
        std::max(0, qFloor(content.right()) - left),
        std::max(0, qFloor(content.bottom()) - top));
    if (!guard || !widget || d_ptr->centerRevision != revision) return;
    widget->setVisible(isTextVisible() && !content.isEmpty());
    if (!guard || !widget || d_ptr->centerRevision != revision) return;
    widget->raise();
}

void ZzProgressRing::setTextVisible(bool visible)
{
    QProgressBar::setTextVisible(visible);
    updateCenterWidgetGeometry();
}

void ZzProgressRing::resizeEvent(QResizeEvent *event)
{
    QProgressBar::resizeEvent(event);
    updateCenterWidgetGeometry();
}

bool ZzProgressRing::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ParentChange && watched == d_ptr->centerWidget.data()
        && d_ptr->centerWidget->parentWidget() != this) {
        releaseCenterWidget();
    }
    return QProgressBar::eventFilter(watched, event);
}

} // namespace ZzFluentUI
