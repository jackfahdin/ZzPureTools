#include <ZzFluentUI/ZzAudioLevelMeter.h>
#include "private/ZzAudioLevelMeterPrivate.h"
#include <QEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ZzFluentUI {
ZzAudioLevelMeter::ZzAudioLevelMeter(QWidget *parent)
    : QWidget(parent), d_(std::make_unique<ZzAudioLevelMeterPrivate>(this))
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}
ZzAudioLevelMeter::~ZzAudioLevelMeter() = default;

int ZzAudioLevelMeter::channelCount() const { return d_->channelCount; }
qreal ZzAudioLevelMeter::minimumDecibels() const { return d_->minimumDecibels; }
qreal ZzAudioLevelMeter::maximumDecibels() const { return d_->maximumDecibels; }
qreal ZzAudioLevelMeter::warningDecibels() const { return d_->warningDecibels; }
qreal ZzAudioLevelMeter::clipDecibels() const { return d_->clipDecibels; }
int ZzAudioLevelMeter::segmentCount() const { return d_->segmentCount; }
qreal ZzAudioLevelMeter::segmentSpacing() const { return d_->segmentSpacing; }
qreal ZzAudioLevelMeter::segmentRadius() const { return d_->segmentRadius; }
qreal ZzAudioLevelMeter::channelSpacing() const { return d_->channelSpacing; }
ZzAudioLevelMeter::ScalePosition ZzAudioLevelMeter::scalePosition() const { return d_->scalePosition; }
ZzAudioLevelMeter::ScaleMode ZzAudioLevelMeter::scaleMode() const { return d_->scaleMode; }
qreal ZzAudioLevelMeter::scaleInterval() const { return d_->scaleInterval; }
int ZzAudioLevelMeter::scaleTickCount() const { return d_->scaleTickCount; }
QString ZzAudioLevelMeter::scaleUnit() const { return d_->scaleUnit; }
bool ZzAudioLevelMeter::isScaleUnitVisible() const { return d_->scaleUnitVisible; }
int ZzAudioLevelMeter::scalePrecision() const { return d_->scalePrecision; }
bool ZzAudioLevelMeter::areScaleTickMarksVisible() const { return d_->scaleTickMarksVisible; }
qreal ZzAudioLevelMeter::scaleTickLength() const { return d_->scaleTickLength; }
bool ZzAudioLevelMeter::areChannelLabelsVisible() const { return d_->channelLabelsVisible; }
bool ZzAudioLevelMeter::isPeakHoldEnabled() const { return d_->peakHoldEnabled; }
int ZzAudioLevelMeter::peakHoldDuration() const { return d_->peakHoldDuration; }
qreal ZzAudioLevelMeter::decayRate() const { return d_->decayRate; }
qreal ZzAudioLevelMeter::peakDecayRate() const { return d_->peakDecayRate; }
int ZzAudioLevelMeter::inputTimeout() const { return d_->inputTimeout; }
bool ZzAudioLevelMeter::isAnimationEnabled() const { return d_->animationEnabled; }
ZzAudioLevelMeter::ColorMode ZzAudioLevelMeter::colorMode() const { return d_->colorMode; }
QColor ZzAudioLevelMeter::backgroundColor() const { return d_->backgroundColor; }
QColor ZzAudioLevelMeter::activeColor() const { return d_->activeColor; }
QColor ZzAudioLevelMeter::inactiveColor() const { return d_->inactiveColor; }
QColor ZzAudioLevelMeter::warningColor() const { return d_->warningColor; }
QColor ZzAudioLevelMeter::clipColor() const { return d_->clipColor; }
QColor ZzAudioLevelMeter::peakColor() const { return d_->peakColor; }
QColor ZzAudioLevelMeter::scaleColor() const { return d_->scaleColor; }

