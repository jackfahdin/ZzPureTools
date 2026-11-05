#include <ZzFluentUI/ZzToggleSwitch.h>

#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QHideEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleOptionButton>

#include "private/ZzToggleSwitchPrivate.h"
#include "private/ZzControlAppearancePrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int zzTrackWidth = 40;
constexpr int zzTrackHeight = 20;
constexpr int zzKnobExtent = 12;
constexpr int zzTrackInset = 3;
constexpr int zzTextSpacing = 8;
constexpr double zzMinimumKnobContrast = 3.0;

QRect zzTrackRect(const QWidget *widget)
{
    const QRect content = widget->rect().marginsRemoved(widget->contentsMargins());
    return QRect(widget->isRightToLeft() ? content.right() - zzTrackWidth + 1 : content.left(),
        content.center().y() - zzTrackHeight / 2, zzTrackWidth, zzTrackHeight);
}

QRectF zzKnobRect(const QRect &track, bool rtl, qreal progress, bool hovered, bool pressed)
{
    const qreal height = hovered && !pressed ? 14.0 : zzKnobExtent;
    const qreal width = pressed ? 18.0 : height;
    const qreal travel = zzTrackWidth - (2 * zzTrackInset) - width;
    return QRectF(track.left() + zzTrackInset + travel * (rtl ? 1.0 - progress : progress),
        track.top() + (zzTrackHeight - height) / 2.0, width, height);
}

qreal zzDragProgress(const QWidget *widget, const QPointF &pressPosition,
    qreal pressProgress, const QPointF &position)
{
    const qreal travel = zzTrackWidth - 2 * zzTrackInset
        - zzKnobRect(zzTrackRect(widget), widget->isRightToLeft(), 0.0, false, true).width();
    const qreal delta = position.x() - pressPosition.x();
    return std::clamp(pressProgress + (widget->isRightToLeft() ? -delta : delta) / travel,
        0.0, 1.0);
}

/**
 * @brief 按表面明暗选择无描边的开启态圆点。
 * @param palette 控件调色板。
 * @return 浅色主题保持白色，深色主题优先使用表面深灰色。
 */
QColor zzCheckedKnobColor(const QPalette &palette)
{
    const QColor surface = palette.color(QPalette::Window);
    const bool dark = ZzControlAppearancePrivate::contrastingText(surface) == QColorConstants::White;
    if (!dark) {
        return QColorConstants::White;
    }
    QColor accent = ZzControlAppearancePrivate::accent(palette);
    if (accent.alpha() != 255) {
        // 对半透明强调色按轨道落在窗口表面后的实际颜色计算对比。
        const int alpha = accent.alpha();
        const auto blend = [alpha](int channel, int background) {
            return (channel * alpha + background * (255 - alpha) + 127) / 255;
        };
        accent = QColor::fromRgb(blend(accent.red(), surface.red()),
            blend(accent.green(), surface.green()), blend(accent.blue(), surface.blue()));
    }
    if (ZzControlAppearancePrivate::contrastRatio(surface, accent) >= zzMinimumKnobContrast) {
        return surface;
    }
    // 深灰不足时先尝试黑色，避免默认蓝色轨道在深色主题仍使用白色圆点。
    const QColor extreme = QColorConstants::Black;
    if (ZzControlAppearancePrivate::contrastRatio(extreme, accent) >= zzMinimumKnobContrast) {
        return extreme;
    }
    return ZzControlAppearancePrivate::contrastingText(accent);
}

} // namespace

ZzToggleSwitch::ZzToggleSwitch(QWidget *parent)
    : QCheckBox(parent)
{
    // Creating the animation sends ChildAdded synchronously through event().
    // Keep the pointer initialized to null until construction has completed.
    d_ptr = std::make_unique<ZzToggleSwitchPrivate>(this);
    setAttribute(Qt::WA_Hover);
    d_ptr->progress = isChecked() ? 1.0 : 0.0;
    connect(
        this,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            d_ptr->moveTo(checked);
        });
}

