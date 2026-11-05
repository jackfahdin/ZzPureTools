#pragma once
#include <ZzFluentUI/ZzAudioSpectrum.h>
#include <QTimer>
#include "ZzSpectrumAnalyzer.h"
#include "ZzSpectrumInput.h"
#include "ZzWidgetTheme.h"
namespace ZzFluentUI {
class ZzAudioSpectrumPrivate final
{
public:
    using Clock = ZzSpectrumInput::Clock;
    struct ZzPublished { QVector<qreal> levels, displayed, peaks; bool running; };
    explicit ZzAudioSpectrumPrivate(ZzAudioSpectrum *owner);
    ZzPublished before() const;
    void publish(const ZzPublished &old, bool configuration = false, bool appearance = false);
    void reconfigure();
    void reset();
    void wake();
    void tick();
    void synchronize();
    void snapToTarget();
    bool animated() const;
    void paint();
    ZzAudioSpectrum *const q;
    ZzWidgetTheme theme;
    ZzSpectrumAnalyzer analyzer;
    ZzSpectrumInput input;
    ZzSpectrumSettings settings;
    QTimer timer;
    QVector<qreal> target, displayed, peaks, velocity, peakVelocity;
    QVector<Clock::time_point> peakTimes;
    Clock::time_point lastTick{}, lastInput{};
    quint64 serial = 0, revision = 0;
    bool presented = false;
    ZzAudioSpectrum::InputMode inputMode = ZzAudioSpectrum::Pcm;
    int refreshInterval = 16;
    bool animationEnabled = true;
    int inputTimeout = 120;
    qreal gravity = 4.2;
    int peakHoldDuration = 200;
    qreal barGap = 2;
    qreal cornerRadius = 3;
    bool reflectionEnabled = true;
    qreal reflectionOpacity = 0.58;
    bool peakEnabled = true;
    qreal peakHeight = 2;
    bool rainbowEnabled = true;
    QColor accentColor;
    QColor backgroundColor;
};
}