QStringList ZzAudioLevelMeter::channelLabels() const { return d_->channelLabels; }
QVector<qreal> ZzAudioLevelMeter::customScaleValues() const { return d_->customScaleValues; }
QVector<qreal> ZzAudioLevelMeter::levels() const { return d_->levels; }
QVector<qreal> ZzAudioLevelMeter::displayedLevels() const { return d_->displayed; }
QVector<qreal> ZzAudioLevelMeter::peakLevels() const { return d_->peaks; }
qreal ZzAudioLevelMeter::level(int channel) const { return d_->levels.value(channel, d_->minimumDecibels); }
qreal ZzAudioLevelMeter::displayedLevel(int channel) const { return d_->displayed.value(channel, d_->minimumDecibels); }
qreal ZzAudioLevelMeter::peakLevel(int channel) const { return d_->peaks.value(channel, d_->minimumDecibels); }
bool ZzAudioLevelMeter::isRunning() const { return d_->timer.isActive(); }
QSize ZzAudioLevelMeter::sizeHint() const { return QSize(d_->channelCount == 1 ? 100 : 70 + d_->channelCount * 42, 320); }
QSize ZzAudioLevelMeter::minimumSizeHint() const { return QSize(d_->channelCount == 1 ? 64 : 48 + d_->channelCount * 24, 140); }

void ZzAudioLevelMeter::setMinimumDecibels(qreal value)
{
    if (std::isfinite(value)) setRange(qBound(-160.0, value, d_->maximumDecibels - 1), d_->maximumDecibels);
}
void ZzAudioLevelMeter::setMaximumDecibels(qreal value)
{
    if (std::isfinite(value)) setRange(d_->minimumDecibels, qBound(d_->minimumDecibels + 1, value, 24.0));
}
void ZzAudioLevelMeter::setRange(qreal minimum, qreal maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum) return;
    minimum = qBound(-160.0, minimum, 23.0);
    maximum = qBound(minimum + 1, maximum, 24.0);
    if (minimum == d_->minimumDecibels && maximum == d_->maximumDecibels) return;
    const auto old = d_->before();
    std::vector<std::function<void()>> notifications;
    if (minimum != d_->minimumDecibels)
        notifications.emplace_back([this, minimum] { Q_EMIT minimumDecibelsChanged(minimum); });
    if (maximum != d_->maximumDecibels)
        notifications.emplace_back([this, maximum] { Q_EMIT maximumDecibelsChanged(maximum); });
    d_->minimumDecibels = minimum;
    d_->maximumDecibels = maximum;
    const qreal warning = qBound(minimum, d_->warningDecibels, maximum);
    const qreal clip = qBound(warning, d_->clipDecibels, maximum);
    if (warning != d_->warningDecibels)
        notifications.emplace_back([this, warning] { Q_EMIT warningDecibelsChanged(warning); });
    if (clip != d_->clipDecibels)
        notifications.emplace_back([this, clip] { Q_EMIT clipDecibelsChanged(clip); });
    d_->warningDecibels = warning;
    d_->clipDecibels = clip;
    d_->resizeStorage();
    updateGeometry();
    d_->publish(old, std::move(notifications));
}

void ZzAudioLevelMeter::setChannelCount(int value)
{
    value = qBound(1, value, 8);
    if (d_->channelCount == value) return;
    const auto old = d_->before();
    d_->channelCount = value;
    d_->resizeStorage();
    d_->publish(old);
}

void ZzAudioLevelMeter::setWarningDecibels(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(d_->minimumDecibels, value, d_->clipDecibels);
    if (d_->warningDecibels == value) return;
    const auto old = d_->before();
    d_->warningDecibels = value;
    d_->publish(old, {[this, value] { Q_EMIT warningDecibelsChanged(value); }});
}

void ZzAudioLevelMeter::setClipDecibels(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(d_->warningDecibels, value, d_->maximumDecibels);
    if (d_->clipDecibels == value) return;
    const auto old = d_->before();
    d_->clipDecibels = value;
    d_->publish(old, {[this, value] { Q_EMIT clipDecibelsChanged(value); }});
}

