#include "ZzFluentStylePrivate.h"

#include <algorithm>
#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconCacheKey.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "ZzScrollBarPrivate.h"

namespace ZzFluentUI {

ZzFluentStylePrivate::ZzFluentStylePrivate(
    ZzFluentStyle *q,
    ZzThemeController *themeController)
    : q_ptr(q)
    , controller(themeController)
{
    auto *application = qobject_cast<QApplication *>(
        QCoreApplication::instance());
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(themeController != nullptr);
    Q_ASSERT(themeController != nullptr
             && themeController->thread() == q_ptr->thread());
    Q_ASSERT(application != nullptr);
    if (q_ptr == nullptr
        || themeController == nullptr
        || themeController->thread() != q_ptr->thread()
        || application == nullptr) {
        std::terminate();
    }

    snapshot = themeController->snapshot();
    iconRevision = snapshot->revision();
    cache.rebuildVisuals(*snapshot);
    QObject::connect(
        themeController,
        &ZzThemeController::snapshotChanged,
        q_ptr,
        [this](quint64, ZzThemeChangeKinds changes) {
            applySnapshot(changes);
        });
}

QPixmap ZzFluentStylePrivate::iconPixmap(
    const ZzIconDescriptor &descriptor,
    QSize logicalSize,
    qreal devicePixelRatio,
    QColor color,
    Qt::LayoutDirection direction)
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    if (!descriptor.resourceId.startsWith(QStringLiteral(":/"))
        || logicalSize.isEmpty()
        || !color.isValid()) {
        return {};
    }

    const quint16 dprBucket = ZzDpiScale::bucket(devicePixelRatio);
    const qreal effectiveDpr = static_cast<qreal>(dprBucket) / 100.0;
    const bool mirrored = descriptor.mirroredInRightToLeft
        && direction == Qt::RightToLeft;
    const ZzIconCacheKey key(
        descriptor.resourceId,
        mirrored,
        logicalSize,
        dprBucket,
        color.rgba(),
        iconRevision);
    if (const QPixmap *cached = cache.icon(key); cached != nullptr) {
        return *cached;
    }

    const QSize physicalSize(
        ZzDpiScale::physicalPixels(
            logicalSize.width(), effectiveDpr),
        ZzDpiScale::physicalPixels(
            logicalSize.height(), effectiveDpr));
    if (!cache.canCacheIcon(physicalSize)) {
        return {};
    }

    QSvgRenderer renderer(descriptor.resourceId);
    if (!renderer.isValid()) {
        return {};
    }

    QImage image(
        physicalSize,
        QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return {};
    }
    image.fill(Qt::transparent);
    QPainter painter(&image);
    if (mirrored) {
        painter.translate(physicalSize.width(), 0);
        painter.scale(-1.0, 1.0);
    }
    renderer.render(
        &painter,
        QRectF(
            0.0,
            0.0,
            physicalSize.width(),
            physicalSize.height()));
    painter.end();

    QPainter tintPainter(&image);
    tintPainter.setCompositionMode(
        QPainter::CompositionMode_SourceIn);
    tintPainter.fillRect(image.rect(), color);
    tintPainter.end();

    QPixmap rendered = QPixmap::fromImage(std::move(image));
    if (rendered.isNull()) {
        return {};
    }
    rendered.setDevicePixelRatio(effectiveDpr);
    cache.insertIcon(key, rendered);
    return rendered;
}

void ZzFluentStylePrivate::drawCheckIndicator(
    const QStyleOption *option,
    QPainter *painter,
    bool radio) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool checked = option->state.testFlag(QStyle::State_On);
    const bool mixed = option->state.testFlag(QStyle::State_NoChange);
    const bool marked = checked || mixed;
    const QPalette::ColorGroup group = enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    const QColor border = option->palette.color(group, QPalette::Text);
    const QColor fill = marked
        ? option->palette.color(group, QPalette::Highlight)
        : option->palette.color(group, QPalette::Base);
    const QRectF rect = QRectF(option->rect).adjusted(
        1.0,
        1.0,
        -1.0,
        -1.0);
    painter->setPen(QPen(border, 1.0));
    painter->setBrush(fill);
    if (radio) {
        painter->drawEllipse(rect);
    } else {
        painter->drawRoundedRect(rect, 3.0, 3.0);
    }
    if (marked) {
        const QColor mark = option->palette.color(
            group,
            QPalette::HighlightedText);
        painter->setPen(QPen(mark, 2.0));
        if (mixed) {
            painter->drawLine(
                QPointF(rect.left() + 4.0, rect.center().y()),
                QPointF(rect.right() - 4.0, rect.center().y()));
        } else if (radio) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(mark);
            painter->drawEllipse(rect.center(), 4.0, 4.0);
        } else {
            QPainterPath path;
            path.moveTo(rect.left() + 4.0, rect.center().y());
            path.lineTo(
                rect.center().x() - 1.0,
                rect.bottom() - 4.0);
            path.lineTo(
                rect.right() - 3.0,
                rect.top() + 4.0);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(path);
        }
    }
    painter->restore();
}

