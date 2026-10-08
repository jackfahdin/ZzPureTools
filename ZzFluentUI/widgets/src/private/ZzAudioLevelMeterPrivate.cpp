#include "ZzAudioLevelMeterPrivate.h"

#include <QPointer>
#include <QStyle>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {
ZzAudioLevelMeterPrivate::ZzAudioLevelMeterPrivate(ZzAudioLevelMeter *widget) : q(widget)
{
    timer.setInterval(16);
    timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer, &QTimer::timeout, q, [this] { advance(); });
    resizeStorage();
}

ZzAudioLevelMeterPrivate::Before ZzAudioLevelMeterPrivate::before() const
{
    return {channelCount, levels, peaks, timer.isActive()};
}

void ZzAudioLevelMeterPrivate::publish(const Before &old,
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
        if (!guard || guard->d_->revision != currentRevision) return;
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
    const Before old = before();
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
    const Before old = before();
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
} // namespace ZzFluentUI
