#include "ZzAudioLevelMeterPrivate.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {
namespace {

/** @brief 按固定比例混合两种颜色，amount 为 0 返回 a、为 1 返回 b。 */
QColor zzMixMeterColor(const QColor &a, const QColor &b, qreal amount)
{
    const float t = static_cast<float>(qBound(0.0, amount, 1.0));
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

/** @brief 浅色主题下的默认电平表背景色；用户显式设置时忽略。 */
constexpr char zzMeterBackgroundLight[] = "#F4F4F4";
/** @brief 深色主题下的默认电平表背景色；用户显式设置时忽略。 */
constexpr char zzMeterBackgroundDark[] = "#111111";
/** @brief 浅色主题下的默认激活电平颜色；用户显式设置时忽略。 */
constexpr char zzMeterActiveLight[] = "#C85D00";
/** @brief 深色主题下的默认激活电平颜色；用户显式设置时忽略。 */
constexpr char zzMeterActiveDark[] = "#FF9F2D";
/** @brief 浅色主题下的默认告警电平颜色；用户显式设置时忽略。 */
constexpr char zzMeterWarningLight[] = "#A15C00";
/** @brief 深色主题下的默认告警电平颜色；用户显式设置时忽略。 */
constexpr char zzMeterWarningDark[] = "#FFD166";
/** @brief 浅色主题下的默认削波电平颜色；用户显式设置时忽略。 */
constexpr char zzMeterClipLight[] = "#C42B1C";
/** @brief 深色主题下的默认削波电平颜色；用户显式设置时忽略。 */
constexpr char zzMeterClipDark[] = "#FF5A5F";
/** @brief 浅色主题下的默认峰值标记颜色；用户显式设置时忽略。 */
constexpr char zzMeterPeakLight[] = "#202020";
/** @brief 深色主题下的默认峰值标记颜色；用户显式设置时忽略。 */
constexpr char zzMeterPeakDark[] = "#FFFFFF";

} // namespace

ZzAudioLevelMeterPrivate::ZzAudioLevelMeterPrivate(ZzAudioLevelMeter *widget) : q(widget)
{
    timer.setInterval(16);
    timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer, &QTimer::timeout, q, [this] { advance(); });
    resizeStorage();
}

ZzAudioLevelMeterPrivate::ZzMeterSnapshot ZzAudioLevelMeterPrivate::before() const
{
    return {channelCount, levels, peaks, timer.isActive()};
}

void ZzAudioLevelMeterPrivate::publish(const ZzMeterSnapshot &old,
                                      std::vector<std::function<void()>> notifications,
                                      bool forcePeaks)
{
    syncTimer();
    if (old.channels != channelCount) q->updateGeometry();
    q->update();
    const quint64 currentRevision = ++revision;
    // 所有参数在发送第一个信号之前复制；同步删除或重入后立即终止旧事务。
    const QPointer<ZzAudioLevelMeter> guard(q);
    if (old.channels != channelCount) {
        notifications.insert(notifications.begin(), [widget = q, value = channelCount] {
            Q_EMIT widget->channelCountChanged(value);
        });
    }
    if (old.levels != levels) {
        notifications.emplace_back([widget = q, value = levels] { Q_EMIT widget->levelsChanged(value); });
    }
    if (forcePeaks || old.peaks != peaks) {
        notifications.emplace_back([widget = q, value = peaks] { Q_EMIT widget->peakLevelsChanged(value); });
    }
    if (old.running != timer.isActive()) {
        notifications.emplace_back([widget = q, value = timer.isActive()] {
            Q_EMIT widget->runningChanged(value);
        });
    }
    for (const auto &notify : notifications) {
        notify();
        if (!guard || guard->d_ptr->revision != currentRevision) return;
    }
}

qreal ZzAudioLevelMeterPrivate::bound(qreal value) const
{
    return std::isfinite(value) ? qBound(minimumDecibels, value, maximumDecibels) : minimumDecibels;
}