void ZzFluentStylePrivate::drawPushButton(
    const QStyleOptionButton *option,
    QPainter *painter,
    const QWidget *widget) const
{
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool pressed = option->state.testFlag(QStyle::State_Sunken);
    const bool hovered = option->state.testFlag(QStyle::State_MouseOver);
    const bool checked = option->state.testFlag(QStyle::State_On);
    const bool defaultButton = option->features.testFlag(
        QStyleOptionButton::DefaultButton);
    const QColor paletteButton = option->palette.color(
        enabled ? QPalette::Normal : QPalette::Disabled,
        QPalette::Button);
    const QColor paletteAccent = option->palette.color(
        QPalette::Highlight);
    const bool accentAppearance = checked
        || defaultButton
        || paletteButton == paletteAccent;
    const bool subtleAppearance = paletteButton.alpha() == 0;

    QColor fill = paletteButton;
    if (!enabled) {
        fill = snapshot->color(ZzColorToken::ControlFillDisabled);
    } else if (accentAppearance) {
        fill = paletteAccent.isValid()
            ? paletteAccent
            : snapshot->color(ZzColorToken::Accent);
    } else if (pressed) {
        fill = snapshot->color(ZzColorToken::ControlFillPressed);
    } else if (hovered) {
        fill = snapshot->color(ZzColorToken::ControlFillHover);
    } else if (!fill.isValid()) {
        fill = snapshot->color(ZzColorToken::ControlFill);
    }

    QColor stroke = snapshot->color(ZzColorToken::ControlStroke);
    if (subtleAppearance && enabled && !hovered && !pressed) {
        stroke.setAlpha(0);
    }
    const qreal radius = snapshot->metric(
        ZzMetricToken::CornerRadiusMedium);
    const qreal strokeWidth = snapshot->metric(
        ZzMetricToken::StrokeThin);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(stroke, strokeWidth));
    painter->setBrush(fill);
    painter->drawRoundedRect(
        QRectF(option->rect).adjusted(
            strokeWidth / 2.0,
            strokeWidth / 2.0,
            -strokeWidth / 2.0,
            -strokeWidth / 2.0),
        radius,
        radius);
    painter->restore();

    QStyleOptionButton labelOption = *option;
    if (accentAppearance) {
        labelOption.palette.setColor(
            QPalette::ButtonText,
            option->palette.color(QPalette::HighlightedText));
    }
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_PushButtonLabel,
        &labelOption,
        painter,
        widget);

    if (option->state.testFlag(QStyle::State_HasFocus)) {
        QStyleOptionFocusRect focus;
        focus.rect = option->rect.adjusted(2, 2, -2, -2);
        focus.state = option->state;
        focus.direction = option->direction;
        focus.palette = option->palette;
        focus.fontMetrics = option->fontMetrics;
        q_ptr->drawPrimitive(
            QStyle::PE_FrameFocusRect,
            &focus,
            painter,
            widget);
    }
}

void ZzFluentStylePrivate::drawInputPanel(
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    Q_UNUSED(widget)
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const QPalette::ColorGroup group = enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    QColor fill = option->palette.color(group, QPalette::Base);
    if (!enabled) {
        fill = snapshot->color(ZzColorToken::ControlFillDisabled);
    } else if (option->state.testFlag(QStyle::State_MouseOver)
               && !option->state.testFlag(QStyle::State_HasFocus)) {
        fill = snapshot->color(ZzColorToken::ControlFillHover);
    }
    const QColor stroke = option->state.testFlag(QStyle::State_HasFocus)
        ? option->palette.color(group, QPalette::Highlight)
        : snapshot->color(ZzColorToken::ControlStroke);
    const qreal strokeWidth = snapshot->metric(
        ZzMetricToken::StrokeThin);
    const qreal radius = snapshot->metric(
        ZzMetricToken::CornerRadiusMedium);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(stroke, strokeWidth));
    painter->setBrush(fill);
    painter->drawRoundedRect(
        QRectF(option->rect).adjusted(
            strokeWidth / 2.0,
            strokeWidth / 2.0,
            -strokeWidth / 2.0,
            -strokeWidth / 2.0),
        radius,
        radius);
    painter->restore();
}

void ZzFluentStylePrivate::drawComboBox(
    const QStyleOptionComboBox *option,
    QPainter *painter,
    const QWidget *widget) const
{
    drawInputPanel(option, painter, widget);
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_ComboBoxLabel,
        option,
        painter,
        widget);

    const QRect arrowRect = q_ptr->subControlRect(
        QStyle::CC_ComboBox,
        option,
        QStyle::SC_ComboBoxArrow,
        widget);
    if (arrowRect.isEmpty()) {
        return;
    }
    const QPointF center = QRectF(arrowRect).center();
    const qreal halfWidth = qMin(4.0, arrowRect.width() / 4.0);
    const qreal halfHeight = qMin(2.5, arrowRect.height() / 6.0);
    const bool popupOpen = option->state.testFlag(QStyle::State_On);
    QPainterPath arrow;
    if (popupOpen) {
        arrow.moveTo(center.x() - halfWidth, center.y() + halfHeight);
        arrow.lineTo(center.x(), center.y() - halfHeight);
        arrow.lineTo(center.x() + halfWidth, center.y() + halfHeight);
    } else {
        arrow.moveTo(center.x() - halfWidth, center.y() - halfHeight);
        arrow.lineTo(center.x(), center.y() + halfHeight);
        arrow.lineTo(center.x() + halfWidth, center.y() - halfHeight);
    }
    const QPalette::ColorGroup group = option->state.testFlag(
        QStyle::State_Enabled)
        ? QPalette::Normal
        : QPalette::Disabled;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(
        option->palette.color(group, QPalette::Text),
        1.5,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter->drawPath(arrow);
    painter->restore();
}

