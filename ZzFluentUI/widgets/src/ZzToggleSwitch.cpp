#include <ZzFluentUI/ZzToggleSwitch.h>

#include <algorithm>
#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOptionButton>

#include "private/ZzToggleSwitchPrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int zzTrackWidth = 40;
constexpr int zzTrackHeight = 20;
constexpr int zzKnobExtent = 16;
constexpr int zzTrackInset = 2;
constexpr int zzTextSpacing = 8;

} // namespace

ZzToggleSwitch::ZzToggleSwitch(QWidget *parent)
    : QCheckBox(parent)
    , d_ptr(std::make_unique<ZzToggleSwitchPrivate>(this))
{
    d_ptr->progress = isChecked() ? 1.0 : 0.0;
    connect(
        this,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            d_ptr->moveTo(checked);
        });
}

ZzToggleSwitch::ZzToggleSwitch(QString text, QWidget *parent)
    : ZzToggleSwitch(parent)
{
    setText(std::move(text));
}

ZzToggleSwitch::~ZzToggleSwitch() = default;

QSize ZzToggleSwitch::sizeHint() const
{
    const QMargins margins = contentsMargins();
    const int textWidth = text().isEmpty()
        ? 0
        : fontMetrics().horizontalAdvance(text()) + zzTextSpacing;
    const int contentHeight = std::max(
        zzTrackHeight,
        text().isEmpty() ? 0 : fontMetrics().height());
    return QSize(
        zzTrackWidth + textWidth + margins.left() + margins.right(),
        contentHeight + margins.top() + margins.bottom());
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
    const qreal visualProgress = rightToLeft
        ? 1.0 - d_ptr->progress
        : d_ptr->progress;
    const qreal travel = zzTrackWidth
        - (2 * zzTrackInset)
        - zzKnobExtent;
    const QRectF knob(
        track.left() + zzTrackInset + (travel * visualProgress),
        track.top() + zzTrackInset,
        zzKnobExtent,
        zzKnobExtent);

    const QPalette::ColorGroup group = isEnabled()
        ? QPalette::Normal
        : QPalette::Disabled;
    const QColor trackColor = isChecked()
        ? option.palette.color(group, QPalette::Highlight)
        : option.palette.color(group, QPalette::Mid);
    const QColor knobColor = isChecked()
        ? option.palette.color(group, QPalette::HighlightedText)
        : option.palette.color(group, QPalette::Base);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(
        QRectF(track),
        zzTrackHeight / 2.0,
        zzTrackHeight / 2.0);
    painter.setBrush(knobColor);
    painter.drawEllipse(knob);

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
