#include <ZzFluentUI/ZzToggleSwitch.h>

#include <algorithm>

#include <QtCore/QEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QPainter>
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

} // namespace

ZzToggleSwitch::ZzToggleSwitch(QWidget *parent)
    : QCheckBox(parent)
    , d_ptr(std::make_unique<ZzToggleSwitchPrivate>(this))
{
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
    return rect().contains(position);
}

void ZzToggleSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QStyleOptionButton option;
    initStyleOption(&option);
    const QRect content = rect().marginsRemoved(contentsMargins());
    const bool rightToLeft = option.direction == Qt::RightToLeft;
    const int trackX = rightToLeft
        ? content.right() - zzTrackWidth + 1
        : content.left();
    const QRect track(
        trackX,
        content.center().y() - zzTrackHeight / 2,
        zzTrackWidth,
        zzTrackHeight);
    const bool hovered = isEnabled() && option.state.testFlag(QStyle::State_MouseOver);
    const bool pressed = isEnabled() && option.state.testFlag(QStyle::State_Sunken);
    const qreal knobHeight = hovered && !pressed ? 14.0 : zzKnobExtent;
    const qreal knobWidth = pressed ? 18.0 : knobHeight;
    const qreal visualProgress = rightToLeft
        ? 1.0 - d_ptr->progress
        : d_ptr->progress;
    const qreal travel = zzTrackWidth
        - (2 * zzTrackInset)
        - knobWidth;
    const QRectF knob(
        track.left() + zzTrackInset + (travel * visualProgress),
        track.top() + (zzTrackHeight - knobHeight) / 2.0,
        knobWidth,
        knobHeight);

    const QPalette::ColorGroup group = isEnabled()
        ? QPalette::Normal
        : QPalette::Disabled;
    QColor neutral = option.palette.color(group, QPalette::Text);
    neutral.setAlphaF(isEnabled() ? 0.65F : 0.35F);
    const QColor trackColor = isChecked()
        ? (isEnabled() ? ZzControlAppearancePrivate::fill(option.palette, hovered, pressed) : neutral)
        : option.palette.color(group, hovered || pressed ? QPalette::Button : QPalette::Window);
    const QColor knobColor = isChecked()
        ? (isEnabled() ? ZzControlAppearancePrivate::text(option.palette)
                       : option.palette.color(QPalette::Window))
        : neutral;

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
    painter.drawRoundedRect(knob, knobHeight / 2.0, knobHeight / 2.0);

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
    QCheckBox::changeEvent(event);
    if (event == nullptr) {
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