QRect ZzFluentStylePrivate::comboBoxSubControlRect(
    const QStyleOptionComboBox *option,
    QStyle::SubControl subControl) const
{
    if (option == nullptr || option->rect.isEmpty()) {
        return {};
    }
    if (subControl == QStyle::SC_ComboBoxFrame) {
        return option->rect;
    }

    const QRect bounds = option->rect;
    const int arrowWidth = qMin(32, bounds.width());
    const QRect logicalArrow(
        bounds.right() - arrowWidth + 1,
        bounds.top(),
        arrowWidth,
        bounds.height());
    if (subControl == QStyle::SC_ComboBoxArrow) {
        return QStyle::visualRect(
            option->direction,
            bounds,
            logicalArrow);
    }
    if (subControl == QStyle::SC_ComboBoxEditField) {
        const int left = qMin(bounds.right() + 1, bounds.left() + 12);
        const int right = logicalArrow.left() - 1;
        if (right < left) {
            return {};
        }
        return QStyle::visualRect(
            option->direction,
            bounds,
            QRect(
                QPoint(left, bounds.top()),
                QPoint(right, bounds.bottom())));
    }
    return {};
}

QStyle::SubControl ZzFluentStylePrivate::hitTestComboBox(
    const QStyleOptionComboBox *option,
    const QPoint &position) const
{
    if (option == nullptr || !option->rect.contains(position)) {
        return QStyle::SC_None;
    }
    for (const QStyle::SubControl subControl : {
             QStyle::SC_ComboBoxArrow,
             QStyle::SC_ComboBoxEditField,
             QStyle::SC_ComboBoxFrame}) {
        if (comboBoxSubControlRect(option, subControl).contains(position)) {
            return subControl;
        }
    }
    return QStyle::SC_None;
}

bool ZzFluentStylePrivate::isComboBoxPopupContext(
    const QWidget *widget) const noexcept
{
    const QWidget *current = widget;
    while (current != nullptr) {
        if (qobject_cast<const QComboBox *>(current) != nullptr) {
            return true;
        }
        current = current->parentWidget();
    }
    return false;
}

void ZzFluentStylePrivate::drawComboBoxPopupItem(
    const QStyleOptionViewItem *option,
    QPainter *painter,
    const QWidget *widget) const
{
    QStyleOptionViewItem adjusted = *option;
    const bool selected = adjusted.state.testFlag(QStyle::State_Selected);
    const bool hovered = adjusted.state.testFlag(QStyle::State_MouseOver);
    const bool enabled = adjusted.state.testFlag(QStyle::State_Enabled);

    if (selected || hovered) {
        const QRectF surface = QRectF(adjusted.rect).adjusted(
            2.0,
            2.0,
            -2.0,
            -2.0);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(
            selected
                ? ZzColorToken::ControlFillPressed
                : ZzColorToken::ControlFillHover));
        painter->drawRoundedRect(
            surface,
            snapshot->metric(ZzMetricToken::CornerRadiusSmall),
            snapshot->metric(ZzMetricToken::CornerRadiusSmall));
        if (selected) {
            const QRect logicalIndicator(
                adjusted.rect.left() + 4,
                adjusted.rect.center().y() - 8,
                3,
                16);
            const QRect indicator = QStyle::visualRect(
                adjusted.direction,
                adjusted.rect,
                logicalIndicator);
            painter->setBrush(snapshot->color(ZzColorToken::Accent));
            painter->drawRoundedRect(QRectF(indicator), 1.5, 1.5);
        }
        painter->restore();
    }

    adjusted.state.setFlag(QStyle::State_Selected, false);
    adjusted.state.setFlag(QStyle::State_MouseOver, false);
    const QPalette::ColorGroup group = enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    adjusted.palette.setColor(
        QPalette::Text,
        adjusted.palette.color(group, QPalette::Text));
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_ItemViewItem,
        &adjusted,
        painter,
        widget);
}

void ZzFluentStylePrivate::drawComboBoxPopupMenuItem(
    const QStyleOptionMenuItem *option,
    QPainter *painter,
    const QWidget *widget) const
{
    QStyleOptionMenuItem adjusted = *option;
    const bool current = adjusted.checked;
    const bool hovered = adjusted.state.testFlag(QStyle::State_Selected)
        || adjusted.state.testFlag(QStyle::State_MouseOver);
    const bool enabled = adjusted.state.testFlag(QStyle::State_Enabled);

    if (current || hovered) {
        const QRectF surface = QRectF(adjusted.rect).adjusted(
            2.0,
            2.0,
            -2.0,
            -2.0);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(
            current
                ? ZzColorToken::ControlFillPressed
                : ZzColorToken::ControlFillHover));
        painter->drawRoundedRect(
            surface,
            snapshot->metric(ZzMetricToken::CornerRadiusSmall),
            snapshot->metric(ZzMetricToken::CornerRadiusSmall));
        if (current) {
            const QRect logicalIndicator(
                adjusted.rect.left() + 4,
                adjusted.rect.center().y() - 8,
                3,
                16);
            const QRect indicator = QStyle::visualRect(
                adjusted.direction,
                adjusted.rect,
                logicalIndicator);
            painter->setBrush(snapshot->color(ZzColorToken::Accent));
            painter->drawRoundedRect(QRectF(indicator), 1.5, 1.5);
        }
        painter->restore();
    }

    adjusted.checked = false;
    adjusted.checkType = QStyleOptionMenuItem::NotCheckable;
    adjusted.state.setFlag(QStyle::State_Selected, false);
    adjusted.state.setFlag(QStyle::State_MouseOver, false);
    const QPalette::ColorGroup group = enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    adjusted.palette.setColor(
        QPalette::Text,
        adjusted.palette.color(group, QPalette::Text));
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_MenuItem,
        &adjusted,
        painter,
        widget);
}

