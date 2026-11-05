#include "ZzExampleAudioSpectrumPage.h"
#include "ZzExampleCustomCard.h"
#include <ZzFluentUI/ZzAudioSpectrum.h>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <numbers>

namespace ZzExample {
namespace {
using Spectrum = ZzFluentUI::ZzAudioSpectrum;
QString text(const char *value) { return QCoreApplication::translate("ZzAudioSpectrumExample", value); }

/** @brief 页面可见时生成 PCM；不访问设备、不播放声音。 */
class ZzSpectrumDemoSource final : public QObject
{
public:
    ZzSpectrumDemoSource(QWidget *page, Spectrum *spectrum, QComboBox *source, QPushButton *play)
        : QObject(page), page_(page), spectrum_(spectrum), source_(source), play_(play),
          timer_(this), presented_(page->isVisible())
    {
        timer_.setObjectName(QStringLiteral("spectrumDemoTimer"));
        timer_.setInterval(20);
        page->installEventFilter(this);
        connect(&timer_, &QTimer::timeout, this, [this] { generate(); });
        connect(play, &QPushButton::toggled, this, [this](bool) { sync(); });
        sync();
    }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == page_ && (event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
            presented_ = event->type() == QEvent::Show;
            sync();
        }
        return QObject::eventFilter(watched, event);
    }
private:
    void sync()
    {
        if (presented_ && page_->isVisible() && play_->isChecked()) timer_.start();
        else timer_.stop();
    }
    void generate()
    {
        if (!spectrum_) { timer_.stop(); return; }
        const int rate = spectrum_->sampleRate();
        const int channels = spectrum_->channelCount();
        const int frames = rate / 50;
        QVector<float> samples(frames * channels);
        for (int frame = 0; frame < frames; ++frame) {
            const qreal frequency = source_->currentIndex() == 2
                ? 80 * std::pow(150.0, std::fmod(seconds_, 6.0) / 6.0) : 1000;
            phase_ = std::fmod(phase_ + frequency / rate, 1.0);
            qreal value = 0.65 * std::sin(2 * std::numbers::pi * phase_);
            if (source_->currentIndex() == 1) {
                value = 0;
                for (const int hz : {220, 1000, 4000})
                    value += 0.25 * std::sin(2 * std::numbers::pi * hz * seconds_);
            } else if (source_->currentIndex() == 3) value = 0;
            for (int channel = 0; channel < channels; ++channel)
                samples[frame * channels + channel] = static_cast<float>(value);
            seconds_ += 1.0 / rate;
        }
        spectrum_->pushSamples(samples);
    }
    QWidget *page_;
    QPointer<Spectrum> spectrum_;
    QComboBox *source_;
    QPushButton *play_;
    QTimer timer_;
    bool presented_ = false;
    qreal phase_ = 0, seconds_ = 0;
};
}

