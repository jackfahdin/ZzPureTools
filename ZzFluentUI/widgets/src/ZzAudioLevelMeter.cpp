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
    : QWidget(parent), d_ptr(std::make_unique<ZzAudioLevelMeterPrivate>(this))
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}
ZzAudioLevelMeter::~ZzAudioLevelMeter() = default;

int ZzAudioLevelMeter::channelCount() const { return d_ptr->channelCount; }
qreal ZzAudioLevelMeter::minimumDecibels() const { return d_ptr->minimumDecibels; }
qreal ZzAudioLevelMeter::maximumDecibels() const { return d_ptr->maximumDecibels; }
qreal ZzAudioLevelMeter::warningDecibels() const { return d_ptr->warningDecibels; }
qreal ZzAudioLevelMeter::clipDecibels() const { return d_ptr->clipDecibels; }
int ZzAudioLevelMeter::segmentCount() const { return d_ptr->segmentCount; }
qreal ZzAudioLevelMeter::segmentSpacing() const { return d_ptr->segmentSpacing; }
qreal ZzAudioLevelMeter::segmentRadius() const { return d_ptr->segmentRadius; }
qreal ZzAudioLevelMeter::channelSpacing() const { return d_ptr->channelSpacing; }
ZzAudioLevelMeter::ZzMeterScalePosition ZzAudioLevelMeter::scalePosition() const { return d_ptr->scalePosition; }
ZzAudioLevelMeter::ZzMeterScaleMode ZzAudioLevelMeter::scaleMode() const { return d_ptr->scaleMode; }
qreal ZzAudioLevelMeter::scaleInterval() const { return d_ptr->scaleInterval; }
int ZzAudioLevelMeter::scaleTickCount() const { return d_ptr->scaleTickCount; }
QString ZzAudioLevelMeter::scaleUnit() const { return d_ptr->scaleUnit; }
bool ZzAudioLevelMeter::isScaleUnitVisible() const { return d_ptr->scaleUnitVisible; }
int ZzAudioLevelMeter::scalePrecision() const { return d_ptr->scalePrecision; }
bool ZzAudioLevelMeter::areScaleTickMarksVisible() const { return d_ptr->scaleTickMarksVisible; }
qreal ZzAudioLevelMeter::scaleTickLength() const { return d_ptr->scaleTickLength; }
bool ZzAudioLevelMeter::areChannelLabelsVisible() const { return d_ptr->channelLabelsVisible; }
bool ZzAudioLevelMeter::isPeakHoldEnabled() const { return d_ptr->peakHoldEnabled; }
int ZzAudioLevelMeter::peakHoldDuration() const { return d_ptr->peakHoldDuration; }
qreal ZzAudioLevelMeter::decayRate() const { return d_ptr->decayRate; }
qreal ZzAudioLevelMeter::peakDecayRate() const { return d_ptr->peakDecayRate; }
int ZzAudioLevelMeter::inputTimeout() const { return d_ptr->inputTimeout; }
bool ZzAudioLevelMeter::isAnimationEnabled() const { return d_ptr->animationEnabled; }
ZzAudioLevelMeter::ZzMeterColorMode ZzAudioLevelMeter::colorMode() const { return d_ptr->colorMode; }
QColor ZzAudioLevelMeter::backgroundColor() const { return d_ptr->backgroundColor; }
QColor ZzAudioLevelMeter::activeColor() const { return d_ptr->activeColor; }
QColor ZzAudioLevelMeter::inactiveColor() const { return d_ptr->inactiveColor; }
QColor ZzAudioLevelMeter::warningColor() const { return d_ptr->warningColor; }
QColor ZzAudioLevelMeter::clipColor() const { return d_ptr->clipColor; }
QColor ZzAudioLevelMeter::peakColor() const { return d_ptr->peakColor; }
QColor ZzAudioLevelMeter::scaleColor() const { return d_ptr->scaleColor; }