void ZzFluentStylePrivate::drawSpinBox(
    const QStyleOptionSpinBox *option,
    QPainter *painter,
    const QWidget *widget) const
{
    if (option->frame
        && option->subControls.testFlag(QStyle::SC_SpinBoxFrame)) {
        drawInputPanel(option, painter, widget);
    }
    if (option->buttonSymbols == QAbstractSpinBox::NoButtons) {
        return;
    }

    const bool widgetEnabled = option->state.testFlag(
        QStyle::State_Enabled);
    const auto drawButton = [this, option, painter, widgetEnabled](
                                QStyle::SubControl subControl,
                                QAbstractSpinBox::StepEnabledFlag stepFlag,
                                bool increase) {
        if (!option->subControls.testFlag(subControl)) {
            return;
        }
        const QRect rect = spinBoxSubControlRect(option, subControl);
        if (rect.isEmpty()) {
            return;
        }
        const bool stepEnabled = widgetEnabled
            && option->stepEnabled.testFlag(stepFlag);
        const bool active = option->activeSubControls.testFlag(
            subControl);
        const bool hovered = active && option->state.testFlag(
            QStyle::State_MouseOver);
        const bool pressed = active && option->state.testFlag(
            QStyle::State_Sunken);

        QColor fill = Qt::transparent;
        if (pressed) {
            fill = snapshot->color(ZzColorToken::ControlFillPressed);
        } else if (hovered) {
            fill = snapshot->color(ZzColorToken::ControlFillHover);
        }
        const QPalette::ColorGroup group = stepEnabled
            ? QPalette::Normal
            : QPalette::Disabled;
        const QColor glyph = option->palette.color(
            group,
            QPalette::Text);
        const QPointF center = QRectF(rect).center();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRect(rect);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(
            glyph,
            1.5,
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin));
        QPainterPath symbol;
        if (option->buttonSymbols == QAbstractSpinBox::PlusMinus) {
            symbol.moveTo(center.x() - 3.5, center.y());
            symbol.lineTo(center.x() + 3.5, center.y());
            if (increase) {
                symbol.moveTo(center.x(), center.y() - 3.5);
                symbol.lineTo(center.x(), center.y() + 3.5);
            }
        } else {
            const qreal direction = increase ? -1.0 : 1.0;
            symbol.moveTo(
                center.x() - 3.5,
                center.y() - (1.5 * direction));
            symbol.lineTo(center.x(), center.y() + (2.0 * direction));
            symbol.lineTo(
                center.x() + 3.5,
                center.y() - (1.5 * direction));
        }
        painter->drawPath(symbol);
        painter->restore();
    };

    drawButton(
        QStyle::SC_SpinBoxUp,
        QAbstractSpinBox::StepUpEnabled,
        true);
    drawButton(
        QStyle::SC_SpinBoxDown,
        QAbstractSpinBox::StepDownEnabled,
        false);
}

QRect ZzFluentStylePrivate::spinBoxSubControlRect(
    const QStyleOptionSpinBox *option,
    QStyle::SubControl subControl) const
{
    if (option == nullptr || option->rect.isEmpty()) {
        return {};
    }
    const QRect frame = option->rect;
    if (subControl == QStyle::SC_SpinBoxFrame) {
        return frame;
    }

    const bool hasButtons = option->buttonSymbols
        != QAbstractSpinBox::NoButtons;
    const int buttonWidth = hasButtons
        ? std::min(28, frame.width())
        : 0;
    const int contentWidth = std::max(0, frame.width() - buttonWidth);
    const int leftPadding = std::min(8, contentWidth / 2);
    const int rightPadding = std::min(4, std::max(
        0,
        contentWidth - leftPadding));
    const int verticalPadding = frame.height() >= 3 ? 1 : 0;
    const QRect logicalEdit(
        frame.left() + leftPadding,
        frame.top() + verticalPadding,
        std::max(0, contentWidth - leftPadding - rightPadding),
        std::max(0, frame.height() - (2 * verticalPadding)));
    if (subControl == QStyle::SC_SpinBoxEditField) {
        return QStyle::visualRect(
            option->direction,
            frame,
            logicalEdit);
    }
    if (!hasButtons) {
        return {};
    }

    const int upperHeight = (frame.height() + 1) / 2;
    const QRect logicalUp(
        frame.right() - buttonWidth + 1,
        frame.top(),
        buttonWidth,
        upperHeight);
    const QRect logicalDown(
        logicalUp.left(),
        logicalUp.bottom() + 1,
        buttonWidth,
        frame.height() - upperHeight);
    if (subControl == QStyle::SC_SpinBoxUp) {
        return QStyle::visualRect(option->direction, frame, logicalUp);
    }
    if (subControl == QStyle::SC_SpinBoxDown) {
        return QStyle::visualRect(option->direction, frame, logicalDown);
    }
    return {};
}