void ZzAudioLevelMeter::setSegmentCount(int value)
{
    value = qBound(2, value, 120);
    if (d_->segmentCount == value) return;
    const auto old = d_->before();
    d_->segmentCount = value;
    d_->publish(old, {[this, value] { Q_EMIT segmentCountChanged(value); }});
}

void ZzAudioLevelMeter::setSegmentSpacing(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 20.0);
    if (d_->segmentSpacing == value) return;
    const auto old = d_->before();
    d_->segmentSpacing = value;
    d_->publish(old, {[this, value] { Q_EMIT segmentSpacingChanged(value); }});
}

void ZzAudioLevelMeter::setSegmentRadius(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 20.0);
    if (d_->segmentRadius == value) return;
    const auto old = d_->before();
    d_->segmentRadius = value;
    d_->publish(old, {[this, value] { Q_EMIT segmentRadiusChanged(value); }});
}

void ZzAudioLevelMeter::setChannelSpacing(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 40.0);
    if (d_->channelSpacing == value) return;
    const auto old = d_->before();
    d_->channelSpacing = value;
    d_->publish(old, {[this, value] { Q_EMIT channelSpacingChanged(value); }});
}

void ZzAudioLevelMeter::setScalePosition(ScalePosition value)
{
    if (value < NoScale || value > CenterScale) return;
    if (d_->scalePosition == value) return;
    const auto old = d_->before();
    d_->scalePosition = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scalePositionChanged(value); }});
}

void ZzAudioLevelMeter::setScaleMode(ScaleMode value)
{
    if (value < IntervalScale || value > CustomScale) return;
    if (d_->scaleMode == value) return;
    const auto old = d_->before();
    d_->scaleMode = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scaleModeChanged(value); }});
}

void ZzAudioLevelMeter::setScaleInterval(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(1.0, value, 60.0);
    if (d_->scaleInterval == value) return;
    const auto old = d_->before();
    d_->scaleInterval = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scaleIntervalChanged(value); }});
}

void ZzAudioLevelMeter::setScaleTickCount(int value)
{
    value = qBound(2, value, 64);
    if (d_->scaleTickCount == value) return;
    const auto old = d_->before();
    d_->scaleTickCount = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scaleTickCountChanged(value); }});
}

void ZzAudioLevelMeter::setScaleUnit(QString value)
{
    if (d_->scaleUnit == value) return;
    const auto old = d_->before();
    d_->scaleUnit = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scaleUnitChanged(value); }});
}

void ZzAudioLevelMeter::setScaleUnitVisible(bool value)
{
    if (d_->scaleUnitVisible == value) return;
    const auto old = d_->before();
    d_->scaleUnitVisible = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scaleUnitVisibleChanged(value); }});
}

void ZzAudioLevelMeter::setScalePrecision(int value)
{
    value = qBound(0, value, 3);
    if (d_->scalePrecision == value) return;
    const auto old = d_->before();
    d_->scalePrecision = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scalePrecisionChanged(value); }});
}

void ZzAudioLevelMeter::setScaleTickMarksVisible(bool value)
{
    if (d_->scaleTickMarksVisible == value) return;
    const auto old = d_->before();
    d_->scaleTickMarksVisible = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scaleTickMarksVisibleChanged(value); }});
}

void ZzAudioLevelMeter::setScaleTickLength(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(1.0, value, 20.0);
    if (d_->scaleTickLength == value) return;
    const auto old = d_->before();
    d_->scaleTickLength = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT scaleTickLengthChanged(value); }});
}

void ZzAudioLevelMeter::setChannelLabelsVisible(bool value)
{
    if (d_->channelLabelsVisible == value) return;
    const auto old = d_->before();
    d_->channelLabelsVisible = value;
    updateGeometry();
    d_->publish(old, {[this, value] { Q_EMIT channelLabelsVisibleChanged(value); }});
}