ZzToggleSwitch::ZzToggleSwitch(
    const QString &text,
    QWidget *parent)
    : ZzToggleSwitch(parent)
{
    setText(text);
}

ZzToggleSwitch::~ZzToggleSwitch() = default;

QSize ZzToggleSwitch::sizeHint() const
{
    const QMargins margins = contentsMargins();
    const int textWidth = text().isEmpty()
        ? 0
        : fontMetrics().horizontalAdvance(text()) + zzTextSpacing;
    const int contentHeight = std::max(
        28,
        text().isEmpty() ? 0 : fontMetrics().height());
    return QSize(
        zzTrackWidth + textWidth + margins.left() + margins.right(),
        contentHeight + margins.top() + margins.bottom());
}

bool ZzToggleSwitch::hitButton(const QPoint &position) const
{
    return d_ptr->committingDrag || rect().contains(position);
}

bool ZzToggleSwitch::event(QEvent *event)
{
    if (d_ptr && (event->type() == QEvent::UngrabMouse
        || event->type() == QEvent::WindowDeactivate
        || event->type() == QEvent::FocusOut
        || event->type() == QEvent::EnabledChange
        || (d_ptr->dragCandidate && (event->type() == QEvent::Resize
            || event->type() == QEvent::ContentsRectChange)))) {
        // Cancel before Qt's focus/enable handlers can turn an interrupted
        // thumb gesture into released() inside QWidget's focus transition.
        d_ptr->finishImmediately();
    }
    return QCheckBox::event(event);
}

void ZzToggleSwitch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isEnabled()) {
        d_ptr->canceledUntilRelease = false;
        const QRectF knob = zzKnobRect(zzTrackRect(this), isRightToLeft(),
            d_ptr->progress, underMouse(), false);
        d_ptr->dragCandidate = knob.contains(event->position());
        d_ptr->dragging = false;
        d_ptr->pressPosition = event->position();
        d_ptr->pressProgress = d_ptr->progress;
        if (d_ptr->dragCandidate) {
            d_ptr->animation->stop();
        }
    }
    // pressed() receivers may delete this widget.
    QCheckBox::mousePressEvent(event);
}

void ZzToggleSwitch::mouseMoveEvent(QMouseEvent *event)
{
    if (d_ptr->canceledUntilRelease) {
        event->accept();
        return;
    }
    if (!d_ptr->dragCandidate || !isEnabled()) {
        QCheckBox::mouseMoveEvent(event);
        return;
    }
    const qreal delta = event->position().x() - d_ptr->pressPosition.x();
    const int threshold = std::max(2, QApplication::startDragDistance() / 3);
    if (!d_ptr->dragging && qAbs(delta) < threshold) {
        event->accept();
        return;
    }
    d_ptr->dragging = true;
    d_ptr->animation->stop();
    d_ptr->progress = zzDragProgress(this, d_ptr->pressPosition,
        d_ptr->pressProgress, event->position());
    setDown(true);
    update();
    event->accept();
}

void ZzToggleSwitch::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && d_ptr->canceledUntilRelease) {
        d_ptr->canceledUntilRelease = false;
        setDown(false);
        QCheckBox::mouseReleaseEvent(event);
        return;
    }
    if (event->button() != Qt::LeftButton || !d_ptr->dragging) {
        const bool resume = event->button() == Qt::LeftButton && d_ptr->dragCandidate;
        if (event->button() == Qt::LeftButton) {
            d_ptr->dragCandidate = false;
        }
        const QPointer<ZzToggleSwitch> guard(this);
        QCheckBox::mouseReleaseEvent(event);
        if (guard && resume && !d_ptr->dragCandidate) {
            // An outside release or exclusive-group click need not change
            // checked, so toggled() alone cannot resume the frozen animation.
            d_ptr->moveTo(isChecked());
        }
        return;
    }
    d_ptr->progress = zzDragProgress(this, d_ptr->pressPosition,
        d_ptr->pressProgress, event->position());
    d_ptr->dragTarget = d_ptr->progress >= 0.5;
    d_ptr->dragCandidate = false;
    d_ptr->dragging = false;
    d_ptr->committingDrag = true;
    const QPointer<ZzToggleSwitch> guard(this);
    // Qt owns exclusive-group rules, released/clicked and group notifications.
    QCheckBox::mouseReleaseEvent(event);
    if (!guard) {
        return;
    }
    d_ptr->committingDrag = false;
    if (!d_ptr->dragCandidate) {
        d_ptr->moveTo(isChecked());
    }
}

