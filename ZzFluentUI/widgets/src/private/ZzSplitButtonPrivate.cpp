#include "ZzSplitButtonPrivate.h"

#include <algorithm>

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionButton>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzSplitButton.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzSplitButtonPrivate::ZzSplitButtonPrivate(ZzSplitButton *q)
    : q_ptr(q)
    , theme(q)
{
    Q_ASSERT(q_ptr != nullptr);
    q_ptr->setMouseTracking(true);
    q_ptr->setFocusPolicy(Qt::StrongFocus);
}

ZzSplitButtonPrivate::~ZzSplitButtonPrivate()
{
    QObject::disconnect(menuDestroyedConnection);
    QObject::disconnect(menuAboutToHideConnection);
}

ZzSplitButtonRegions ZzSplitButtonPrivate::regions() const
{
    const QRect contents = q_ptr->rect();
    const int availableWidth = std::max(0, contents.width());
    const int preferredMenuWidth = qCeil(
        theme.snapshot()->metric(ZzMetricToken::SplitButtonMenuExtent));
    const int menuWidth = std::min(
        preferredMenuWidth,
        std::max(0, availableWidth - 1));
    const QRect logicalMain(
        contents.left(),
        contents.top(),
        availableWidth - menuWidth,
        contents.height());
    const QRect logicalMenu(
        logicalMain.right() + 1,
        contents.top(),
        menuWidth,
        contents.height());
    return {
        QStyle::visualRect(
            q_ptr->layoutDirection(), contents, logicalMain),
        QStyle::visualRect(
            q_ptr->layoutDirection(), contents, logicalMenu)};
}

void ZzSplitButtonPrivate::updateHover(const QPoint &position)
{
    const ZzSplitButtonRegions current = regions();
    const bool nextMain = q_ptr->isEnabled()
        && current.main.contains(position);
    const bool nextMenu = q_ptr->isEnabled()
        && current.menu.contains(position);
    if (mainHovered == nextMain && menuHovered == nextMenu) {
        return;
    }
    mainHovered = nextMain;
    menuHovered = nextMenu;
    q_ptr->update();
}

void ZzSplitButtonPrivate::setAppearance(ZzButtonAppearance value)
{
    if (appearance == value) {
        return;
    }
    appearance = value;
    q_ptr->update();
    Q_EMIT q_ptr->appearanceChanged(value);
}

void ZzSplitButtonPrivate::setMenu(QMenu *value)
{
    if (menu == value) {
        return;
    }
    QMenu *const previous = menu.data();
    if (menuOpen && previous != nullptr && previous->isVisible()) {
        previous->hide();
    }
    QObject::disconnect(menuDestroyedConnection);
    QObject::disconnect(menuAboutToHideConnection);
    menuDestroyedConnection = {};
    menuAboutToHideConnection = {};
    menu = value;
    menuOpen = false;

    if (value != nullptr) {
        menuDestroyedConnection = QObject::connect(
            value,
            &QObject::destroyed,
            q_ptr,
            [this] {
                menu = nullptr;
                menuOpen = false;
                menuDestroyedConnection = {};
                menuAboutToHideConnection = {};
                q_ptr->update();
                Q_EMIT q_ptr->menuChanged(nullptr);
            });
        menuAboutToHideConnection = QObject::connect(
            value,
            &QMenu::aboutToHide,
            q_ptr,
            [this] {
                menuOpen = false;
                q_ptr->update();
            });
    }
    q_ptr->update();
    Q_EMIT q_ptr->menuChanged(value);
}

void ZzSplitButtonPrivate::showMenu()
{
    if (!q_ptr->isEnabled()) {
        return;
    }
    Q_EMIT q_ptr->menuRequested();
    QMenu *const target = menu.data();
    if (target == nullptr || target->isVisible()) {
        return;
    }

    target->ensurePolished();
    const QSize popupSize = target->sizeHint();
    const QRect bounds = q_ptr->rect();
    QPoint anchor = q_ptr->mapToGlobal(
        QPoint(bounds.left(), bounds.bottom() + 1));
    if (q_ptr->layoutDirection() == Qt::RightToLeft) {
        anchor.setX(q_ptr->mapToGlobal(
            QPoint(bounds.right() + 1, bounds.bottom() + 1)).x()
            - popupSize.width());
    }
    menuOpen = true;
    q_ptr->update();
    target->popup(anchor);
}

