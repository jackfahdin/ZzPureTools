#include "ZzComboBoxDelegate.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QStyle>

#include "ZzPureTools/Core/ZzApplication.hpp"
#include "ZzPureTools/Core/ZzTheme.hpp"
#include "ZzPaintPrimitives.hpp"

void ZzComboBoxDelegate::paint(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const
{
    if (context.theme == nullptr) {
        return;
    }

    const ZzColorTokens& colors = context.theme->colors();
    const ZzMetricTokens& metrics = context.theme->metrics();

    QColor fill = colors.controlFill;
    QColor stroke = colors.controlStroke;

    if (!context.enabled) {
        fill = colors.controlFillDisabled;
    } else if (context.popupVisible) {
        fill = colors.controlFillPressed;
        stroke = colors.controlStrokeHover;
    } else {
        fill = ZzPaintPrimitives::blend(fill, colors.controlFillHover, context.hoverProgress);
        stroke = ZzPaintPrimitives::blend(stroke, colors.controlStrokeHover, context.hoverProgress);
    }

    ZzPaintPrimitives::drawRoundedSurface(painter, rect, colors, metrics, fill, stroke);
    ZzPaintPrimitives::drawComboArrow(
        painter,
        rect,
        colors,
        context.enabled ? context.state : ZzControlState::Disabled);

    const QRectF textRect(
        rect.left() + metrics.contentPadding,
        rect.top(),
        rect.width() - metrics.contentPadding - 28.0,
        rect.height());
    painter.save();
    painter.setPen(context.enabled ? colors.textPrimary : colors.textDisabled);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, context.text);
    painter.restore();
}

ZzComboBoxItemDelegate::ZzComboBoxItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ZzComboBoxItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    ZzControlState state = ZzControlState::Normal;
    if (!option.state.testFlag(QStyle::State_Enabled)) {
        state = ZzControlState::Disabled;
    } else if (option.state.testFlag(QStyle::State_MouseOver)) {
        state = ZzControlState::Hovered;
    }

    const ZzColorTokens& colors = theme->colors();
    const ZzMetricTokens& metrics = theme->metrics();
    const QRectF rect = option.rect;

    QColor fill = Qt::transparent;
    if (selected) {
        fill = colors.popupItemSelected;
    } else if (state == ZzControlState::Hovered) {
        fill = colors.popupItemHover;
    }

    if (fill.alpha() > 0) {
        QPainterPath path;
        path.addRoundedRect(rect.adjusted(4, 1, -4, -1), metrics.controlRadius - 1, metrics.controlRadius - 1);
        painter->fillPath(path, fill);
    }

    painter->setPen(theme->textColor(state));
    painter->drawText(
        rect.adjusted(metrics.contentPadding, 0, -8, 0),
        Qt::AlignVCenter | Qt::AlignLeft,
        index.data(Qt::DisplayRole).toString());
    painter->restore();
}

QSize ZzComboBoxItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(index)
    ZzTheme* theme = ZzApplication::instance().theme();
    const int height = theme != nullptr ? theme->metrics().controlHeight : 32;
    return QSize(option.rect.width(), height);
}