qreal ZzAudioLevelMeterPrivate::ratio(qreal value) const
{
    return (bound(value) - minimumDecibels) / (maximumDecibels - minimumDecibels);
}

void ZzAudioLevelMeterPrivate::resizeStorage()
{
    const qsizetype oldSize = levels.size();
    levels.resize(channelCount);
    displayed.resize(channelCount);
    peaks.resize(channelCount);
    holdRemaining.resize(channelCount);
    for (int i = 0; i < channelCount; ++i) {
        if (i >= oldSize) {
            levels[i] = displayed[i] = peaks[i] = minimumDecibels;
            holdRemaining[i] = 0;
        } else {
            levels[i] = bound(levels[i]);
            displayed[i] = bound(displayed[i]);
            peaks[i] = bound(peaks[i]);
        }
    }
}

bool ZzAudioLevelMeterPrivate::motionEnabled() const
{
    return animationEnabled && q->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, q);
}

void ZzAudioLevelMeterPrivate::syncTimer()
{
    const bool motion = motionEnabled();
    if (!motion) {
        displayed = levels;
        peaks = levels;
        holdRemaining.fill(0);
    }
    bool work = inputTimeout > 0 && inputElapsed.isValid();
    for (int i = 0; i < channelCount; ++i) {
        work = work || (decayRate > 0 && displayed[i] > levels[i])
                    || (peakHoldEnabled && peakDecayRate > 0 && peaks[i] > displayed[i]);
    }
    const bool run = motion && q->isVisible() && q->isEnabled() && work;
    if (run && !timer.isActive()) {
        frameElapsed.start();
        timer.start();
    } else if (!run && timer.isActive()) {
        timer.stop();
    }
}

void ZzAudioLevelMeterPrivate::apply(const QVector<qreal> &values)
{
    if (values.isEmpty()) { q->clear(); return; }
    const ZzMeterSnapshot old = before();
    // 先把旧输入推进到此刻，再提交新峰值；高频输入也不会暂停衰减计时。
    if (timer.isActive()) advanceValues();
    channelCount = static_cast<int>(qMin<qsizetype>(8, values.size()));
    resizeStorage();
    const bool motion = motionEnabled();
    for (int i = 0; i < channelCount; ++i) {
        const qreal value = bound(values[i]);
        levels[i] = value;
        if (!motion) {
            displayed[i] = peaks[i] = value;
            holdRemaining[i] = 0;
        } else {
            displayed[i] = qMax(value, displayed[i]);
            if (!peakHoldEnabled) {
                peaks[i] = displayed[i];
                holdRemaining[i] = 0;
            } else if (value >= peaks[i]) {
                peaks[i] = value;
                holdRemaining[i] = peakHoldDuration;
            }
        }
    }
    inputElapsed.start();
    publish(old);
}

void ZzAudioLevelMeterPrivate::advance()
{
    const ZzMeterSnapshot old = before();
    advanceValues();
    publish(old);
}

void ZzAudioLevelMeterPrivate::advanceValues()
{
    // 使用真实经过时间，GUI 短时繁忙后也能追上真实的 dB/s 衰减。
    const qreal elapsed = static_cast<qreal>(frameElapsed.restart());
    qreal decayMilliseconds = elapsed;
    if (inputTimeout > 0 && inputElapsed.isValid() && inputElapsed.elapsed() >= inputTimeout) {
        // 首次超时帧只衰减真正越过超时边界的时间。
        decayMilliseconds = qMin(elapsed, static_cast<qreal>(inputElapsed.elapsed() - inputTimeout));
        levels.fill(minimumDecibels);
        inputElapsed.invalidate();
    }
    for (int i = 0; i < channelCount; ++i) {
        displayed[i] = qMax(levels[i], displayed[i] - decayRate * decayMilliseconds / 1000.0);
        if (!peakHoldEnabled) {
            peaks[i] = displayed[i];
        } else {
            const qreal decayTime = qMax(0.0, elapsed - holdRemaining[i]);
            holdRemaining[i] = qMax(0.0, holdRemaining[i] - elapsed);
            peaks[i] = qMax(displayed[i], peaks[i] - peakDecayRate * decayTime / 1000.0);
        }
    }
}

