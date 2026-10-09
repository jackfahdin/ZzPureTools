#include "ZzColorShadeStrip.h"
#include "ZzColorPickerMath.h"
#include "ZzColorPickerMetrics.h"
#include <QtCore/QPointer>

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {
ZzColorShadeStrip::ZzColorShadeStrip(QWidget *parent)
    : QWidget(parent), theme_(this)
{
    setObjectName(QStringLiteral("zzColorShadeStrip"));
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(ZzColorPickerPreviewHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize ZzColorShadeStrip::sizeHint() const
{
    return {240, 28};
}

void ZzColorShadeStrip::setColor(QColor color, qreal hue, qreal saturation)
{
    colors_.clear();
    for (int index = 0; index < 7; ++index) {
        colors_.append(zzColorFromHsv(hue, saturation, 0.15 + index * 0.85 / 6, color.alphaF()));
    }
    update();
}

void ZzColorShadeStrip::paintEvent(QPaintEvent *)
{
    theme_.refreshFallback();
    QPainter painter(this);
    if (!isEnabled()) {
        painter.setOpacity(0.45);
    }
    for (int index = 0; index < colors_.size(); ++index) {
        const int visual = isRightToLeft() ? 6 - index : index;
        const QRect cell(visual * width() / 7, 0, (visual + 1) * width() / 7 - visual * width() / 7, height());
        painter.fillRect(cell.adjusted(1, 0, -1, 0), colors_.at(index));
        if (hasFocus() && index == focused_) {
            painter.setPen(QPen(theme_.snapshot()->color(ZzColorToken::FocusStroke), 2));
            painter.drawRect(cell.adjusted(2, 1, -3, -2));
        }
    }
}

void ZzColorShadeStrip::mousePressEvent(QMouseEvent *event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton || colors_.size() != 7) {
        return;
    }
    focused_ = qBound(0, int(event->position().x() * 7 / qMax(1, width())), 6);
    if (isRightToLeft()) {
        focused_ = 6 - focused_;
    }
    const QPointer<ZzColorShadeStrip> guard(this);
    setFocus(Qt::MouseFocusReason);
    if (!guard) {
        return;
    }
    const auto onSelected = selected;
    const QColor color = colors_.at(focused_);
    if (onSelected) {
        onSelected(color);
    }
}

void ZzColorShadeStrip::keyPressEvent(QKeyEvent *event)
{
    if (!isEnabled() || colors_.size() != 7) {
        return;
    }
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Left) {
        const int direction = (event->key() == Qt::Key_Right ? 1 : -1) * (isRightToLeft() ? -1 : 1);
        focused_ = qBound(0, focused_ + direction, 6);
        update();
    } else if ((event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) && selected) {
        const auto onSelected = selected;
        const QColor color = colors_.at(focused_);
        onSelected(color);
    } else {
        QWidget::keyPressEvent(event);
    }
}
} // namespace ZzFluentUI