void ZzAudioLevelMeter::setPeakHoldEnabled(bool value)
{
    if (d_->peakHoldEnabled == value) return;
    const auto old = d_->before();
    d_->peakHoldEnabled = value;
    if (!value) d_->peaks = d_->displayed;
    d_->holdRemaining.fill(value ? d_->peakHoldDuration : 0);
    d_->publish(old, {[this, value] { Q_EMIT peakHoldEnabledChanged(value); }});
}

void ZzAudioLevelMeter::setPeakHoldDuration(int value)
{
    value = qBound(0, value, 10000);
    if (d_->peakHoldDuration == value) return;
    const auto old = d_->before();
    d_->peakHoldDuration = value;
    for (qreal &remaining : d_->holdRemaining) remaining = qMin(remaining, qreal(value));
    d_->publish(old, {[this, value] { Q_EMIT peakHoldDurationChanged(value); }});
}

void ZzAudioLevelMeter::setDecayRate(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 1000.0);
    if (d_->decayRate == value) return;
    const auto old = d_->before();
    d_->decayRate = value;
    d_->publish(old, {[this, value] { Q_EMIT decayRateChanged(value); }});
}

void ZzAudioLevelMeter::setPeakDecayRate(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 1000.0);
    if (d_->peakDecayRate == value) return;
    const auto old = d_->before();
    d_->peakDecayRate = value;
    d_->publish(old, {[this, value] { Q_EMIT peakDecayRateChanged(value); }});
}

void ZzAudioLevelMeter::setInputTimeout(int value)
{
    value = qBound(0, value, 10000);
    if (d_->inputTimeout == value) return;
    const auto old = d_->before();
    d_->inputTimeout = value;
    d_->publish(old, {[this, value] { Q_EMIT inputTimeoutChanged(value); }});
}

void ZzAudioLevelMeter::setAnimationEnabled(bool value)
{
    if (d_->animationEnabled == value) return;
    const auto old = d_->before();
    d_->animationEnabled = value;
    d_->holdRemaining.fill(value ? d_->peakHoldDuration : 0);
    d_->publish(old, {[this, value] { Q_EMIT animationEnabledChanged(value); }});
}

void ZzAudioLevelMeter::setColorMode(ColorMode value)
{
    if (value < SingleColor || value > GradientColors) return;
    if (d_->colorMode == value) return;
    const auto old = d_->before();
    d_->colorMode = value;
    d_->publish(old, {[this, value] { Q_EMIT colorModeChanged(value); }});
}

void ZzAudioLevelMeter::setBackgroundColor(QColor value)
{
    if (d_->backgroundColor == value) return;
    const auto old = d_->before();
    d_->backgroundColor = value;
    d_->publish(old, {[this, value] { Q_EMIT backgroundColorChanged(value); }});
}

void ZzAudioLevelMeter::setActiveColor(QColor value)
{
    if (d_->activeColor == value) return;
    const auto old = d_->before();
    d_->activeColor = value;
    d_->publish(old, {[this, value] { Q_EMIT activeColorChanged(value); }});
}

void ZzAudioLevelMeter::setInactiveColor(QColor value)
{
    if (d_->inactiveColor == value) return;
    const auto old = d_->before();
    d_->inactiveColor = value;
    d_->publish(old, {[this, value] { Q_EMIT inactiveColorChanged(value); }});
}

void ZzAudioLevelMeter::setWarningColor(QColor value)
{
    if (d_->warningColor == value) return;
    const auto old = d_->before();
    d_->warningColor = value;
    d_->publish(old, {[this, value] { Q_EMIT warningColorChanged(value); }});
}

void ZzAudioLevelMeter::setClipColor(QColor value)
{
    if (d_->clipColor == value) return;
    const auto old = d_->before();
    d_->clipColor = value;
    d_->publish(old, {[this, value] { Q_EMIT clipColorChanged(value); }});
}

void ZzAudioLevelMeter::setPeakColor(QColor value)
{
    if (d_->peakColor == value) return;
    const auto old = d_->before();
    d_->peakColor = value;
    d_->publish(old, {[this, value] { Q_EMIT peakColorChanged(value); }});
}

