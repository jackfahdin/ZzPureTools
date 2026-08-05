#include "ZzActionCardPrivate.h"

#include <algorithm>

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionButton>

#include <ZzFluentUI/ZzActionCard.h>

namespace ZzFluentUI {

namespace {

constexpr int zzCardHorizontalPadding = 12;
constexpr int zzCardVerticalPadding = 10;
constexpr int zzCardContentSpacing = 10;
constexpr int zzCardTextSpacing = 2;
constexpr int zzCardIndicatorExtent = 16;

/** @brief 返回当前按钮状态应使用的 palette group。 */
QPalette::ColorGroup zzCardColorGroup(const ZzActionCard *card)
{
    if (!card->isEnabled()) {
        return QPalette::Disabled;
    }
    return card->isActiveWindow()
        ? QPalette::Active
        : QPalette::Inactive;
}

/** @brief 返回用于标题的半粗体字体。 */
QFont zzCardTitleFont(const ZzActionCard *card)
{
    QFont result = card->font();
    result.setWeight(QFont::DemiBold);
    return result;
}

} // namespace

ZzActionCardPrivate::ZzActionCardPrivate(ZzActionCard *q) noexcept
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzActionCardPrivate::initStyleOption(
    QStyleOptionButton *option) const
{
    Q_ASSERT(option != nullptr);
    if (option == nullptr) {
        return;
    }
    option->initFrom(q_ptr);
    option->rect = q_ptr->rect();
    option->text.clear();
    option->icon = {};
    option->iconSize = {};
    if (q_ptr->isDown()) {
        option->state |= QStyle::State_Sunken;
    }
    if (q_ptr->isChecked()) {
        option->state |= QStyle::State_On;
    } else {
        option->state |= QStyle::State_Off;
    }
}

ZzActionCardLayout ZzActionCardPrivate::layout() const
{
    ZzActionCardLayout result;
    const QRect bounds = q_ptr->rect();
    const QRect content = bounds.adjusted(
        zzCardHorizontalPadding,
        zzCardVerticalPadding,
        -zzCardHorizontalPadding,
        -zzCardVerticalPadding);
    if (content.isEmpty()) {
        return result;
    }

    int logicalLeft = content.left();
    int logicalRight = content.right();
    if (!q_ptr->icon().isNull()) {
        const int styleIconExtent = q_ptr->style()->pixelMetric(
            QStyle::PM_SmallIconSize,
            nullptr,
            q_ptr);
        const QSize requested = q_ptr->iconSize().isValid()
            ? q_ptr->iconSize()
            : QSize(styleIconExtent, styleIconExtent);
        const int extent = std::clamp(
            std::max(requested.width(), requested.height()),
            1,
            content.height());
        const QRect logicalIcon(
            logicalLeft,
            content.center().y() - extent / 2,
            extent,
            extent);
        result.iconRect = QStyle::visualRect(
            q_ptr->layoutDirection(),
            bounds,
            logicalIcon);
        logicalLeft += extent + zzCardContentSpacing;
    }

    if (trailingIndicatorVisible) {
        const QRect logicalIndicator(
            logicalRight - zzCardIndicatorExtent + 1,
            content.center().y() - zzCardIndicatorExtent / 2,
            zzCardIndicatorExtent,
            zzCardIndicatorExtent);
        result.indicatorRect = QStyle::visualRect(
            q_ptr->layoutDirection(),
            bounds,
            logicalIndicator);
        logicalRight -= zzCardIndicatorExtent + zzCardContentSpacing;
    }

    const int textWidth = std::max(0, logicalRight - logicalLeft + 1);
    if (textWidth <= 0) {
        return result;
    }
    const QFontMetrics titleMetrics(zzCardTitleFont(q_ptr));
    const QFontMetrics descriptionMetrics(q_ptr->font());
    const int titleHeight = titleMetrics.height();
    if (description.isEmpty()) {
        const QRect logicalTitle(
            logicalLeft,
            content.center().y() - titleHeight / 2,
            textWidth,
            titleHeight);
        result.titleRect = QStyle::visualRect(
            q_ptr->layoutDirection(),
            bounds,
            logicalTitle);
        return result;
    }

    const int descriptionHeight = descriptionMetrics.height();
    const int textHeight = titleHeight
        + zzCardTextSpacing
        + descriptionHeight;
    const int textTop = content.center().y() - textHeight / 2;
    const QRect logicalTitle(
        logicalLeft,
        textTop,
        textWidth,
        titleHeight);
    const QRect logicalDescription(
        logicalLeft,
        textTop + titleHeight + zzCardTextSpacing,
        textWidth,
        descriptionHeight);
    result.titleRect = QStyle::visualRect(
        q_ptr->layoutDirection(),
        bounds,
        logicalTitle);
    result.descriptionRect = QStyle::visualRect(
        q_ptr->layoutDirection(),
        bounds,
        logicalDescription);
    return result;
}

void ZzActionCardPrivate::paint(QPainter *painter) const
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    if (painter == nullptr || !painter->isActive()) {
        return;
    }

    QStyleOptionButton option;
    initStyleOption(&option);
    q_ptr->style()->drawControl(
        QStyle::CE_PushButton,
        &option,
        painter,
        q_ptr);

    const ZzActionCardLayout cardLayout = layout();
    const QPalette::ColorGroup group = zzCardColorGroup(q_ptr);
    const QPalette::ColorRole titleRole = q_ptr->isChecked()
        ? QPalette::HighlightedText
        : QPalette::ButtonText;
    const QPalette::ColorRole descriptionRole = q_ptr->isChecked()
        ? QPalette::HighlightedText
        : QPalette::PlaceholderText;
    const QIcon::Mode iconMode = q_ptr->isEnabled()
        ? QIcon::Normal
        : QIcon::Disabled;
    const QIcon::State iconState = q_ptr->isChecked()
        ? QIcon::On
        : QIcon::Off;

    painter->save();
    painter->setRenderHints(
        QPainter::Antialiasing | QPainter::TextAntialiasing,
        true);
    if (!cardLayout.iconRect.isEmpty()) {
        q_ptr->icon().paint(
            painter,
            cardLayout.iconRect,
            Qt::AlignCenter,
            iconMode,
            iconState);
    }

    const int textAlignment = Qt::AlignLeading | Qt::AlignVCenter;
    const QFont titleFont = zzCardTitleFont(q_ptr);
    painter->setFont(titleFont);
    painter->setPen(q_ptr->palette().color(group, titleRole));
    const QFontMetrics titleMetrics(titleFont);
    painter->drawText(
        cardLayout.titleRect,
        textAlignment,
        titleMetrics.elidedText(
            q_ptr->text(),
            Qt::ElideRight,
            cardLayout.titleRect.width()));

    if (!cardLayout.descriptionRect.isEmpty()) {
        painter->setFont(q_ptr->font());
        painter->setPen(q_ptr->palette().color(group, descriptionRole));
        const QFontMetrics descriptionMetrics(q_ptr->font());
        painter->drawText(
            cardLayout.descriptionRect,
            textAlignment,
            descriptionMetrics.elidedText(
                description,
                Qt::ElideRight,
                cardLayout.descriptionRect.width()));
    }

    if (!cardLayout.indicatorRect.isEmpty()) {
        q_ptr->style()
            ->standardIcon(QStyle::SP_ArrowForward, &option, q_ptr)
            .paint(
                painter,
                cardLayout.indicatorRect,
                Qt::AlignCenter,
                iconMode,
                iconState);
    }
    painter->restore();
}

} // namespace ZzFluentUI
