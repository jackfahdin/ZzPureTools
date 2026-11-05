#include "ZzAudioSpectrumPrivate.h"
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {
ZzAudioSpectrumPrivate::ZzAudioSpectrumPrivate(ZzAudioSpectrum *owner)
    : q(owner), theme(owner)
{
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(refreshInterval);
    QObject::connect(&timer, &QTimer::timeout, q, [this] { tick(); });
    reconfigure();
}
ZzAudioSpectrumPrivate::ZzPublished ZzAudioSpectrumPrivate::before() const
{
    return {target, displayed, peaks, timer.isActive()};
}
void ZzAudioSpectrumPrivate::publish(const ZzPublished &old, bool configuration, bool appearance)
{
    const auto current = before();
    const quint64 version = ++revision;
    QPointer<ZzAudioSpectrum> guard(q);
    if (configuration || appearance || old.displayed != current.displayed || old.peaks != current.peaks)
        q->update();
    // Every emission may synchronously delete the widget or replace its state.
    // Never continue an older publication after such a callback.
    if (configuration) {
        Q_EMIT q->configurationChanged();
        if (!guard || guard->d_ptr->revision != version) return;
    }
    if (appearance) {
        Q_EMIT q->appearanceChanged();
        if (!guard || guard->d_ptr->revision != version) return;
    }
    if (old.levels != current.levels) {
        Q_EMIT q->levelsChanged(current.levels);
        if (!guard || guard->d_ptr->revision != version) return;
    }
    if (old.displayed != current.displayed) {
        Q_EMIT q->displayedLevelsChanged(current.displayed);
        if (!guard || guard->d_ptr->revision != version) return;
    }
    if (old.peaks != current.peaks) {
        Q_EMIT q->peakLevelsChanged(current.peaks);
        if (!guard || guard->d_ptr->revision != version) return;
    }
    if (old.running != current.running) Q_EMIT q->runningChanged(current.running);
}
void ZzAudioSpectrumPrivate::reset()
{
    input.clear();
    timer.stop();
    lastInput = lastTick = {};
    target.fill(0, settings.bars);
    displayed = peaks = velocity = peakVelocity = target;
    peakTimes.fill({}, settings.bars);
}
void ZzAudioSpectrumPrivate::reconfigure()
{
    analyzer.configure(settings);
    input.configure(settings.fftSize, settings.channels, inputMode == ZzAudioSpectrum::Pcm, inputTimeout);
    reset();
}
bool ZzAudioSpectrumPrivate::animated() const
{
    return animationEnabled && !theme.snapshot()->reducedMotion()
        && q->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, q);
}
void ZzAudioSpectrumPrivate::synchronize()
{
    if (!presented || !q->isVisible() || !q->isEnabled()) {
        timer.stop();
        lastTick = {};
        return;
    }
    if (!timer.isActive()) {
        lastTick = Clock::now();
        timer.start(refreshInterval);
    }
}
void ZzAudioSpectrumPrivate::wake()
{
    const auto old = before();
    // Wakes never analyze input. Hidden/disabled buffers wait for a visible tick.
    synchronize();
    publish(old);
}
void ZzAudioSpectrumPrivate::snapToTarget()
{
    displayed = peaks = target;
    velocity.fill(0);
    peakVelocity.fill(0);
    peakTimes.fill(Clock::now());
}
void ZzAudioSpectrumPrivate::tick()
{
    const auto old = before();
    const auto now = Clock::now();
    const qreal dt = lastTick == Clock::time_point{} ? 0
        : std::chrono::duration<qreal>(now - lastTick).count();
    lastTick = now;
    const auto expired = [this, now](Clock::time_point time) {
        return time != Clock::time_point{} && inputTimeout > 0
            && std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count() >= inputTimeout;
    };
    if (inputMode == ZzAudioSpectrum::Pcm) {
        const auto latest = input.takeLatest();
        if (latest.serial != serial) {
            serial = latest.serial;
            if (latest.samples.isEmpty()) target.fill(0);
            if (latest.timestamp != Clock::time_point{} && !expired(latest.timestamp)) {
                lastInput = latest.timestamp;
                if (!latest.samples.isEmpty()) target = analyzer.analyze(latest.samples);
            }
        }
    }
    if (expired(lastInput)) {
        target.fill(0);
        lastInput = {};
    }
    if (!animated()) {
        snapToTarget();
    } else {
        const qreal attack = -std::expm1(-dt / 0.030);
        for (int i = 0; i < settings.bars; ++i) {
            if (target[i] >= displayed[i]) {
                displayed[i] += (target[i] - displayed[i]) * attack;
                velocity[i] = 0;
            } else {
                displayed[i] = std::max(target[i], displayed[i] - velocity[i] * dt - gravity * dt * dt / 2);
                velocity[i] += gravity * dt;
            }
            if (std::abs(displayed[i] - target[i]) < 0.0001) displayed[i] = target[i];
            if (displayed[i] >= peaks[i]) {
                peaks[i] = displayed[i]; peakTimes[i] = now; peakVelocity[i] = 0;
            } else {
                const auto holdEnd = peakTimes[i] + std::chrono::milliseconds(peakHoldDuration);
                const qreal fallTime = std::clamp(std::chrono::duration<qreal>(now - holdEnd).count(), 0.0, dt);
                peaks[i] = std::max(displayed[i], peaks[i] - peakVelocity[i] * fallTime
                    - gravity * 0.25 * fallTime * fallTime);
                peakVelocity[i] += gravity * 0.5 * fallTime;
            }
        }
    }
    const bool waiting = lastInput != Clock::time_point{} && inputTimeout > 0;
    const bool moving = displayed != target || peaks != displayed;
    if (!waiting && !moving) timer.stop();
    publish(old);
}
void ZzAudioSpectrumPrivate::paint()
{
    const auto snapshot = theme.snapshot();
    const bool contrast = snapshot->mode() == ZzThemeMode::HighContrast;
    const QColor background = contrast || !backgroundColor.isValid()
        ? snapshot->color(ZzColorToken::Surface) : backgroundColor;
    const QColor accent = contrast ? snapshot->color(ZzColorToken::TextPrimary)
        : accentColor.isValid() ? accentColor : snapshot->color(ZzColorToken::Accent);
    QPainter painter(q);
    painter.fillRect(q->rect(), background);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const QRectF bounds = QRectF(q->contentsRect()).adjusted(2, 2, -2, -2);
    if (bounds.width() <= 0 || bounds.height() <= 0) return;
    const qreal bottom = bounds.top() + bounds.height() * (reflectionEnabled ? 0.64 : 1);
    const qreal height = bottom - bounds.top();
    const int columns = std::min(settings.bars, std::max(1, int(bounds.width() / (barGap + 1))));
    const qreal pitch = bounds.width() / columns;
    const qreal width = std::max(0.5, pitch - std::min(barGap, pitch - 0.5));
    for (int column = 0; column < columns; ++column) {
        qreal level = 0, peak = 0;
        for (int band = column * settings.bars / columns; band < (column + 1) * settings.bars / columns; ++band) {
            level = std::max(level, displayed[band]); peak = std::max(peak, peaks[band]);
        }
        const QColor color = rainbowEnabled && !contrast
            ? QColor::fromHsvF(float(0.72 * (1 - qreal(column) / std::max(1, columns - 1))), 0.8F, 0.95F)
            : accent;
        const qreal x = bounds.left() + column * pitch + (pitch - width) / 2;
        const QRectF bar(x, bottom - height * level, width, height * level);
        const qreal radius = std::min({cornerRadius, width / 2, bar.height() / 2});
        if (level > 0) {
            painter.setBrush(color);
            painter.drawRoundedRect(bar, radius, radius);
            if (reflectionEnabled && reflectionOpacity > 0 && bounds.bottom() > bottom + 2) {
                // Reflect the original path vertically about its baseline, then
                // clip to the reflection area and apply a scene-space fade.
                QPainterPath path; path.addRoundedRect(bar, radius, radius);
                const QTransform mirror(1, 0, 0, -1, 0, 2 * bottom + 2);
                QLinearGradient fade(0, bottom + 2, 0, bounds.bottom());
                QColor near = color; near.setAlphaF(float(reflectionOpacity) * color.alphaF());
                QColor far = color; far.setAlphaF(0);
                fade.setColorAt(0, near); fade.setColorAt(1, far);
                painter.save();
                painter.setClipRect(QRectF(bounds.left(), bottom + 2, bounds.width(), bounds.bottom() - bottom - 2));
                painter.fillPath(mirror.map(path), fade);
                painter.restore();
            }
        }
        if (peakEnabled && peak > 0 && peakHeight > 0) {
            painter.setBrush(color);
            const qreal y = std::max(bounds.top(), bottom - height * peak - peakHeight);
            painter.drawRoundedRect(QRectF(x, y, width, std::min(peakHeight, height)),
                                    std::min(cornerRadius, width / 2), std::min(cornerRadius, peakHeight / 2));
        }
    }
}
} // namespace ZzFluentUI
