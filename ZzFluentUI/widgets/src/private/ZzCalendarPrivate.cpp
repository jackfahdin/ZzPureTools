#include "ZzCalendarPrivate.h"

#include <algorithm>

#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtGui/QPen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzFluentStyle.h>

namespace ZzFluentUI {

ZzCalendarPrivate::ZzCalendarPrivate(ZzCalendar *q)
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
    for (std::size_t index = 0; index < dayTexts.size(); ++index) {
        dayTexts[index] = QString::number(static_cast<int>(index) + 1);
    }
}

void ZzCalendarPrivate::paintCell(
    QPainter *painter,
    const QRect &rect,
    QDate date) const
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    Q_ASSERT(date.isValid());
    if (painter == nullptr || !painter->isActive() || !date.isValid()) {
        return;
    }

    const bool withinRange = date >= q_ptr->minimumDate()
        && date <= q_ptr->maximumDate();
    const bool enabled = q_ptr->isEnabled() && withinRange;
    const bool selected = enabled
        && q_ptr->selectionMode() == QCalendarWidget::SingleSelection
        && date == q_ptr->selectedDate();
    const bool today = date == QDate::currentDate();
    const bool adjacentMonth = date.year() != q_ptr->yearShown()
        || date.month() != q_ptr->monthShown();
    const QPalette::ColorGroup activeGroup = q_ptr->isActiveWindow()
        ? QPalette::Active
        : QPalette::Inactive;
    const QPalette::ColorGroup textGroup = enabled && !adjacentMonth
        ? activeGroup
        : QPalette::Disabled;
    const int buttonMargin = q_ptr->style()->pixelMetric(
        QStyle::PM_ButtonMargin,
        nullptr,
        q_ptr);
    const qreal radius = static_cast<qreal>(std::max(2, buttonMargin / 2));
    const qreal inset = std::max(1.0, radius / 2.0);
    const qreal devicePixelRatio = std::max(1.0, q_ptr->devicePixelRatioF());
    const qreal strokeWidth = 1.0 / devicePixelRatio;
    const QRectF cellRect = QRectF(rect).adjusted(
        inset,
        inset,
        -inset,
        -inset);
    const qreal diameter = std::max(
        0.0,
        std::min(cellRect.width(), cellRect.height()));
    const QRectF stateRect(
        cellRect.center().x() - diameter / 2.0,
        cellRect.center().y() - diameter / 2.0,
        diameter,
        diameter);

    painter->save();
    painter->setRenderHints(
        QPainter::Antialiasing | QPainter::TextAntialiasing,
        true);
    painter->setFont(q_ptr->font());

    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(q_ptr->palette().color(
            activeGroup,
            QPalette::Highlight));
        painter->drawEllipse(stateRect);
    } else if (today && enabled) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(
            q_ptr->palette().color(activeGroup, QPalette::Highlight),
            strokeWidth));
        painter->drawEllipse(stateRect);
    }

    const QWidget *focusWidget = QApplication::focusWidget();
    const bool hasFocusWithin = q_ptr->hasFocus()
        || (focusWidget != nullptr && q_ptr->isAncestorOf(focusWidget));
    const auto *fluentStyle = qobject_cast<const ZzFluentStyle *>(
        q_ptr->style());
    const bool showFocusVisual = fluentStyle != nullptr
        ? fluentStyle->isFocusVisualVisible(q_ptr)
        : hasFocusWithin;
    if (selected && hasFocusWithin && showFocusVisual) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(
            q_ptr->palette().color(activeGroup, QPalette::HighlightedText),
            strokeWidth));
        painter->drawEllipse(stateRect.adjusted(
            strokeWidth,
            strokeWidth,
            -strokeWidth,
            -strokeWidth));
    }

    const QPalette::ColorRole textRole = selected
        ? QPalette::HighlightedText
        : QPalette::Text;
    const QPalette::ColorGroup colorGroup = selected
        ? activeGroup
        : textGroup;
    painter->setPen(q_ptr->palette().color(colorGroup, textRole));
    painter->drawText(
        rect,
        Qt::AlignCenter,
        dayTexts.at(static_cast<std::size_t>(date.day() - 1)));
    painter->restore();
}

} // namespace ZzFluentUI