void ZzToggleSwitch::nextCheckState()
{
    if (d_ptr->committingDrag) {
        const bool checked = d_ptr->dragTarget;
        d_ptr->committingDrag = false;
        const QPointer<ZzToggleSwitch> guard(this);
        setChecked(checked);
        if (guard) {
            QCheckBox::checkStateSet();
        }
        return;
    }
    QCheckBox::nextCheckState();
}

void ZzToggleSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QStyleOptionButton option;
    initStyleOption(&option);
    const QRect content = rect().marginsRemoved(contentsMargins());
    const bool rightToLeft = option.direction == Qt::RightToLeft;
    const QRect track = zzTrackRect(this);
    const bool hovered = isEnabled() && option.state.testFlag(QStyle::State_MouseOver);
    const bool pressed = isEnabled() && option.state.testFlag(QStyle::State_Sunken);
    const QRectF knob = zzKnobRect(track, rightToLeft, d_ptr->progress, hovered, pressed);

    const QPalette::ColorGroup group = isEnabled()
        ? QPalette::Normal
        : QPalette::Disabled;
    QColor neutral = option.palette.color(group, QPalette::Text);
    neutral.setAlphaF(isEnabled() ? 0.65F : 0.35F);
    const QColor knobColor = isChecked()
        ? (isEnabled() ? zzCheckedKnobColor(option.palette)
                       : option.palette.color(QPalette::Window))
        : neutral;
    const QColor trackColor = isChecked()
        ? (isEnabled() ? ZzControlAppearancePrivate::fill(option.palette, hovered, pressed, knobColor) : neutral)
        : option.palette.color(group, hovered || pressed ? QPalette::Button : QPalette::Window);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(isChecked() ? QPen(Qt::NoPen) : QPen(neutral, 1.0));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(
        QRectF(track).adjusted(0.5, 0.5, -0.5, -0.5),
        zzTrackHeight / 2.0,
        zzTrackHeight / 2.0);
    painter.setBrush(knobColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(knob, knob.height() / 2.0, knob.height() / 2.0);

    if (!text().isEmpty()) {
        QRect textRect = content;
        if (rightToLeft) {
            textRect.setRight(track.left() - zzTextSpacing);
        } else {
            textRect.setLeft(track.right() + zzTextSpacing);
        }
        style()->drawItemText(
            &painter,
            textRect,
            Qt::AlignVCenter | Qt::AlignLeading,
            option.palette,
            isEnabled(),
            text(),
            QPalette::WindowText);
    }

    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.rect = rect().adjusted(1, 1, -1, -1);
        focus.state = option.state;
        focus.direction = option.direction;
        focus.palette = option.palette;
        focus.fontMetrics = option.fontMetrics;
        style()->drawPrimitive(
            QStyle::PE_FrameFocusRect,
            &focus,
            &painter,
            this);
    }
}

void ZzToggleSwitch::changeEvent(QEvent *event)
{
    const QPointer<ZzToggleSwitch> guard(this);
    QCheckBox::changeEvent(event);
    if (!guard || event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::EnabledChange:
    case QEvent::PaletteChange:
    case QEvent::LayoutDirectionChange:
        d_ptr->finishImmediately();
        break;
    case QEvent::StyleChange:
    case QEvent::FontChange:
        d_ptr->finishImmediately();
        updateGeometry();
        break;
    default:
        break;
    }
}

void ZzToggleSwitch::hideEvent(QHideEvent *event)
{
    d_ptr->finishImmediately();
    QCheckBox::hideEvent(event);
}

} // namespace ZzFluentUI
