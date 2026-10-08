#include "ZzAudioLevelMeterPrivate.h"
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <QFontMetrics>
#include <QPainter>
#include <cmath>

namespace ZzFluentUI {
namespace {
QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    const float t = static_cast<float>(qBound(0.0, amount, 1.0));
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
}

void ZzAudioLevelMeterPrivate::paint()
{
    QPainter painter(q);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette palette = q->palette();
    const auto group = q->isEnabled() ? QPalette::Active : QPalette::Disabled;
    bool dark = palette.color(QPalette::Window).lightness() < 128;
    bool highContrast = false;
    if (const auto *fluent = qobject_cast<const ZzFluentStyle *>(q->style())) {
        const auto snapshot = fluent->themeSnapshot();
        highContrast = snapshot->mode() == ZzThemeMode::HighContrast;
        if (!highContrast) dark = snapshot->mode() == ZzThemeMode::Dark;
    }
    const auto themed = [&](const QColor &custom, const char *light, const char *darkValue,
                            QPalette::ColorRole role) {
        if (custom.isValid()) return custom;
        return highContrast ? palette.color(group, role)
                            : QColor(QLatin1String(dark ? darkValue : light));
    };
    const auto disabled = [&](QColor color) {
        if (!q->isEnabled()) color.setAlphaF(color.alphaF() * 0.45f);
        return color;
    };
    const QColor background = themed(backgroundColor, "#F4F4F4", "#111111", QPalette::Window);
    const QColor active = themed(activeColor, "#C85D00", "#FF9F2D", QPalette::Highlight);
    const QColor warning = themed(warningColor, "#A15C00", "#FFD166", QPalette::Text);
    const QColor clip = themed(clipColor, "#C42B1C", "#FF5A5F", QPalette::Text);
    const QColor peak = themed(peakColor, "#202020", "#FFFFFF", QPalette::Text);
    QColor inactive = inactiveColor.isValid() ? inactiveColor : palette.color(group, QPalette::Text);
    QColor text = scaleColor.isValid() ? scaleColor : palette.color(group, QPalette::Text);
    if (!inactiveColor.isValid()) inactive.setAlphaF(highContrast ? 0.35f : 0.14f);
    if (!scaleColor.isValid() && !highContrast) text.setAlphaF(0.58f);
    const auto colorFor = [&](qreal value) {
        if (colorMode == ZzAudioLevelMeter::SingleColor) return active;
        if (colorMode == ZzAudioLevelMeter::ThresholdColors)
            return value >= clipDecibels ? clip : (value >= warningDecibels ? warning : active);
        if (value <= warningDecibels)
            return mix(active, warning, warningDecibels > minimumDecibels
                ? (value - minimumDecibels) / (warningDecibels - minimumDecibels) : 1.0);
        if (value <= clipDecibels)
            return mix(warning, clip, clipDecibels > warningDecibels
                ? (value - warningDecibels) / (clipDecibels - warningDecibels) : 1.0);
        return clip;
    };
    painter.fillRect(q->rect(), background);
    const QFontMetrics metrics(q->font());
    const auto scaleValues = ticks();
    const qreal labelHeight = channelLabelsVisible ? metrics.height() + 6.0 : 0.0;
    qreal textWidth = 0;
    for (qreal value : scaleValues) textWidth = qMax(textWidth, qreal(metrics.horizontalAdvance(tickLabel(value))));
    const qreal tickSpace = scaleTickMarksVisible ? scaleTickLength + 4 : 0;
    const qreal scaleWidth = scalePosition == ZzAudioLevelMeter::NoScale ? 0 : textWidth + tickSpace + 4;
    const qreal edge = scalePosition == ZzAudioLevelMeter::NoScale ? 10 : qMax(10.0, metrics.height() * 0.5 + 1);
    const QRectF content = QRectF(q->rect()).adjusted(10, edge, -10, -edge - labelHeight);
    if (content.width() <= 0 || content.height() < 4) return;
    ScalePosition position = scalePosition;
    if (position == ZzAudioLevelMeter::CenterScale && channelCount != 2) position = ZzAudioLevelMeter::RightScale;
    QRectF scale;
    QRectF meterArea = content;
    QVector<QRectF> channels;
    if (position == ZzAudioLevelMeter::LeftScale) {
        scale = QRectF(content.left(), content.top(), scaleWidth, content.height());
        meterArea.setLeft(scale.right() + channelSpacing);
    } else if (position == ZzAudioLevelMeter::RightScale) {
        scale = QRectF(content.right() - scaleWidth, content.top(), scaleWidth, content.height());
        meterArea.setRight(scale.left() - channelSpacing);
    }
    if (position == ZzAudioLevelMeter::CenterScale) {
        const qreal channelWidth = (content.width() - scaleWidth - 2 * channelSpacing) / 2;
        if (channelWidth <= 0) return;
        channels.append(QRectF(content.left(), content.top(), channelWidth, content.height()));
        scale = QRectF(content.left() + channelWidth + channelSpacing, content.top(), scaleWidth, content.height());
        channels.append(QRectF(scale.right() + channelSpacing, content.top(), channelWidth, content.height()));
    } else {
        const qreal channelWidth = (meterArea.width() - channelSpacing * (channelCount - 1)) / channelCount;
        if (channelWidth <= 0) return;
        for (int i = 0; i < channelCount; ++i)
            channels.append(QRectF(meterArea.left() + i * (channelWidth + channelSpacing),
                                   meterArea.top(), channelWidth, meterArea.height()));
    }
    painter.setPen(Qt::NoPen);
    for (int channel = 0; channel < channelCount; ++channel) {
        const QRectF bar = channels[channel];
        const int count = qMin(segmentCount, qMax(2, static_cast<int>(std::floor(bar.height() / 2))));
        const qreal spacing = qMin(segmentSpacing, bar.height() * 0.45 / (count - 1));
        const qreal height = (bar.height() - spacing * (count - 1)) / count;
        for (int segment = 0; segment < count; ++segment) {
            const qreal fraction = qreal(segment + 1) / count;
            const qreal value = minimumDecibels + fraction * (maximumDecibels - minimumDecibels);
            const QRectF part(bar.left(), bar.bottom() - (segment + 1) * height - segment * spacing,
                              bar.width(), height);
            painter.setBrush(disabled(fraction <= ratio(displayed[channel]) ? colorFor(value) : inactive));
            painter.drawRoundedRect(part, segmentRadius, segmentRadius);
        }
        if (peakHoldEnabled && peaks[channel] > minimumDecibels) {
            const qreal y = bar.bottom() - ratio(peaks[channel]) * bar.height();
            const qreal marker = qBound(1.0, height, 2.0);
            painter.setBrush(disabled(peak));
            painter.drawRoundedRect(QRectF(bar.left(), y - marker / 2, bar.width(), marker), marker / 2, marker / 2);
        }
        if (channelLabelsVisible) {
            painter.setPen(disabled(text));
            painter.drawText(QRectF(bar.left(), content.bottom() + 4, bar.width(), labelHeight),
                             Qt::AlignHCenter | Qt::AlignTop,
                             metrics.elidedText(channelLabel(channel), Qt::ElideRight, static_cast<int>(bar.width())));
            painter.setPen(Qt::NoPen);
        }
    }
    if (scale.isEmpty()) return;
    painter.setPen(disabled(text));
    const bool right = position != ZzAudioLevelMeter::LeftScale;
    qreal previousBottom = -1e9;
    for (qsizetype i = 0; i < scaleValues.size(); ++i) {
        const qreal y = content.bottom() - ratio(scaleValues[i]) * content.height();
        const qreal top = y - metrics.height() * 0.5;
        // 优先保留两端刻度；靠近底部端点的中间标签跳过，避免相互重叠。
        const qreal lastY = content.bottom() - ratio(scaleValues.last()) * content.height();
        if (top < previousBottom + 2) continue;
        if (i > 0 && i + 1 < scaleValues.size() && lastY - y < metrics.height() + 2) continue;
        QRectF label(scale.left(), top, scale.width(), metrics.height());
        if (scaleTickMarksVisible) {
            if (right) {
                painter.drawLine(QPointF(scale.left(), y), QPointF(scale.left() + scaleTickLength, y));
                label.setLeft(label.left() + tickSpace);
            } else {
                painter.drawLine(QPointF(scale.right() - scaleTickLength, y), QPointF(scale.right(), y));
                label.setRight(label.right() - tickSpace);
            }
        }
        const Qt::Alignment alignment = (right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter;
        painter.drawText(label, static_cast<int>(alignment.toInt()),
                         tickLabel(scaleValues[i]));
        previousBottom = top + metrics.height();
    }
}
} // namespace ZzFluentUI