QStringList ZzAudioLevelMeter::channelLabels() const { return d_ptr->channelLabels; }
QVector<qreal> ZzAudioLevelMeter::customScaleValues() const { return d_ptr->customScaleValues; }
QVector<qreal> ZzAudioLevelMeter::levels() const { return d_ptr->levels; }
QVector<qreal> ZzAudioLevelMeter::displayedLevels() const { return d_ptr->displayed; }
QVector<qreal> ZzAudioLevelMeter::peakLevels() const { return d_ptr->peaks; }
qreal ZzAudioLevelMeter::level(int channel) const { return d_ptr->levels.value(channel, d_ptr->minimumDecibels); }
qreal ZzAudioLevelMeter::displayedLevel(int channel) const { return d_ptr->displayed.value(channel, d_ptr->minimumDecibels); }
qreal ZzAudioLevelMeter::peakLevel(int channel) const { return d_ptr->peaks.value(channel, d_ptr->minimumDecibels); }
bool ZzAudioLevelMeter::isRunning() const { return d_ptr->timer.isActive(); }
QSize ZzAudioLevelMeter::sizeHint() const { return QSize(d_ptr->channelCount == 1 ? 100 : 70 + d_ptr->channelCount * 42, 320); }
QSize ZzAudioLevelMeter::minimumSizeHint() const { return QSize(d_ptr->channelCount == 1 ? 64 : 48 + d_ptr->channelCount * 24, 140); }

void ZzAudioLevelMeter::setMinimumDecibels(qreal value)
{
    if (std::isfinite(value)) setRange(qBound(-160.0, value, d_ptr->maximumDecibels - 1), d_ptr->maximumDecibels);
}
void ZzAudioLevelMeter::setMaximumDecibels(qreal value)
{
    if (std::isfinite(value)) setRange(d_ptr->minimumDecibels, qBound(d_ptr->minimumDecibels + 1, value, 24.0));
}
void ZzAudioLevelMeter::setRange(qreal minimum, qreal maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum) return;
    minimum = qBound(-160.0, minimum, 23.0);
    maximum = qBound(minimum + 1, maximum, 24.0);
    if (minimum == d_ptr->minimumDecibels && maximum == d_ptr->maximumDecibels) return;
    const auto old = d_ptr->before();
    std::vector<std::function<void()>> notifications;
    if (minimum != d_ptr->minimumDecibels)
        notifications.emplace_back([this, minimum] { Q_EMIT minimumDecibelsChanged(minimum); });
    if (maximum != d_ptr->maximumDecibels)
        notifications.emplace_back([this, maximum] { Q_EMIT maximumDecibelsChanged(maximum); });
    d_ptr->minimumDecibels = minimum;
    d_ptr->maximumDecibels = maximum;
    const qreal warning = qBound(minimum, d_ptr->warningDecibels, maximum);
    const qreal clip = qBound(warning, d_ptr->clipDecibels, maximum);
    if (warning != d_ptr->warningDecibels)
        notifications.emplace_back([this, warning] { Q_EMIT warningDecibelsChanged(warning); });
    if (clip != d_ptr->clipDecibels)
        notifications.emplace_back([this, clip] { Q_EMIT clipDecibelsChanged(clip); });
    d_ptr->warningDecibels = warning;
    d_ptr->clipDecibels = clip;
    d_ptr->resizeStorage();
    updateGeometry();
    d_ptr->publish(old, std::move(notifications));
}

void ZzAudioLevelMeter::setChannelCount(int value)
{
    value = qBound(1, value, 8);
    if (d_ptr->channelCount == value) return;
    const auto old = d_ptr->before();
    d_ptr->channelCount = value;
    d_ptr->resizeStorage();
    d_ptr->publish(old);
}

void ZzAudioLevelMeter::setWarningDecibels(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(d_ptr->minimumDecibels, value, d_ptr->clipDecibels);
    if (d_ptr->warningDecibels == value) return;
    const auto old = d_ptr->before();
    d_ptr->warningDecibels = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT warningDecibelsChanged(value); }});
}

void ZzAudioLevelMeter::setClipDecibels(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(d_ptr->warningDecibels, value, d_ptr->maximumDecibels);
    if (d_ptr->clipDecibels == value) return;
    const auto old = d_ptr->before();
    d_ptr->clipDecibels = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT clipDecibelsChanged(value); }});
}

void ZzAudioLevelMeter::setSegmentCount(int value)
{
    value = qBound(2, value, 120);
    if (d_ptr->segmentCount == value) return;
    const auto old = d_ptr->before();
    d_ptr->segmentCount = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT segmentCountChanged(value); }});
}

void ZzAudioLevelMeter::setSegmentSpacing(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 20.0);
    if (d_ptr->segmentSpacing == value) return;
    const auto old = d_ptr->before();
    d_ptr->segmentSpacing = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT segmentSpacingChanged(value); }});
}

void ZzAudioLevelMeter::setSegmentRadius(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 20.0);
    if (d_ptr->segmentRadius == value) return;
    const auto old = d_ptr->before();
    d_ptr->segmentRadius = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT segmentRadiusChanged(value); }});
}

void ZzAudioLevelMeter::setChannelSpacing(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 40.0);
    if (d_ptr->channelSpacing == value) return;
    const auto old = d_ptr->before();
    d_ptr->channelSpacing = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT channelSpacingChanged(value); }});
}

