#include <ZzFluentUI/ZzAudioSpectrum.h>
#include "private/ZzAudioSpectrumPrivate.h"
#include <QEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {
ZzAudioSpectrum::ZzAudioSpectrum(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<ZzAudioSpectrumPrivate>(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
ZzAudioSpectrum::~ZzAudioSpectrum() = default;

ZzAudioSpectrum::InputMode ZzAudioSpectrum::inputMode() const
{
    return d_ptr->inputMode;
}
void ZzAudioSpectrum::setInputMode(InputMode value)
{
    if (value != Pcm && value != ExternalBands) return;
    if (inputMode() == value) return;
    const auto old = d_ptr->before();
    d_ptr->inputMode = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

ZzAudioSpectrum::ChannelMix ZzAudioSpectrum::channelMix() const
{
    return d_ptr->settings.powerAverage ? PowerAverage : WaveformAverage;
}
void ZzAudioSpectrum::setChannelMix(ChannelMix value)
{
    if (value != PowerAverage && value != WaveformAverage) return;
    if (channelMix() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.powerAverage = value == PowerAverage;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

int ZzAudioSpectrum::sampleRate() const
{
    return d_ptr->settings.sampleRate;
}
void ZzAudioSpectrum::setSampleRate(int value)
{
    value = std::clamp(value, 8000, 192000);
    if (sampleRate() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.sampleRate = value;
    d_ptr->settings.maximumFrequency = std::min(d_ptr->settings.maximumFrequency, qreal(value) / 2);
    d_ptr->settings.minimumFrequency = std::min(d_ptr->settings.minimumFrequency, d_ptr->settings.maximumFrequency - 1);
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

int ZzAudioSpectrum::channelCount() const
{
    return d_ptr->settings.channels;
}
void ZzAudioSpectrum::setChannelCount(int value)
{
    value = std::clamp(value, 1, 8);
    if (channelCount() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.channels = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

int ZzAudioSpectrum::fftSize() const
{
    return d_ptr->settings.fftSize;
}
void ZzAudioSpectrum::setFftSize(int value)
{
    if (value != 1024 && value != 2048 && value != 4096 && value != 8192) return;
    if (fftSize() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.fftSize = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

int ZzAudioSpectrum::barCount() const
{
    return d_ptr->settings.bars;
}
void ZzAudioSpectrum::setBarCount(int value)
{
    value = std::clamp(value, 8, 256);
    if (barCount() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.bars = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

qreal ZzAudioSpectrum::minimumFrequency() const
{
    return d_ptr->settings.minimumFrequency;
}
void ZzAudioSpectrum::setMinimumFrequency(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 1.0, d_ptr->settings.maximumFrequency - 1);
    if (minimumFrequency() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.minimumFrequency = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

qreal ZzAudioSpectrum::maximumFrequency() const
{
    return d_ptr->settings.maximumFrequency;
}
void ZzAudioSpectrum::setMaximumFrequency(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, d_ptr->settings.minimumFrequency + 1, qreal(d_ptr->settings.sampleRate) / 2);
    if (maximumFrequency() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.maximumFrequency = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

qreal ZzAudioSpectrum::minimumDecibels() const
{
    return d_ptr->settings.minimumDecibels;
}
void ZzAudioSpectrum::setMinimumDecibels(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, -160.0, d_ptr->settings.maximumDecibels - 0.1);
    if (minimumDecibels() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.minimumDecibels = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

qreal ZzAudioSpectrum::maximumDecibels() const
{
    return d_ptr->settings.maximumDecibels;
}
void ZzAudioSpectrum::setMaximumDecibels(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, d_ptr->settings.minimumDecibels + 0.1, 24.0);
    if (maximumDecibels() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.maximumDecibels = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

qreal ZzAudioSpectrum::spectralTilt() const
{
    return d_ptr->settings.tilt;
}
void ZzAudioSpectrum::setSpectralTilt(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, -6.0, 9.0);
    if (spectralTilt() == value) return;
    const auto old = d_ptr->before();
    d_ptr->settings.tilt = value;
    d_ptr->reconfigure();
    d_ptr->publish(old, true, false);
}

int ZzAudioSpectrum::refreshInterval() const
{
    return d_ptr->refreshInterval;
}
void ZzAudioSpectrum::setRefreshInterval(int value)
{
    value = std::clamp(value, 1, 1000);
    if (refreshInterval() == value) return;
    const auto old = d_ptr->before();
    d_ptr->refreshInterval = value;
    d_ptr->timer.setInterval(value);
    if (d_ptr->lastInput != ZzAudioSpectrumPrivate::Clock::time_point{} || d_ptr->displayed != d_ptr->target || d_ptr->peaks != d_ptr->displayed)
        d_ptr->synchronize();
    d_ptr->publish(old, true, false);
}

bool ZzAudioSpectrum::animationEnabled() const
{
    return d_ptr->animationEnabled;
}
void ZzAudioSpectrum::setAnimationEnabled(bool value)
{
    if (animationEnabled() == value) return;
    const auto old = d_ptr->before();
    d_ptr->animationEnabled = value;
    if (!value) d_ptr->snapToTarget();
    if (d_ptr->lastInput != ZzAudioSpectrumPrivate::Clock::time_point{} || d_ptr->displayed != d_ptr->target || d_ptr->peaks != d_ptr->displayed)
        d_ptr->synchronize();
    d_ptr->publish(old, true, false);
}

int ZzAudioSpectrum::inputTimeout() const
{
    return d_ptr->inputTimeout;
}
void ZzAudioSpectrum::setInputTimeout(int value)
{
    value = std::clamp(value, 0, 60000);
    if (inputTimeout() == value) return;
    const auto old = d_ptr->before();
    d_ptr->inputTimeout = value;
    d_ptr->input.setInputTimeout(value);
    if (d_ptr->lastInput != ZzAudioSpectrumPrivate::Clock::time_point{} || d_ptr->displayed != d_ptr->target || d_ptr->peaks != d_ptr->displayed)
        d_ptr->synchronize();
    d_ptr->publish(old, true, false);
}

qreal ZzAudioSpectrum::gravity() const
{
    return d_ptr->gravity;
}
void ZzAudioSpectrum::setGravity(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.01, 100.0);
    if (gravity() == value) return;
    const auto old = d_ptr->before();
    d_ptr->gravity = value;
    if (d_ptr->lastInput != ZzAudioSpectrumPrivate::Clock::time_point{} || d_ptr->displayed != d_ptr->target || d_ptr->peaks != d_ptr->displayed)
        d_ptr->synchronize();
    d_ptr->publish(old, true, false);
}

int ZzAudioSpectrum::peakHoldDuration() const
{
    return d_ptr->peakHoldDuration;
}
void ZzAudioSpectrum::setPeakHoldDuration(int value)
{
    value = std::clamp(value, 0, 60000);
    if (peakHoldDuration() == value) return;
    const auto old = d_ptr->before();
    d_ptr->peakHoldDuration = value;
    if (d_ptr->lastInput != ZzAudioSpectrumPrivate::Clock::time_point{} || d_ptr->displayed != d_ptr->target || d_ptr->peaks != d_ptr->displayed)
        d_ptr->synchronize();
    d_ptr->publish(old, true, false);
}

qreal ZzAudioSpectrum::barGap() const
{
    return d_ptr->barGap;
}
void ZzAudioSpectrum::setBarGap(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 64.0);
    if (barGap() == value) return;
    const auto old = d_ptr->before();
    d_ptr->barGap = value;
    d_ptr->publish(old, false, true);
}

qreal ZzAudioSpectrum::cornerRadius() const
{
    return d_ptr->cornerRadius;
}
void ZzAudioSpectrum::setCornerRadius(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 64.0);
    if (cornerRadius() == value) return;
    const auto old = d_ptr->before();
    d_ptr->cornerRadius = value;
    d_ptr->publish(old, false, true);
}

bool ZzAudioSpectrum::reflectionEnabled() const
{
    return d_ptr->reflectionEnabled;
}
void ZzAudioSpectrum::setReflectionEnabled(bool value)
{
    if (reflectionEnabled() == value) return;
    const auto old = d_ptr->before();
    d_ptr->reflectionEnabled = value;
    d_ptr->publish(old, false, true);
}

qreal ZzAudioSpectrum::reflectionOpacity() const
{
    return d_ptr->reflectionOpacity;
}
void ZzAudioSpectrum::setReflectionOpacity(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 1.0);
    if (reflectionOpacity() == value) return;
    const auto old = d_ptr->before();
    d_ptr->reflectionOpacity = value;
    d_ptr->publish(old, false, true);
}

bool ZzAudioSpectrum::peakEnabled() const
{
    return d_ptr->peakEnabled;
}
void ZzAudioSpectrum::setPeakEnabled(bool value)
{
    if (peakEnabled() == value) return;
    const auto old = d_ptr->before();
    d_ptr->peakEnabled = value;
    d_ptr->publish(old, false, true);
}

qreal ZzAudioSpectrum::peakHeight() const
{
    return d_ptr->peakHeight;
}
void ZzAudioSpectrum::setPeakHeight(qreal value)
{
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 64.0);
    if (peakHeight() == value) return;
    const auto old = d_ptr->before();
    d_ptr->peakHeight = value;
    d_ptr->publish(old, false, true);
}

bool ZzAudioSpectrum::rainbowEnabled() const
{
    return d_ptr->rainbowEnabled;
}
void ZzAudioSpectrum::setRainbowEnabled(bool value)
{
    if (rainbowEnabled() == value) return;
    const auto old = d_ptr->before();
    d_ptr->rainbowEnabled = value;
    d_ptr->publish(old, false, true);
}

QColor ZzAudioSpectrum::accentColor() const
{
    return d_ptr->accentColor;
}
void ZzAudioSpectrum::setAccentColor(QColor value)
{
    if (accentColor() == value) return;
    const auto old = d_ptr->before();
    d_ptr->accentColor = value;
    d_ptr->publish(old, false, true);
}

QColor ZzAudioSpectrum::backgroundColor() const
{
    return d_ptr->backgroundColor;
}
void ZzAudioSpectrum::setBackgroundColor(QColor value)
{
    if (backgroundColor() == value) return;
    const auto old = d_ptr->before();
    d_ptr->backgroundColor = value;
    d_ptr->publish(old, false, true);
}

bool ZzAudioSpectrum::isRunning() const { return d_ptr->timer.isActive(); }
QVector<qreal> ZzAudioSpectrum::levels() const { return d_ptr->target; }
QVector<qreal> ZzAudioSpectrum::displayedLevels() const { return d_ptr->displayed; }
QVector<qreal> ZzAudioSpectrum::peakLevels() const { return d_ptr->peaks; }
QVector<qreal> ZzAudioSpectrum::centerFrequencies() const { return d_ptr->analyzer.centerFrequencies(); }
QSize ZzAudioSpectrum::sizeHint() const { return {360, 200}; }
QSize ZzAudioSpectrum::minimumSizeHint() const { return {32, 24}; }
void ZzAudioSpectrum::pushPcm16Le(const QByteArray &bytes)
{
    if (d_ptr->input.pushPcm(bytes))
        QMetaObject::invokeMethod(this, [this] {
            d_ptr->input.acknowledgeWake();
            d_ptr->wake();
        }, Qt::QueuedConnection);
}
void ZzAudioSpectrum::pushSamples(const QVector<float> &samples)
{
    if (d_ptr->input.pushFloats(samples))
        QMetaObject::invokeMethod(this, [this] {
            d_ptr->input.acknowledgeWake();
            d_ptr->wake();
        }, Qt::QueuedConnection);
}
void ZzAudioSpectrum::setBandLevels(const QVector<qreal> &levels)
{
    if (d_ptr->inputMode != ExternalBands || levels.size() != d_ptr->settings.bars) return;
    if (std::any_of(levels.cbegin(), levels.cend(), [](qreal v) { return !std::isfinite(v); })) return;
    const auto old = d_ptr->before();
    const bool firstFrame = !d_ptr->timer.isActive()
        || d_ptr->lastInput == ZzAudioSpectrumPrivate::Clock::time_point{};
    d_ptr->target = levels;
    for (qreal &level : d_ptr->target) level = std::clamp(level, 0.0, 1.0);
    d_ptr->lastInput = ZzAudioSpectrumPrivate::Clock::now();
    if (!d_ptr->animated() && firstFrame) {
        d_ptr->snapToTarget();
        // A show-event timer may already be running before the first input.
        // Start its presentation interval at this immediate first frame.
        if (d_ptr->timer.isActive()) {
            d_ptr->lastTick = d_ptr->lastInput;
            d_ptr->timer.start(d_ptr->refreshInterval);
        }
    }
    d_ptr->synchronize();
    d_ptr->publish(old);
}
void ZzAudioSpectrum::clear()
{
    const auto old = d_ptr->before();
    d_ptr->reset();
    d_ptr->publish(old);
}
void ZzAudioSpectrum::paintEvent(QPaintEvent *) { d_ptr->paint(); }
void ZzAudioSpectrum::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    d_ptr->presented = true;
    d_ptr->wake();
}
void ZzAudioSpectrum::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    d_ptr->presented = false;
    d_ptr->wake();
}
void ZzAudioSpectrum::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (!d_ptr) return;
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::EnabledChange || event->type() == QEvent::ApplicationPaletteChange) {
        d_ptr->theme.refreshFallback();
        update();
        const auto old = d_ptr->before();
        if (!d_ptr->animated()) d_ptr->snapToTarget();
        d_ptr->synchronize();
        d_ptr->publish(old);
    }
}
} // namespace ZzFluentUI