QStyle::SubControl ZzFluentStylePrivate::hitTestSpinBox(
    const QStyleOptionSpinBox *option,
    const QPoint &position) const
{
    if (option == nullptr || !option->rect.contains(position)) {
        return QStyle::SC_None;
    }
    for (const QStyle::SubControl subControl : {
             QStyle::SC_SpinBoxUp,
             QStyle::SC_SpinBoxDown,
             QStyle::SC_SpinBoxEditField}) {
        if (spinBoxSubControlRect(option, subControl).contains(position)) {
            return subControl;
        }
    }
    return QStyle::SC_SpinBoxFrame;
}

void ZzFluentStylePrivate::drawTabBarTab(
    const QStyleOptionTab *option,
    QPainter *painter,
    const QWidget *widget) const
{
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool selected = option->state.testFlag(QStyle::State_Selected);
    const bool hovered = option->state.testFlag(QStyle::State_MouseOver);
    QColor fill = Qt::transparent;
    if (!enabled) {
        fill = snapshot->color(ZzColorToken::ControlFillDisabled);
    } else if (selected) {
        fill = option->palette.color(QPalette::Base);
    } else if (hovered) {
        fill = snapshot->color(ZzColorToken::ControlFillHover);
    }
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRect(option->rect);
    if (selected) {
        QRect indicator = option->rect;
        indicator.setTop(indicator.bottom() - 2);
        painter->fillRect(
            indicator,
            option->palette.color(QPalette::Highlight));
    }
    painter->restore();
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_TabBarTabLabel,
        option,
        painter,
        widget);
}

void ZzFluentStylePrivate::drawToolTipPanel(
    const QStyleOption *option,
    QPainter *painter) const
{
    const qreal strokeWidth = snapshot->metric(
        ZzMetricToken::StrokeThin);
    const qreal radius = snapshot->metric(
        ZzMetricToken::CornerRadiusSmall);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(
        snapshot->color(ZzColorToken::ControlStroke),
        strokeWidth));
    painter->setBrush(snapshot->color(ZzColorToken::SurfaceSecondary));
    painter->drawRoundedRect(
        QRectF(option->rect).adjusted(
            strokeWidth / 2.0,
            strokeWidth / 2.0,
            -strokeWidth / 2.0,
            -strokeWidth / 2.0),
        radius,
        radius);
    painter->restore();
}

void ZzFluentStylePrivate::drawMenuPanel(
    const QStyleOption *option,
    QPainter *painter) const
{
    const qreal strokeWidth = snapshot->metric(
        ZzMetricToken::StrokeThin);
    const qreal radius = snapshot->metric(
        ZzMetricToken::CornerRadiusMedium);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(
        snapshot->color(ZzColorToken::ControlStroke),
        strokeWidth));
    painter->setBrush(snapshot->color(ZzColorToken::SurfaceSecondary));
    painter->drawRoundedRect(
        QRectF(option->rect).adjusted(
            strokeWidth / 2.0,
            strokeWidth / 2.0,
            -strokeWidth / 2.0,
            -strokeWidth / 2.0),
        radius,
        radius);
    painter->restore();
}

void ZzFluentStylePrivate::drawMenuEmptyArea(
    const QStyleOption *option,
    QPainter *painter) const
{
    painter->fillRect(
        option->rect,
        snapshot->color(ZzColorToken::SurfaceSecondary));
}

void ZzFluentStylePrivate::drawMenuBarPanel(
    const QStyleOption *option,
    QPainter *painter) const
{
    painter->fillRect(
        option->rect,
        snapshot->color(ZzColorToken::Surface));
}

void ZzFluentStylePrivate::drawMenuBarEmptyArea(
    const QStyleOption *option,
    QPainter *painter) const
{
    drawMenuBarPanel(option, painter);
}

void ZzFluentStylePrivate::drawMenuBarItem(
    const QStyleOptionMenuItem *option,
    QPainter *painter,
    const QWidget *widget) const
{
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool pressed = option->state.testFlag(QStyle::State_Sunken);
    const bool hovered = option->state.testFlag(QStyle::State_Selected)
        || option->state.testFlag(QStyle::State_MouseOver);
    if (enabled && (pressed || hovered)) {
        const QRectF surface = QRectF(option->rect).adjusted(
            2.0,
            2.0,
            -2.0,
            -2.0);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(
            pressed
                ? ZzColorToken::ControlFillPressed
                : ZzColorToken::ControlFillHover));
        painter->drawRoundedRect(
            surface,
            snapshot->metric(ZzMetricToken::CornerRadiusSmall),
            snapshot->metric(ZzMetricToken::CornerRadiusSmall));
        painter->restore();
    }

    QStyleOptionMenuItem adjusted = *option;
    adjusted.state.setFlag(QStyle::State_Sunken, false);
    adjusted.state.setFlag(QStyle::State_Selected, false);
    adjusted.state.setFlag(QStyle::State_MouseOver, false);
    adjusted.palette.setColor(
        QPalette::All,
        QPalette::Button,
        Qt::transparent);
    adjusted.palette.setColor(
        QPalette::All,
        QPalette::Window,
        Qt::transparent);
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_MenuBarItem,
        &adjusted,
        painter,
        widget);
}