void ZzAudioLevelMeter::setScaleColor(QColor value)
{
    if (d_->scaleColor == value) return;
    const auto old = d_->before();
    d_->scaleColor = value;
    d_->publish(old, {[this, value] { Q_EMIT scaleColorChanged(value); }});
}

void ZzAudioLevelMeter::setChannelLabels(const QStringList &labels)
{
    if (d_->channelLabels == labels) return;
    const auto old = d_->before();
    d_->channelLabels = labels;
    d_->publish(old, {[this, value = labels] { Q_EMIT channelLabelsChanged(value); }});
}
void ZzAudioLevelMeter::setCustomScaleValues(const QVector<qreal> &values)
{
    QVector<qreal> normalized;
    normalized.reserve(values.size());
    for (qreal value : values) if (std::isfinite(value)) normalized.append(value);
    std::sort(normalized.begin(), normalized.end(), std::greater<qreal>());
    normalized.erase(std::unique(normalized.begin(), normalized.end(),
        [](qreal a, qreal b) { return qAbs(a - b) < 0.0001; }), normalized.end());
    if (d_->customScaleValues == normalized) return;
    const auto old = d_->before();
    d_->customScaleValues = normalized;
    updateGeometry();
    d_->publish(old, {[this, normalized] { Q_EMIT customScaleValuesChanged(normalized); }});
}
void ZzAudioLevelMeter::setLevel(qreal value) { setLevels({value}); }
void ZzAudioLevelMeter::setStereoLevels(qreal left, qreal right) { setLevels({left, right}); }
void ZzAudioLevelMeter::setLevels(const QVector<qreal> &values)
{
    if (QThread::currentThread() != thread()) {
        // 从一开始就限制复制范围，避免排队闭包持有无限大的隐式共享输入。
        QVector<qreal> copy;
        const qsizetype count = qMin<qsizetype>(8, values.size());
        copy.reserve(count);
        for (qsizetype i = 0; i < count; ++i) copy.append(values[i]);
        QMetaObject::invokeMethod(this, [this, copy] { d_->apply(copy); }, Qt::QueuedConnection);
        return;
    }
    d_->apply(values);
}
void ZzAudioLevelMeter::setLinearLevel(qreal amplitude) { setLinearLevels({amplitude}); }
void ZzAudioLevelMeter::setLinearLevels(const QVector<qreal> &amplitudes)
{
    QVector<qreal> values;
    const qsizetype count = qMin<qsizetype>(8, amplitudes.size());
    values.reserve(count);
    for (qsizetype i = 0; i < count; ++i) {
        const qreal magnitude = std::abs(amplitudes[i]);
        values.append(std::isfinite(magnitude) && magnitude > 0
            ? 20 * std::log10(magnitude) : -std::numeric_limits<qreal>::infinity());
    }
    setLevels(values);
}
void ZzAudioLevelMeter::resetPeaks()
{
    const auto old = d_->before();
    d_->peaks = d_->displayed;
    d_->holdRemaining.fill(d_->peakHoldDuration);
    d_->publish(old, {}, true);
}
void ZzAudioLevelMeter::clear()
{
    const auto old = d_->before();
    d_->levels.fill(d_->minimumDecibels);
    d_->displayed.fill(d_->minimumDecibels);
    d_->peaks.fill(d_->minimumDecibels);
    d_->holdRemaining.fill(0);
    d_->inputElapsed.invalidate();
    d_->publish(old);
}
void ZzAudioLevelMeter::paintEvent(QPaintEvent *) { d_->paint(); }
void ZzAudioLevelMeter::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    d_->publish(d_->before());
}
void ZzAudioLevelMeter::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    d_->publish(d_->before());
}
void ZzAudioLevelMeter::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::EnabledChange:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
    case QEvent::FontChange:
        if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange)
            updateGeometry();
        d_->publish(d_->before());
        return;
    default:
        break;
    }
}
} // namespace ZzFluentUI
