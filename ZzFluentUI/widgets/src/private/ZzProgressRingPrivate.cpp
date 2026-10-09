#include "ZzProgressRingPrivate.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>

#include <ZzFluentUI/ZzProgressRing.h>

namespace ZzFluentUI {

ZzProgressRingPrivate::ZzProgressRingPrivate(ZzProgressRing *q)
    : q_ptr(q)
    , animation(new QVariantAnimation(q))
{
    Q_ASSERT(q_ptr != nullptr);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(indeterminateDuration);
    animation->setLoopCount(-1);
    animation->setEasingCurve(QEasingCurve::Linear);
    QObject::connect(
        animation,
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this](const QVariant &value) {
            phase = std::clamp(value.toReal(), 0.0, 1.0);
            q_ptr->update();
        });
}

ZzProgressRingPrivate::~ZzProgressRingPrivate()
{
    QObject::disconnect(centerDestroyedConnection);
    animation->stop();
    QObject::disconnect(animation, nullptr, q_ptr, nullptr);
}

void ZzProgressRingPrivate::syncAnimation()
{
    const bool shouldAnimate = q_ptr->isVisible()
        && q_ptr->isEnabled()
        && isIndeterminate()
        && q_ptr->style() != nullptr
        && q_ptr->style()->styleHint(
               QStyle::SH_Widget_Animate,
               nullptr,
               q_ptr)
            != 0;
    if (!shouldAnimate) {
        stopAnimation();
        return;
    }
    if (animation->state() != QAbstractAnimation::Running) {
        animation->start();
    }
}

void ZzProgressRingPrivate::stopAnimation() noexcept
{
    animation->stop();
    if (!qFuzzyIsNull(phase)) {
        phase = 0.0;
        q_ptr->update();
    }
}

bool ZzProgressRingPrivate::isIndeterminate() const noexcept
{
    return q_ptr->minimum() == 0 && q_ptr->maximum() == 0;
}

void ZzProgressRingPrivate::setIndeterminateDuration(int milliseconds)
{
    const qreal previousPhase = phase;
    indeterminateDuration = milliseconds;
    animation->setDuration(milliseconds);
    if (animation->state() != QAbstractAnimation::Stopped) {
        // 归一化相位映射到新周期；不重启或在循环边界跳回零度。
        animation->setCurrentTime(std::min(milliseconds - 1, qRound(previousPhase * milliseconds)));
    }
}

QFont ZzProgressRingPrivate::valueFont() const
{
    if (customValueFont != QFont()) {
        return customValueFont;
    }
    QFont result = q_ptr->font();
    if (!q_ptr->testAttribute(Qt::WA_SetFont)) {
        constexpr qreal valueExtentRatio = 0.18;
        constexpr int minimumValuePixels = 12;
        constexpr int maximumValuePixels = 32;
        const QRect content = q_ptr->contentsRect();
        result.setPixelSize(std::clamp(
            qRound(std::min(content.width(), content.height()) * valueExtentRatio),
            minimumValuePixels, maximumValuePixels));
        result.setWeight(QFont::DemiBold);
    }
    return result;
}

QFont ZzProgressRingPrivate::titleFont() const
{
    if (customTitleFont != QFont()) {
        return customTitleFont;
    }
    QFont result = q_ptr->font();
    if (!q_ptr->testAttribute(Qt::WA_SetFont)) {
        result.setPixelSize(std::clamp(qRound(
            std::min(q_ptr->width(), q_ptr->height()) * 0.09), 9, 14));
    }
    return result;
}

void ZzProgressRingPrivate::drawText(QPainter &painter, const QRectF &contentRect) const
{
    if (contentRect.isEmpty()) return;
    const QString value = q_ptr->text();
    const bool hasTitle = !title.isEmpty();
    const bool hasValue = !value.isEmpty();
    if (!hasTitle && !hasValue) return;

    const QFont titleFace = titleFont();
    const QFont valueFace = valueFont();
    const QFontMetricsF titleMetrics(titleFace);
    const QFontMetricsF valueMetrics(valueFace);
    const qreal titleHeight = hasTitle ? titleMetrics.height() : 0;
    const qreal valueHeight = hasValue ? valueMetrics.height() : 0;
    const qreal spacing = hasTitle && hasValue ? textSpacing : 0;
    const qreal totalHeight = titleHeight + valueHeight + spacing;
    if (totalHeight > contentRect.height()) return;
    qreal top = contentRect.center().y() - totalHeight / 2;
    const auto group = !q_ptr->isEnabled() ? QPalette::Disabled
        : q_ptr->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    const auto resolveColor = [&](QColor color, bool secondary) {
        if (!color.isValid()) {
            color = q_ptr->palette().color(group, QPalette::Text);
            if (secondary) color.setAlphaF(color.alphaF() * 0.72F);
        } else if (!q_ptr->isEnabled()) {
            const QColor disabled = q_ptr->palette().color(QPalette::Disabled, QPalette::Text);
            color.setRed((color.red() + disabled.red()) / 2);
            color.setGreen((color.green() + disabled.green()) / 2);
            color.setBlue((color.blue() + disabled.blue()) / 2);
        }
        return color;
    };
    // Fit each line into the circle, including its farthest vertical edge.
    // Keep the established value-only safe square for long Qt format strings.
    const auto drawLine = [&](const QString &text, const QFont &font,
                              const QFontMetricsF &metrics, QColor color, qreal height) {
        const qreal radius = contentRect.width() / 2;
        const qreal farEdge = std::max(std::abs(top - contentRect.center().y()),
            std::abs(top + height - contentRect.center().y()));
        qreal width = 2 * std::sqrt(std::max(0.0, radius * radius - farEdge * farEdge));
        if (!hasTitle) width = std::min(width, (radius + 1) * std::numbers::sqrt2_v<qreal>);
        if (width <= 0) return;
        const QRectF line(contentRect.center().x() - width / 2, top, width, height);
        painter.setFont(font);
        painter.setPen(color);
        painter.drawText(line, Qt::AlignCenter | Qt::TextSingleLine,
            metrics.elidedText(text, Qt::ElideRight, width));
    };
    painter.save();
    painter.setClipRect(contentRect, Qt::IntersectClip);
    painter.setRenderHint(QPainter::TextAntialiasing);
    if (hasTitle) {
        drawLine(title, titleFace, titleMetrics, resolveColor(titleColor, true), titleHeight);
        top += titleHeight + spacing;
    }
    if (hasValue) drawLine(value, valueFace, valueMetrics, resolveColor(valueColor, false), valueHeight);
    painter.restore();
}

} // namespace ZzFluentUI