void ZzFluentStylePrivate::drawProgressBar(
    const QStyleOptionProgressBar *option,
    QPainter *painter,
    const QWidget *widget) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF groove = QRectF(option->rect).adjusted(
        0.5,
        0.5,
        -0.5,
        -0.5);
    painter->setPen(Qt::NoPen);
    painter->setBrush(option->palette.color(QPalette::Mid));
    painter->drawRoundedRect(groove, 2.0, 2.0);

    QRectF chunk = groove;
    const bool horizontal = option->state.testFlag(
        QStyle::State_Horizontal);
    const bool indeterminate = option->minimum == 0
        && option->maximum == 0;
    if (indeterminate) {
        if (horizontal) {
            chunk.setWidth(groove.width() / 3.0);
            chunk.moveCenter(groove.center());
        } else {
            chunk.setHeight(groove.height() / 3.0);
            chunk.moveCenter(groove.center());
        }
    } else {
        const qint64 span = qint64(option->maximum)
            - qint64(option->minimum);
        const qreal ratio = span > 0
            ? std::clamp(
                  qreal(qint64(option->progress)
                        - qint64(option->minimum))
                      / qreal(span),
                  qreal(0.0),
                  qreal(1.0))
            : qreal(0.0);
        if (horizontal) {
            chunk.setWidth(groove.width() * ratio);
            const bool fromRight = option->invertedAppearance
                != (option->direction == Qt::RightToLeft);
            if (fromRight) {
                chunk.moveRight(groove.right());
            }
        } else {
            chunk.setHeight(groove.height() * ratio);
            if (!option->invertedAppearance) {
                chunk.moveBottom(groove.bottom());
            }
        }
    }
    if (!chunk.isEmpty()) {
        painter->setBrush(option->palette.color(QPalette::Highlight));
        painter->drawRoundedRect(chunk, 2.0, 2.0);
    }
    painter->restore();

    if (option->textVisible) {
        q_ptr->QProxyStyle::drawControl(
            QStyle::CE_ProgressBarLabel,
            option,
            painter,
            widget);
    }
}

void ZzFluentStylePrivate::drawSlider(
    const QStyleOptionSlider *option,
    QPainter *painter,
    const QWidget *widget) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QRectF groove = q_ptr->subControlRect(
        QStyle::CC_Slider,
        option,
        QStyle::SC_SliderGroove,
        widget);
    const QRect handle = q_ptr->subControlRect(
        QStyle::CC_Slider,
        option,
        QStyle::SC_SliderHandle,
        widget);
    if (option->orientation == Qt::Horizontal) {
        groove.setHeight(4.0);
        groove.moveCenter(QRectF(option->rect).center());
    } else {
        groove.setWidth(4.0);
        groove.moveCenter(QRectF(option->rect).center());
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(option->palette.color(QPalette::Mid));
    painter->drawRoundedRect(groove, 2.0, 2.0);
    QRectF active = groove;
    if (option->orientation == Qt::Horizontal) {
        if (option->upsideDown) {
            active.setLeft(handle.center().x());
        } else {
            active.setRight(handle.center().x());
        }
    } else if (option->upsideDown) {
        active.setTop(handle.center().y());
    } else {
        active.setBottom(handle.center().y());
    }
    painter->setBrush(option->palette.color(QPalette::Highlight));
    painter->drawRoundedRect(active, 2.0, 2.0);
    painter->drawEllipse(QRectF(handle));
    if (option->state.testFlag(QStyle::State_HasFocus)) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(
            option->palette.color(QPalette::Highlight),
            2.0));
        painter->drawEllipse(
            QRectF(handle).adjusted(-2.0, -2.0, 2.0, 2.0));
    }
    painter->restore();
}

void ZzFluentStylePrivate::drawScrollBar(
    const QStyleOptionSlider *option,
    QPainter *painter,
    const QWidget *widget) const
{
    if (!option->subControls.testFlag(QStyle::SC_ScrollBarSlider)) {
        return;
    }
    const QRect slider = scrollBarSubControlRect(
        option,
        QStyle::SC_ScrollBarSlider);
    if (slider.isEmpty()) {
        return;
    }

    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool hovered = option->state.testFlag(QStyle::State_MouseOver);
    const bool pressed = option->state.testFlag(QStyle::State_Sunken)
        && option->activeSubControls.testFlag(QStyle::SC_ScrollBarSlider);
    const bool focused = option->state.testFlag(QStyle::State_HasFocus);
    qreal expansion = hovered || pressed || focused ? 1.0 : 0.0;
    const auto *fluentScrollBar = qobject_cast<const ZzScrollBar *>(widget);
    if (fluentScrollBar != nullptr) {
        expansion = std::max(
            expansion,
            std::clamp(fluentScrollBar->d_ptr->expansion, 0.0, 1.0));
    }
    if (!enabled) {
        expansion = 0.0;
    }

    const QPalette::ColorGroup group = enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    if (!qFuzzyIsNull(expansion)) {
        painter->setBrush(snapshot->color(ZzColorToken::ControlFillHover));
        const QRectF track = QRectF(option->rect).adjusted(
            0.5,
            0.5,
            -0.5,
            -0.5);
        const qreal trackRadius = std::min(
            track.width(),
            track.height())
            / 2.0;
        painter->drawRoundedRect(track, trackRadius, trackRadius);
    }

    constexpr qreal compactThickness = 3.0;
    constexpr qreal expandedThickness = 6.0;
    const qreal thickness = compactThickness
        + ((expandedThickness - compactThickness) * expansion);
    QRectF handle(slider);
    if (option->orientation == Qt::Horizontal) {
        handle.setHeight(thickness);
        handle.moveCenter(QRectF(slider).center());
    } else {
        handle.setWidth(thickness);
        handle.moveCenter(QRectF(slider).center());
    }
    const QColor handleColor = focused || pressed
        ? option->palette.color(group, QPalette::Highlight)
        : option->palette.color(group, QPalette::Text);
    painter->setBrush(handleColor);
    painter->drawRoundedRect(handle, thickness / 2.0, thickness / 2.0);
    painter->restore();
}