void ZzAudioLevelMeter::setScalePosition(ZzMeterScalePosition value)
{
    if (value < NoScale || value > CenterScale) return;
    if (d_ptr->scalePosition == value) return;
    const auto old = d_ptr->before();
    d_ptr->scalePosition = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scalePositionChanged(value); }});
}

void ZzAudioLevelMeter::setScaleMode(ZzMeterScaleMode value)
{
    if (value < IntervalScale || value > CustomScale) return;
    if (d_ptr->scaleMode == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleMode = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleModeChanged(value); }});
}

void ZzAudioLevelMeter::setScaleInterval(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(1.0, value, 60.0);
    if (d_ptr->scaleInterval == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleInterval = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleIntervalChanged(value); }});
}

void ZzAudioLevelMeter::setScaleTickCount(int value)
{
    value = qBound(2, value, 64);
    if (d_ptr->scaleTickCount == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleTickCount = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleTickCountChanged(value); }});
}

void ZzAudioLevelMeter::setScaleUnit(QString value)
{
    if (d_ptr->scaleUnit == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleUnit = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleUnitChanged(value); }});
}

void ZzAudioLevelMeter::setScaleUnitVisible(bool value)
{
    if (d_ptr->scaleUnitVisible == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleUnitVisible = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleUnitVisibleChanged(value); }});
}

void ZzAudioLevelMeter::setScalePrecision(int value)
{
    value = qBound(0, value, 3);
    if (d_ptr->scalePrecision == value) return;
    const auto old = d_ptr->before();
    d_ptr->scalePrecision = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scalePrecisionChanged(value); }});
}

void ZzAudioLevelMeter::setScaleTickMarksVisible(bool value)
{
    if (d_ptr->scaleTickMarksVisible == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleTickMarksVisible = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleTickMarksVisibleChanged(value); }});
}

void ZzAudioLevelMeter::setScaleTickLength(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(1.0, value, 20.0);
    if (d_ptr->scaleTickLength == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleTickLength = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleTickLengthChanged(value); }});
}

void ZzAudioLevelMeter::setChannelLabelsVisible(bool value)
{
    if (d_ptr->channelLabelsVisible == value) return;
    const auto old = d_ptr->before();
    d_ptr->channelLabelsVisible = value;
    updateGeometry();
    d_ptr->publish(old, {[this, value] { Q_EMIT channelLabelsVisibleChanged(value); }});
}

void ZzAudioLevelMeter::setPeakHoldEnabled(bool value)
{
    if (d_ptr->peakHoldEnabled == value) return;
    const auto old = d_ptr->before();
    d_ptr->peakHoldEnabled = value;
    if (!value) d_ptr->peaks = d_ptr->displayed;
    d_ptr->holdRemaining.fill(value ? d_ptr->peakHoldDuration : 0);
    d_ptr->publish(old, {[this, value] { Q_EMIT peakHoldEnabledChanged(value); }});
}

void ZzAudioLevelMeter::setPeakHoldDuration(int value)
{
    value = qBound(0, value, 10000);
    if (d_ptr->peakHoldDuration == value) return;
    const auto old = d_ptr->before();
    d_ptr->peakHoldDuration = value;
    for (qreal &remaining : d_ptr->holdRemaining) remaining = qMin(remaining, qreal(value));
    d_ptr->publish(old, {[this, value] { Q_EMIT peakHoldDurationChanged(value); }});
}

void ZzAudioLevelMeter::setDecayRate(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 1000.0);
    if (d_ptr->decayRate == value) return;
    const auto old = d_ptr->before();
    d_ptr->decayRate = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT decayRateChanged(value); }});
}

void ZzAudioLevelMeter::setPeakDecayRate(qreal value)
{
    if (!std::isfinite(value)) return;
    value = qBound(0.0, value, 1000.0);
    if (d_ptr->peakDecayRate == value) return;
    const auto old = d_ptr->before();
    d_ptr->peakDecayRate = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT peakDecayRateChanged(value); }});
}

void ZzAudioLevelMeter::setInputTimeout(int value)
{
    value = qBound(0, value, 10000);
    if (d_ptr->inputTimeout == value) return;
    const auto old = d_ptr->before();
    d_ptr->inputTimeout = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT inputTimeoutChanged(value); }});
}

void ZzAudioLevelMeter::setAnimationEnabled(bool value)
{
    if (d_ptr->animationEnabled == value) return;
    const auto old = d_ptr->before();
    d_ptr->animationEnabled = value;
    d_ptr->holdRemaining.fill(value ? d_ptr->peakHoldDuration : 0);
    d_ptr->publish(old, {[this, value] { Q_EMIT animationEnabledChanged(value); }});
}