QWidget *ZzExampleAudioSpectrumPage::create(QWidget *parent)
{
    auto *page = new QWidget(parent);
    page->setObjectName(QStringLiteral("zzExampleAudioSpectrumPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    auto *description = new QLabel(text("观察声音的频率分布。切换信号和外观，体验峰值保持、平滑回落与渐隐倒影。演示信号不会播放声音。"), page);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *card = new ZzExampleCustomCard(page);
    auto *previewLayout = new QVBoxLayout(card);
    previewLayout->setContentsMargins(20, 20, 20, 16);
    auto *spectrum = new Spectrum(card);
    spectrum->setObjectName(QStringLiteral("zzSpectrumPreview"));
    spectrum->setAccessibleName(text("实时频谱预览"));
    spectrum->setChannelCount(2);
    spectrum->setMinimumHeight(240);
    previewLayout->addWidget(spectrum);
    auto *transport = new QHBoxLayout;
    auto *source = new QComboBox(card);
    source->setObjectName(QStringLiteral("spectrumSource"));
    source->setAccessibleName(text("演示信号"));
    source->addItems({text("正弦 · 1 kHz"), text("多音 · 220 / 1000 / 4000 Hz"), text("扫频 · 80 Hz—12 kHz"), text("静音")});
    source->setCurrentIndex(1);
    auto *play = new QPushButton(text("运行"), card);
    play->setObjectName(QStringLiteral("spectrumPlay"));
    play->setCheckable(true);
    play->setChecked(true);
    transport->addWidget(source, 1);
    transport->addWidget(play);
    previewLayout->addLayout(transport);
    layout->addWidget(card);

    auto *settings = new ZzExampleCustomCard(page);
    auto *form = new QFormLayout(settings);
    form->setContentsMargins(20, 16, 20, 20);
    form->setSpacing(12);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const auto integer = [settings, form](const char *label, const char *name, int low, int high, int value) {
        auto *spin = new QSpinBox(settings);
        spin->setObjectName(QString::fromLatin1(name));
        spin->setRange(low, high);
        spin->setValue(value);
        spin->setAccessibleName(text(label));
        form->addRow(text(label), spin);
        return spin;
    };
    auto *bars = integer("频带数量", "spectrumBars", 8, 256, spectrum->barCount());
    QObject::connect(bars, &QSpinBox::valueChanged, spectrum, &Spectrum::setBarCount);
    auto *fft = new QComboBox(settings);
    fft->setObjectName(QStringLiteral("spectrumFft"));
    fft->setAccessibleName(text("频率分辨率"));
    for (int size : {1024, 2048, 4096, 8192}) fft->addItem(QString::number(size), size);
    fft->setCurrentIndex(1);
    form->addRow(text("频率分辨率"), fft);
    QObject::connect(fft, &QComboBox::currentIndexChanged, spectrum,
        [fft, spectrum](int index) { spectrum->setFftSize(fft->itemData(index).toInt()); });
    auto *low = integer("最低频率 / Hz", "spectrumLow", 10, 15000, 60);
    auto *high = integer("最高频率 / Hz", "spectrumHigh", 100, 22050, 16000);
    const auto syncFrequency = [spectrum, low, high] {
        const QSignalBlocker lowBlocker(low), highBlocker(high);
        low->setMaximum(static_cast<int>(spectrum->maximumFrequency()) - 1);
        high->setMinimum(static_cast<int>(spectrum->minimumFrequency()) + 1);
        low->setValue(static_cast<int>(spectrum->minimumFrequency()));
        high->setValue(static_cast<int>(spectrum->maximumFrequency()));
    };
    QObject::connect(spectrum, &Spectrum::configurationChanged, page, syncFrequency);
    QObject::connect(low, &QSpinBox::valueChanged, spectrum, &Spectrum::setMinimumFrequency);
    QObject::connect(high, &QSpinBox::valueChanged, spectrum, &Spectrum::setMaximumFrequency);
    syncFrequency();
    const auto check = [settings, form, spectrum](const char *label, const char *name, bool value, auto setter) {
        auto *box = new QCheckBox(text(label), settings);
        box->setObjectName(QString::fromLatin1(name));
        box->setChecked(value);
        form->addRow(box);
        QObject::connect(box, &QCheckBox::toggled, spectrum, setter);
    };
    check("彩虹配色（关闭后跟随主题）", "spectrumRainbow", true, &Spectrum::setRainbowEnabled);
    check("渐隐倒影", "spectrumReflection", true, &Spectrum::setReflectionEnabled);
    check("峰值保持", "spectrumPeak", true, &Spectrum::setPeakEnabled);
    check("平滑动画", "spectrumMotion", true, &Spectrum::setAnimationEnabled);
    layout->addWidget(settings);
    auto *hint = new QLabel(text("较大的 FFT 窗口能区分更接近的低频，但响应延迟也会增加。真实应用可直接输入 PCM，或传入已计算的频带。"), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    new ZzSpectrumDemoSource(page, spectrum, source, play);
    return page;
}
}