QRect ZzFluentStylePrivate::scrollBarSubControlRect(
    const QStyleOptionSlider *option,
    QStyle::SubControl subControl) const
{
    if (option == nullptr || option->rect.isEmpty()) {
        return {};
    }
    if (subControl == QStyle::SC_ScrollBarGroove) {
        return option->rect;
    }
    if (subControl == QStyle::SC_ScrollBarAddLine
        || subControl == QStyle::SC_ScrollBarSubLine
        || subControl == QStyle::SC_ScrollBarFirst
        || subControl == QStyle::SC_ScrollBarLast) {
        return {};
    }

    const bool horizontal = option->orientation == Qt::Horizontal;
    const int available = horizontal
        ? option->rect.width()
        : option->rect.height();
    if (available <= 0) {
        return {};
    }
    const qint64 range = std::max<qint64>(
        0,
        static_cast<qint64>(option->maximum)
            - static_cast<qint64>(option->minimum));
    const qint64 pageStep = std::max<qint64>(0, option->pageStep);
    const int minimumLength = std::min(
        available,
        q_ptr->pixelMetric(QStyle::PM_ScrollBarSliderMin, option));
    int sliderLength = available;
    if (range > 0) {
        const qint64 denominator = range + pageStep;
        const int proportional = pageStep > 0 && denominator > 0
            ? qRound(
                  static_cast<qreal>(pageStep)
                  / static_cast<qreal>(denominator)
                  * static_cast<qreal>(available))
            : minimumLength;
        sliderLength = std::clamp(
            proportional,
            minimumLength,
            available);
    }
    const int travel = available - sliderLength;
    const int boundedPosition = std::clamp(
        option->sliderPosition,
        option->minimum,
        option->maximum);
    const int sliderOffset = range > 0
        ? QStyle::sliderPositionFromValue(
              option->minimum,
              option->maximum,
              boundedPosition,
              travel,
              option->upsideDown)
        : 0;
    const QRect slider = horizontal
        ? QRect(
              option->rect.left() + sliderOffset,
              option->rect.top(),
              sliderLength,
              option->rect.height())
        : QRect(
              option->rect.left(),
              option->rect.top() + sliderOffset,
              option->rect.width(),
              sliderLength);
    if (subControl == QStyle::SC_ScrollBarSlider) {
        return slider;
    }

    const QRect before = horizontal
        ? QRect(
              option->rect.left(),
              option->rect.top(),
              slider.left() - option->rect.left(),
              option->rect.height())
        : QRect(
              option->rect.left(),
              option->rect.top(),
              option->rect.width(),
              slider.top() - option->rect.top());
    const QRect after = horizontal
        ? QRect(
              slider.right() + 1,
              option->rect.top(),
              option->rect.right() - slider.right(),
              option->rect.height())
        : QRect(
              option->rect.left(),
              slider.bottom() + 1,
              option->rect.width(),
              option->rect.bottom() - slider.bottom());
    if (subControl == QStyle::SC_ScrollBarSubPage) {
        return option->upsideDown ? after : before;
    }
    if (subControl == QStyle::SC_ScrollBarAddPage) {
        return option->upsideDown ? before : after;
    }
    return {};
}

QStyle::SubControl ZzFluentStylePrivate::hitTestScrollBar(
    const QStyleOptionSlider *option,
    const QPoint &position) const
{
    if (option == nullptr || !option->rect.contains(position)) {
        return QStyle::SC_None;
    }
    for (const QStyle::SubControl subControl : {
             QStyle::SC_ScrollBarSlider,
             QStyle::SC_ScrollBarSubPage,
             QStyle::SC_ScrollBarAddPage}) {
        if (scrollBarSubControlRect(option, subControl).contains(position)) {
            return subControl;
        }
    }
    return QStyle::SC_None;
}

