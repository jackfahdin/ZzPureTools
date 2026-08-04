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
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconCacheKey.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

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
    const QColor fill = enabled
        ? option->palette.color(group, QPalette::Base)
        : snapshot->color(ZzColorToken::ControlFillDisabled);
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
    const QPointF center = QRectF(arrowRect).center();
    const qreal halfWidth = qMin(4.0, arrowRect.width() / 4.0);
    const qreal halfHeight = qMin(2.5, arrowRect.height() / 6.0);
    QPainterPath arrow;
    arrow.moveTo(center.x() - halfWidth, center.y() - halfHeight);
    arrow.lineTo(center.x(), center.y() + halfHeight);
    arrow.lineTo(center.x() + halfWidth, center.y() - halfHeight);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(
        option->palette.color(QPalette::Text),
        1.5,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter->drawPath(arrow);
    painter->restore();
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

void ZzFluentStylePrivate::drawMenuItem(
    const QStyleOptionMenuItem *option,
    QPainter *painter,
    const QWidget *widget) const
{
    QStyleOptionMenuItem adjusted = *option;
    if (adjusted.state.testFlag(QStyle::State_Selected)) {
        painter->fillRect(
            adjusted.rect,
            adjusted.palette.color(QPalette::Highlight));
        adjusted.palette.setColor(
            QPalette::Text,
            adjusted.palette.color(QPalette::HighlightedText));
        adjusted.state.setFlag(QStyle::State_Selected, false);
    }
    q_ptr->QProxyStyle::drawControl(
        QStyle::CE_MenuItem,
        &adjusted,
        painter,
        widget);
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