QVector<qreal> ZzAudioLevelMeterPrivate::ticks() const
{
    QVector<qreal> values;
    if (scaleMode == ZzAudioLevelMeter::CustomScale) {
        // 配置保留完整值；绘制只采集至多 256 个量程内刻度。
        const auto first = std::lower_bound(customScaleValues.cbegin(), customScaleValues.cend(),
                                            maximumDecibels, std::greater<qreal>());
        for (auto it = first; it != customScaleValues.cend() && *it >= minimumDecibels
                             && values.size() < 256; ++it) values.append(*it);
        if (!values.isEmpty()) return values;
    }
    if (scaleMode == ZzAudioLevelMeter::FixedTickCount) {
        for (int i = 0; i < scaleTickCount; ++i)
            values.append(i == scaleTickCount - 1 ? minimumDecibels
                : maximumDecibels - i * (maximumDecibels - minimumDecibels) / (scaleTickCount - 1));
    } else {
        for (int i = 0; i < 185; ++i) {
            const qreal value = maximumDecibels - i * scaleInterval;
            if (value < minimumDecibels) break;
            values.append(value);
        }
        if (values.isEmpty() || values.last() > minimumDecibels) values.append(minimumDecibels);
    }
    return values;
}

QString ZzAudioLevelMeterPrivate::tickLabel(qreal value) const
{
    if (qAbs(value) < 0.5 * std::pow(10.0, -scalePrecision)) value = 0;
    QString label = QString::number(value, 'f', scalePrecision);
    if (scaleUnitVisible && !scaleUnit.isEmpty()) label += QLatin1Char(' ') + scaleUnit;
    return label;
}

QString ZzAudioLevelMeterPrivate::channelLabel(int channel) const
{
    if (channel < channelLabels.size() && !channelLabels[channel].isEmpty()) return channelLabels[channel];
    return channelCount == 2 ? (channel == 0 ? QStringLiteral("L") : QStringLiteral("R"))
                             : QString::number(channel + 1);
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
                            : QColor::fromString(QLatin1String(dark ? darkValue : light));
    };
    const auto disabled = [&](QColor color) {
        if (!q->isEnabled()) color.setAlphaF(color.alphaF() * 0.45f);
        return color;
    };
    const QColor background = themed(backgroundColor, zzMeterBackgroundLight, zzMeterBackgroundDark, QPalette::Window);
    const QColor active = themed(activeColor, zzMeterActiveLight, zzMeterActiveDark, QPalette::Highlight);
    const QColor warning = themed(warningColor, zzMeterWarningLight, zzMeterWarningDark, QPalette::Text);
    const QColor clip = themed(clipColor, zzMeterClipLight, zzMeterClipDark, QPalette::Text);
    const QColor peak = themed(peakColor, zzMeterPeakLight, zzMeterPeakDark, QPalette::Text);
    QColor inactive = inactiveColor.isValid() ? inactiveColor : palette.color(group, QPalette::Text);
    QColor text = scaleColor.isValid() ? scaleColor : palette.color(group, QPalette::Text);
    if (!inactiveColor.isValid()) inactive.setAlphaF(highContrast ? 0.35f : 0.14f);
    if (!scaleColor.isValid() && !highContrast) text.setAlphaF(0.58f);
    const auto colorFor = [&](qreal value) {
        if (colorMode == ZzAudioLevelMeter::SingleColor) return active;
        if (colorMode == ZzAudioLevelMeter::ThresholdColors)
            return value >= clipDecibels ? clip : (value >= warningDecibels ? warning : active);
        if (value <= warningDecibels)
            return zzMixMeterColor(active, warning, warningDecibels > minimumDecibels
                ? (value - minimumDecibels) / (warningDecibels - minimumDecibels) : 1.0);
        if (value <= clipDecibels)
            return zzMixMeterColor(warning, clip, clipDecibels > warningDecibels
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
    ZzMeterScalePosition position = scalePosition;
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