void ZzAudioLevelMeter::setColorMode(ZzMeterColorMode value)
{
    if (value < SingleColor || value > GradientColors) return;
    if (d_ptr->colorMode == value) return;
    const auto old = d_ptr->before();
    d_ptr->colorMode = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT colorModeChanged(value); }});
}

void ZzAudioLevelMeter::setBackgroundColor(QColor value)
{
    if (d_ptr->backgroundColor == value) return;
    const auto old = d_ptr->before();
    d_ptr->backgroundColor = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT backgroundColorChanged(value); }});
}

void ZzAudioLevelMeter::setActiveColor(QColor value)
{
    if (d_ptr->activeColor == value) return;
    const auto old = d_ptr->before();
    d_ptr->activeColor = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT activeColorChanged(value); }});
}

void ZzAudioLevelMeter::setInactiveColor(QColor value)
{
    if (d_ptr->inactiveColor == value) return;
    const auto old = d_ptr->before();
    d_ptr->inactiveColor = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT inactiveColorChanged(value); }});
}

void ZzAudioLevelMeter::setWarningColor(QColor value)
{
    if (d_ptr->warningColor == value) return;
    const auto old = d_ptr->before();
    d_ptr->warningColor = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT warningColorChanged(value); }});
}

void ZzAudioLevelMeter::setClipColor(QColor value)
{
    if (d_ptr->clipColor == value) return;
    const auto old = d_ptr->before();
    d_ptr->clipColor = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT clipColorChanged(value); }});
}

void ZzAudioLevelMeter::setPeakColor(QColor value)
{
    if (d_ptr->peakColor == value) return;
    const auto old = d_ptr->before();
    d_ptr->peakColor = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT peakColorChanged(value); }});
}

void ZzAudioLevelMeter::setScaleColor(QColor value)
{
    if (d_ptr->scaleColor == value) return;
    const auto old = d_ptr->before();
    d_ptr->scaleColor = value;
    d_ptr->publish(old, {[this, value] { Q_EMIT scaleColorChanged(value); }});
}

void ZzAudioLevelMeter::setChannelLabels(const QStringList &labels)
{
    if (d_ptr->channelLabels == labels) return;
    const auto old = d_ptr->before();
    d_ptr->channelLabels = labels;
    d_ptr->publish(old, {[this, value = labels] { Q_EMIT channelLabelsChanged(value); }});
}
void ZzAudioLevelMeter::setCustomScaleValues(const QVector<qreal> &values)
{
    QVector<qreal> normalized;
    normalized.reserve(values.size());
    for (qreal value : values) if (std::isfinite(value)) normalized.append(value);
    std::sort(normalized.begin(), normalized.end(), std::greater<qreal>());
    normalized.erase(std::unique(normalized.begin(), normalized.end(),
        [](qreal a, qreal b) { return qAbs(a - b) < 0.0001; }), normalized.end());
    if (d_ptr->customScaleValues == normalized) return;
    const auto old = d_ptr->before();
    d_ptr->customScaleValues = normalized;
    updateGeometry();
    d_ptr->publish(old, {[this, normalized] { Q_EMIT customScaleValuesChanged(normalized); }});
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
        QMetaObject::invokeMethod(this, [this, copy] { d_ptr->apply(copy); }, Qt::QueuedConnection);
        return;
    }
    d_ptr->apply(values);
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
    const auto old = d_ptr->before();
    d_ptr->peaks = d_ptr->displayed;
    d_ptr->holdRemaining.fill(d_ptr->peakHoldDuration);
    d_ptr->publish(old, {}, true);
}
void ZzAudioLevelMeter::clear()
{
    const auto old = d_ptr->before();
    d_ptr->levels.fill(d_ptr->minimumDecibels);
    d_ptr->displayed.fill(d_ptr->minimumDecibels);
    d_ptr->peaks.fill(d_ptr->minimumDecibels);
    d_ptr->holdRemaining.fill(0);
    d_ptr->inputElapsed.invalidate();
    d_ptr->publish(old);
}
void ZzAudioLevelMeter::paintEvent(QPaintEvent *) { d_ptr->paint(); }
void ZzAudioLevelMeter::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    d_ptr->publish(d_ptr->before());
}
void ZzAudioLevelMeter::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    d_ptr->publish(d_ptr->before());
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
        d_ptr->publish(d_ptr->before());
        return;
    default:
        break;
    }
}
} // namespace ZzFluentUI