void ZzFluentStylePrivate::drawMenuItem(
    const QStyleOptionMenuItem *option,
    QPainter *painter,
    const QWidget *widget) const
{
    QStyleOptionMenuItem adjusted = *option;
    const bool enabled = adjusted.state.testFlag(QStyle::State_Enabled);
    const bool pressed = adjusted.state.testFlag(QStyle::State_Sunken);
    const bool hovered = adjusted.state.testFlag(QStyle::State_Selected)
        || adjusted.state.testFlag(QStyle::State_MouseOver);
    if (adjusted.menuItemType == QStyleOptionMenuItem::Separator) {
        if (adjusted.text.isEmpty()) {
            const QRect lineBounds = adjusted.rect.adjusted(8, 0, -8, 0);
            painter->save();
            painter->setPen(QPen(
                snapshot->color(ZzColorToken::ControlStroke),
                snapshot->metric(ZzMetricToken::StrokeThin)));
            painter->drawLine(
                QPointF(lineBounds.left(), lineBounds.center().y()),
                QPointF(lineBounds.right(), lineBounds.center().y()));
            painter->restore();
            return;
        }
        adjusted.state.setFlag(QStyle::State_Selected, false);
        adjusted.state.setFlag(QStyle::State_MouseOver, false);
        adjusted.state.setFlag(QStyle::State_Sunken, false);
        q_ptr->QProxyStyle::drawControl(
            QStyle::CE_MenuItem,
            &adjusted,
            painter,
            widget);
        return;
    }

    if (enabled && (pressed || hovered)) {
        const QRectF surface = QRectF(adjusted.rect).adjusted(
            4.0,
            2.0,
            -4.0,
            -2.0);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(
            pressed
                ? ZzColorToken::ControlFillPressed
                : ZzColorToken::ControlFillHover));
        painter->drawRoundedRect(
            surface,
            snapshot->metric(ZzMetricToken::CornerRadiusSmall),
            snapshot->metric(ZzMetricToken::CornerRadiusSmall));
        painter->restore();
    }

    const QRect originalRect = adjusted.rect;
    const bool submenu = adjusted.menuItemType
        == QStyleOptionMenuItem::SubMenu;
    const bool customCheck = adjusted.checked
        && adjusted.checkType != QStyleOptionMenuItem::NotCheckable
        && adjusted.icon.isNull();
    adjusted.state.setFlag(QStyle::State_Selected, false);
    adjusted.state.setFlag(QStyle::State_MouseOver, false);
    adjusted.state.setFlag(QStyle::State_Sunken, false);
    if (customCheck) {
        adjusted.checked = false;
    }
    if (submenu) {
        const int trailingWidth = qMin(
            zzMenuTrailingIndicatorWidth,
            adjusted.rect.width());
        const QRect logicalContent = adjusted.rect.adjusted(
            0,
            0,
            -trailingWidth,
            0);
        adjusted.rect = QStyle::visualRect(
            adjusted.direction,
            originalRect,
            logicalContent);
        adjusted.menuItemType = QStyleOptionMenuItem::Normal;
    }
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_MenuItem,
        &adjusted,
        painter,
        widget);

    const QPalette::ColorGroup group = enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    const QColor markColor = customCheck && enabled
        ? adjusted.palette.color(group, QPalette::Highlight)
        : adjusted.palette.color(group, QPalette::Text);
    if (customCheck) {
        const QRect logicalIndicator(
            originalRect.left() + 8,
            originalRect.center().y() - 8,
            16,
            16);
        const QRect indicator = QStyle::visualRect(
            adjusted.direction,
            originalRect,
            logicalIndicator);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(markColor, 2.0));
        painter->setBrush(Qt::NoBrush);
        if (option->checkType == QStyleOptionMenuItem::Exclusive) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(markColor);
            painter->drawEllipse(QPointF(indicator.center()), 4.0, 4.0);
        } else {
            QPainterPath check;
            check.moveTo(
                indicator.left() + 2.0,
                indicator.center().y());
            check.lineTo(
                indicator.center().x() - 1.0,
                indicator.bottom() - 3.0);
            check.lineTo(
                indicator.right() - 1.0,
                indicator.top() + 2.0);
            painter->drawPath(check);
        }
        painter->restore();
    }

    if (submenu) {
        const QRect logicalArrow(
            originalRect.right() - 22,
            originalRect.center().y() - 6,
            14,
            12);
        const QRect arrowRect = QStyle::visualRect(
            adjusted.direction,
            originalRect,
            logicalArrow);
        const qreal direction = adjusted.direction == Qt::RightToLeft
            ? -1.0
            : 1.0;
        const QPointF center = arrowRect.center();
        QPainterPath chevron;
        chevron.moveTo(
            center.x() - (direction * 2.5),
            center.y() - 4.0);
        chevron.lineTo(
            center.x() + (direction * 1.5),
            center.y());
        chevron.lineTo(
            center.x() - (direction * 2.5),
            center.y() + 4.0);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(
            adjusted.palette.color(group, QPalette::Text),
            1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(chevron);
        painter->restore();
    }
}

void ZzFluentStylePrivate::applySnapshot(ZzThemeChangeKinds changes)
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    Q_ASSERT(controller != nullptr);
    if (controller == nullptr) {
        return;
    }

    snapshot = controller->snapshot();
    const bool colorsChanged = changes.testFlag(
        ZzThemeChangeKind::Colors);
    const bool geometryChanged = changes.testFlag(
        ZzThemeChangeKind::Geometry);

    if (colorsChanged) {
        cache.rebuildVisuals(*snapshot);
        cache.clearIcons();
        iconRevision = snapshot->revision();
        QApplication::setPalette(q_ptr->standardPalette());
    }
    if (!geometryChanged) {
        return;
    }

    const auto widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        QEvent event(QEvent::StyleChange);
        QCoreApplication::sendEvent(widget, &event);
        widget->updateGeometry();
        widget->update();
    }
}

} // namespace ZzFluentUI
