#include "ZzImageCardPrivate.h"

#include <algorithm>

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPalette>
#include <QtGui/QPen>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionButton>

#include <ZzFluentUI/ZzImageCard.h>

namespace ZzFluentUI {

namespace {

constexpr int zzImageCardPadding = 12;
constexpr int zzImageCardTextSpacing = 2;
constexpr int zzImageCardEmptyTitleHeight = 52;
constexpr int zzImageCardDescriptionHeight = 72;
constexpr int zzImageCardPlaceholderExtent = 32;

/** @brief 返回当前图片卡状态应使用的 palette group。 */
QPalette::ColorGroup zzImageCardColorGroup(const ZzImageCard *card)
{
    if (!card->isEnabled()) {
        return QPalette::Disabled;
    }
    return card->isActiveWindow()
        ? QPalette::Active
        : QPalette::Inactive;
}

/** @brief 返回图片卡标题使用的半粗体字体。 */
QFont zzImageCardTitleFont(const ZzImageCard *card)
{
    QFont result = card->font();
    result.setWeight(QFont::DemiBold);
    return result;
}

} // namespace

ZzImageCardPrivate::ZzImageCardPrivate(ZzImageCard *q) noexcept
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzImageCardPrivate::initStyleOption(
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

ZzImageCardLayout ZzImageCardPrivate::layout() const
{
    ZzImageCardLayout result;
    const QRect bounds = q_ptr->rect().adjusted(1, 1, -1, -1);
    if (bounds.isEmpty()) {
        return result;
    }
    const int contentHeight = description.isEmpty()
        ? zzImageCardEmptyTitleHeight
        : zzImageCardDescriptionHeight;
    const int imageHeight = std::max(0, bounds.height() - contentHeight);
    result.imageRect = QRect(
        bounds.left(),
        bounds.top(),
        bounds.width(),
        imageHeight);
    const QRect textContent(
        bounds.left() + zzImageCardPadding,
        result.imageRect.bottom() + 1,
        std::max(0, bounds.width() - 2 * zzImageCardPadding),
        contentHeight);
    if (textContent.isEmpty()) {
        return result;
    }

    const QFontMetrics titleMetrics(zzImageCardTitleFont(q_ptr));
    const int titleHeight = titleMetrics.height();
    if (description.isEmpty()) {
        result.titleRect = QRect(
            textContent.left(),
            textContent.center().y() - titleHeight / 2,
            textContent.width(),
            titleHeight);
        return result;
    }

    const QFontMetrics descriptionMetrics(q_ptr->font());
    const int descriptionHeight = descriptionMetrics.height();
    const int totalTextHeight = titleHeight
        + zzImageCardTextSpacing
        + descriptionHeight;
    const int textTop = textContent.center().y() - totalTextHeight / 2;
    result.titleRect = QRect(
        textContent.left(),
        textTop,
        textContent.width(),
        titleHeight);
    result.descriptionRect = QRect(
        textContent.left(),
        textTop + titleHeight + zzImageCardTextSpacing,
        textContent.width(),
        descriptionHeight);
    return result;
}

QRectF ZzImageCardPrivate::sourceRectFor(const QRectF &target) const
{
    const QRectF source(pixmap.rect());
    if (source.isEmpty()
        || target.isEmpty()
        || aspectRatioMode != Qt::KeepAspectRatioByExpanding) {
        return source;
    }
    const qreal sourceRatio = source.width() / source.height();
    const qreal targetRatio = target.width() / target.height();
    QRectF cropped = source;
    if (sourceRatio > targetRatio) {
        const qreal width = source.height() * targetRatio;
        cropped.setLeft(source.center().x() - width / 2.0);
        cropped.setWidth(width);
    } else if (sourceRatio < targetRatio) {
        const qreal height = source.width() / targetRatio;
        cropped.setTop(source.center().y() - height / 2.0);
        cropped.setHeight(height);
    }
    return cropped;
}

QRectF ZzImageCardPrivate::targetRectFor(const QRectF &target) const
{
    if (target.isEmpty()
        || pixmap.isNull()
        || pixmap.height() <= 0
        || aspectRatioMode != Qt::KeepAspectRatio) {
        return target;
    }
    const qreal sourceRatio = static_cast<qreal>(pixmap.width())
        / static_cast<qreal>(pixmap.height());
    const qreal targetRatio = target.width() / target.height();
    QRectF fitted = target;
    if (sourceRatio > targetRatio) {
        const qreal height = target.width() / sourceRatio;
        fitted.setTop(target.center().y() - height / 2.0);
        fitted.setHeight(height);
    } else if (sourceRatio < targetRatio) {
        const qreal width = target.height() * sourceRatio;
        fitted.setLeft(target.center().x() - width / 2.0);
        fitted.setWidth(width);
    }
    return fitted;
}

void ZzImageCardPrivate::paint(QPainter *painter) const
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
    const ZzImageCardLayout cardLayout = layout();
    const QPalette::ColorGroup group = zzImageCardColorGroup(q_ptr);
    const QPalette::ColorRole titleRole = q_ptr->isChecked()
        ? QPalette::HighlightedText
        : QPalette::ButtonText;
    const QPalette::ColorRole descriptionRole = q_ptr->isChecked()
        ? QPalette::HighlightedText
        : QPalette::PlaceholderText;
    const qreal radius = static_cast<qreal>(std::max(
        4,
        q_ptr->style()->pixelMetric(
            QStyle::PM_ButtonMargin,
            &option,
            q_ptr)
            / 2));

    painter->save();
    painter->setRenderHints(
        QPainter::Antialiasing
            | QPainter::TextAntialiasing
            | QPainter::SmoothPixmapTransform,
        true);
    if (!cardLayout.imageRect.isEmpty()) {
        QPainterPath imageClip;
        imageClip.addRoundedRect(
            QRectF(cardLayout.imageRect),
            radius,
            radius);
        painter->save();
        painter->setClipPath(imageClip);
        painter->fillRect(
            cardLayout.imageRect,
            q_ptr->palette().color(group, QPalette::AlternateBase));
        if (!pixmap.isNull()) {
            painter->setOpacity(q_ptr->isEnabled() ? 1.0 : 0.45);
            const QRectF target = targetRectFor(cardLayout.imageRect);
            painter->drawPixmap(target, pixmap, sourceRectFor(target));
        } else {
            const QRect placeholder(
                cardLayout.imageRect.center().x()
                    - zzImageCardPlaceholderExtent / 2,
                cardLayout.imageRect.center().y()
                    - zzImageCardPlaceholderExtent / 2,
                zzImageCardPlaceholderExtent,
                zzImageCardPlaceholderExtent);
            q_ptr->style()
                ->standardIcon(QStyle::SP_FileIcon, &option, q_ptr)
                .paint(
                    painter,
                    placeholder,
                    Qt::AlignCenter,
                    q_ptr->isEnabled() ? QIcon::Normal : QIcon::Disabled);
        }
        painter->restore();
    }

    const int textAlignment = Qt::AlignLeading | Qt::AlignVCenter;
    const QFont titleFont = zzImageCardTitleFont(q_ptr);
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
    painter->restore();
}

} // namespace ZzFluentUI