void ZzSplitButtonPrivate::paint(QPainter *painter) const
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    QStyleOptionButton option;
    initStyleOption(&option);
    const auto snapshot = theme.snapshot();
    const ZzSplitButtonRegions current = regions();
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const bool accentAppearance = appearance == ZzButtonAppearance::Accent
        || option.features.testFlag(QStyleOptionButton::DefaultButton)
        || option.state.testFlag(QStyle::State_On);
    const bool subtleAppearance = appearance == ZzButtonAppearance::Subtle;
    const bool mainPressed = option.state.testFlag(QStyle::State_Sunken)
        && !menuPressed;
    const bool trailingPressed = menuArmed || menuOpen;

    QColor baseFill = snapshot->color(ZzColorToken::ControlFill);
    if (!enabled) {
        baseFill = snapshot->color(ZzColorToken::ControlFillDisabled);
    } else if (accentAppearance) {
        baseFill = option.palette.color(QPalette::Highlight);
    } else if (subtleAppearance) {
        baseFill.setAlpha(0);
    }
    QColor stroke = snapshot->color(ZzColorToken::ControlStroke);
    if (subtleAppearance && enabled
        && snapshot->mode() != ZzThemeMode::HighContrast
        && !mainHovered && !menuHovered
        && !mainPressed && !trailingPressed) {
        stroke.setAlpha(0);
    }

    const qreal radius = snapshot->metric(
        ZzMetricToken::CornerRadiusMedium);
    const qreal strokeWidth = snapshot->metric(
        ZzMetricToken::StrokeThin);
    const QRectF surface = QRectF(option.rect).adjusted(
        strokeWidth / 2.0,
        strokeWidth / 2.0,
        -strokeWidth / 2.0,
        -strokeWidth / 2.0);
    QPainterPath clipPath;
    clipPath.addRoundedRect(surface, radius, radius);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(baseFill);
    painter->drawRoundedRect(surface, radius, radius);
    if (enabled && !accentAppearance) {
        painter->setClipPath(clipPath);
        const auto drawInteraction = [painter, snapshot](
                                         const QRect &rect,
                                         bool hovered,
                                         bool pressed) {
            if (!hovered && !pressed) {
                return;
            }
            painter->fillRect(
                rect,
                snapshot->color(pressed
                        ? ZzColorToken::ControlFillPressed
                        : ZzColorToken::ControlFillHover));
        };
        drawInteraction(current.main, mainHovered, mainPressed);
        drawInteraction(
            current.menu,
            menuHovered || menuOpen,
            trailingPressed);
        painter->setClipping(false);
    }
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(stroke, strokeWidth));
    painter->drawRoundedRect(surface, radius, radius);

    if (current.menu.width() > 0) {
        const qreal separatorX = q_ptr->layoutDirection()
                == Qt::RightToLeft
            ? static_cast<qreal>(current.menu.right()) + 0.5
            : static_cast<qreal>(current.menu.left()) - 0.5;
        painter->setPen(QPen(
            snapshot->color(ZzColorToken::ControlStroke),
            strokeWidth));
        painter->drawLine(
            QPointF(separatorX, current.menu.top() + 2.0),
            QPointF(separatorX, current.menu.bottom() - 2.0));
    }
    painter->restore();

    QStyleOptionButton labelOption = option;
    labelOption.rect = current.main;
    labelOption.features &= ~QStyleOptionButton::HasMenu;
    if (!mainHovered) {
        labelOption.state &= ~QStyle::State_MouseOver;
    }
    if (menuPressed) {
        labelOption.state &= ~QStyle::State_Sunken;
    }
    if (accentAppearance) {
        labelOption.palette.setColor(
            QPalette::ButtonText,
            option.palette.color(QPalette::HighlightedText));
    }
    q_ptr->style()->drawControl(
        QStyle::CE_PushButtonLabel,
        &labelOption,
        painter,
        q_ptr);

    if (current.menu.width() > 0) {
        const int iconExtent = std::max(
            1,
            std::min({
                qCeil(snapshot->metric(ZzMetricToken::IconSmall)),
                current.menu.width(),
                current.menu.height()}));
        const QRect iconRect(
            current.menu.center().x() - iconExtent / 2,
            current.menu.center().y() - iconExtent / 2,
            iconExtent,
            iconExtent);
        const QPalette::ColorGroup group = enabled
            ? QPalette::Normal
            : QPalette::Disabled;
        const QColor iconColor = accentAppearance && enabled
            ? option.palette.color(QPalette::HighlightedText)
            : option.palette.color(group, QPalette::ButtonText);
        auto *fluentStyle = qobject_cast<ZzFluentStyle *>(q_ptr->style());
        if (fluentStyle != nullptr) {
            const QPixmap pixmap = fluentStyle->iconPixmap(
                ZzIconDescriptor::fromFontIcon(ZzFontIcon::ChevronDown),
                iconRect.size(),
                q_ptr->devicePixelRatioF(),
                iconColor,
                q_ptr->layoutDirection());
            if (!pixmap.isNull()) {
                painter->drawPixmap(iconRect, pixmap);
            }
        } else {
            QStyleOption arrowOption;
            arrowOption.initFrom(q_ptr);
            arrowOption.rect = iconRect;
            q_ptr->style()->drawPrimitive(
                QStyle::PE_IndicatorArrowDown,
                &arrowOption,
                painter,
                q_ptr);
        }
    }

    bool showFocus = option.state.testFlag(QStyle::State_HasFocus);
    if (const auto *fluentStyle =
            qobject_cast<const ZzFluentStyle *>(q_ptr->style())) {
        showFocus = showFocus
            && fluentStyle->isFocusVisualVisible(q_ptr);
    }
    if (showFocus) {
        ZzFluentPainter::drawFocusRing(
            painter,
            QRectF(option.rect).adjusted(2.0, 2.0, -2.0, -2.0),
            *snapshot,
            q_ptr->devicePixelRatioF());
    }
}

void ZzSplitButtonPrivate::refreshTheme()
{
    theme.refreshFallback();
    q_ptr->updateGeometry();
    q_ptr->update();
}

void ZzSplitButtonPrivate::initStyleOption(
    QStyleOptionButton *option) const
{
    Q_ASSERT(option != nullptr);
    q_ptr->initStyleOption(option);
    option->features &= ~QStyleOptionButton::HasMenu;
    if (appearance == ZzButtonAppearance::Accent) {
        option->palette.setColor(
            QPalette::Button,
            option->palette.color(QPalette::Highlight));
        option->palette.setColor(
            QPalette::ButtonText,
            option->palette.color(QPalette::HighlightedText));
    } else if (appearance == ZzButtonAppearance::Subtle) {
        QColor fill = option->palette.color(QPalette::Button);
        fill.setAlpha(0);
        option->palette.setColor(QPalette::Button, fill);
    }
}

} // namespace ZzFluentUI
