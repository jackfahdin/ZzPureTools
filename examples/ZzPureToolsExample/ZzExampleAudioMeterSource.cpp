#include "ZzExampleAudioMeterSource.h"
#include "ZzExampleCustomWidgetHelpers.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>
#include <ZzFluentUI/ZzAudioLevelMeter.h>
#include <algorithm>
#include <cmath>
#include <limits>

#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
#include "ZzExampleAudioLevels.h"
#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QMediaPlayer>
#endif

namespace ZzExample {
namespace {
QString zzAudioText(const char *text) { return QCoreApplication::translate("ZzPureToolsExample", text); }
}

ZzExampleAudioMeterSource::ZzExampleAudioMeterSource(QWidget *page, ZzFluentUI::ZzAudioLevelMeter *mono,
    ZzFluentUI::ZzAudioLevelMeter *stereo, ZzFluentUI::ZzAudioLevelMeter *preview)
    : QObject(page)
    , page_(page)
    , mono_(mono)
    , stereo_(stereo)
    , preview_(preview)
{
    setObjectName(QStringLiteral("zzAudioMeterSource"));
    page->installEventFilter(this);
    timer_.setInterval(45);
    connect(&timer_, &QTimer::timeout, this, &ZzExampleAudioMeterSource::simulate);
    simulate();
}

QWidget *ZzExampleAudioMeterSource::createTransport(QWidget *parent)
{
    auto *card = new ZzExampleCustomCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);
    auto *heading = new QLabel(zzAudioText("音乐播放与实时电平"), card);
    auto font = heading->font();
    font.setBold(true);
    font.setPixelSize(14);
    heading->setFont(font);
    layout->addWidget(heading);
    track_ = new QLabel(zzAudioText("请选择本地音乐文件"), card);
    track_->setObjectName(QStringLiteral("zzAudioTrack"));
    track_->setWordWrap(true);
    track_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(track_);
    auto *row = new QHBoxLayout;
    auto *open = new QPushButton(zzAudioText("打开音乐"), card);
    open->setObjectName(QStringLiteral("zzAudioOpen"));
    play_ = new QPushButton(zzAudioText("播放"), card);
    play_->setObjectName(QStringLiteral("zzAudioPlay"));
    play_->setEnabled(false);
    position_ = new QSlider(Qt::Horizontal, card);
    position_->setAccessibleName(zzAudioText("播放进度"));
    position_->setRange(0, 0);
    time_ = new QLabel(QStringLiteral("0:00 / 0:00"), card);
    volume_ = new QSlider(Qt::Horizontal, card);
    volume_->setObjectName(QStringLiteral("zzAudioVolume"));
    volume_->setAccessibleName(zzAudioText("音量"));
    volume_->setRange(0, 100);
    volume_->setValue(70);
    volume_->setFixedWidth(110);
    row->addWidget(open);
    row->addWidget(play_);
    row->addWidget(position_, 1);
    row->addWidget(time_);
    row->addSpacing(8);
    row->addWidget(new QLabel(zzAudioText("音量"), card));
    row->addWidget(volume_);
    layout->addLayout(row);
    simulationButton_ = new QPushButton(zzAudioText("模拟输入"), card);
    simulationButton_->setObjectName(QStringLiteral("zzAudioSimulation"));
    simulationButton_->setCheckable(true);
    simulationButton_->setChecked(true);
    connect(simulationButton_, &QPushButton::toggled, this, &ZzExampleAudioMeterSource::setSimulationEnabled);
    layout->addWidget(simulationButton_, 0, Qt::AlignLeft);
#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
    playbackHint_ = new QLabel(zzAudioText("当前播放环境无法提供实时电平，可使用模拟输入。"), card);
    playbackHint_->setWordWrap(true);
    playbackHint_->hide();
    layout->addWidget(playbackHint_);
    bufferMonitor_.setSingleShot(true);
    bufferMonitor_.setInterval(2000);
    connect(&bufferMonitor_, &QTimer::timeout, this, [this] {
        if (!receivedBuffer_ && !simulation_)
            playbackHint_->show();
    });
    connect(open, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(page_, zzAudioText("打开音乐"),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            zzAudioText("音频文件 (*.mp3 *.flac *.wav *.ogg *.aac *.m4a *.wma *.opus);;所有文件 (*.*)"));
        openFile(file);
    });
    connect(play_, &QPushButton::clicked, this, [this] {
        if (!player_)
            return;
        setSimulationEnabled(false);
        if (player_->playbackState() == QMediaPlayer::PlayingState)
            player_->pause();
        else
            player_->play();
    });
    connect(volume_, &QSlider::valueChanged, this, [this](int value) {
        if (output_)
            output_->setVolume(float(value) / 100.0f);
    });
    connect(position_, &QSlider::sliderMoved, this, [this](int value) {
        if (player_) {
            clear();
            player_->setPosition(value);
        }
    });
#else
    open->setEnabled(false);
    volume_->setEnabled(false);
    position_->setEnabled(false);
    track_->setText(zzAudioText("此版本不支持音乐播放，可使用模拟输入体验电平表。"));
#endif
    synchronize();
    return card;
}

void ZzExampleAudioMeterSource::setSimulationEnabled(bool enabled)
{
    if (simulation_ == enabled)
        return;
    simulation_ = enabled;
    if (simulationButton_) {
        const QSignalBlocker blocker(simulationButton_);
        simulationButton_->setChecked(enabled);
    }
#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
    if (enabled) {
        bufferMonitor_.stop();
        if (playbackHint_)
            playbackHint_->hide();
        if (player_)
            player_->pause();
    }
#endif
    if (enabled)
        simulate();
    synchronize();
}

bool ZzExampleAudioMeterSource::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == page_) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::EnabledChange:
        case QEvent::StyleChange:
            synchronize();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void ZzExampleAudioMeterSource::synchronize()
{
    const bool visible = page_ && page_->isVisible() && page_->isEnabled();
    const bool motion = visible && page_->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, page_);
    if (motion && simulation_)
        timer_.start();
    else
        timer_.stop();
#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
    if (!visible && player_)
        player_->pause();
#endif
}

void ZzExampleAudioMeterSource::simulate()
{
    if (!mono_ || !stereo_ || !preview_)
        return;
    phase_ += 0.16;
    const qreal left = -35.0 + 29.0 * (0.5 + 0.5 * std::sin(phase_));
    const qreal right = -38.0 + 31.0 * (0.5 + 0.5 * std::sin(phase_ * 0.83 + 1.2));
    mono_->setLevel(left);
    stereo_->setStereoLevels(left, right);
    QVector<qreal> levels(preview_->channelCount());
    for (qsizetype channel = 0; channel < levels.size(); ++channel)
        levels[channel] = -40.0
            + 34.0 * (0.5 + 0.5 * std::sin(phase_ * (0.75 + qreal(channel) * 0.04) + qreal(channel) * 0.71));
    preview_->setLevels(levels);
}

void ZzExampleAudioMeterSource::submit(const QVector<qreal> &levels)
{
    if (!mono_ || !stereo_ || !preview_)
        return;
    if (levels.isEmpty()) {
        clear();
        return;
    }
    mono_->setLevel(*std::max_element(levels.begin(), levels.end()));
    stereo_->setStereoLevels(levels[0], levels.size() > 1 ? levels[1] : levels[0]);
    QVector<qreal> mapped(preview_->channelCount(), -160.0);
    std::copy_n(levels.begin(), std::min(levels.size(), mapped.size()), mapped.begin());
    preview_->setLevels(mapped);
}

void ZzExampleAudioMeterSource::clear()
{
    for (const auto &meter : { mono_, stereo_, preview_ })
        if (meter)
            meter->clear();
}

#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
void ZzExampleAudioMeterSource::openFile(const QString &file)
{
    if (file.isEmpty() || !track_ || !page_ || !page_->isVisible() || !page_->isEnabled())
        return;
    ensurePlayer();
    setSimulationEnabled(false);
    clear();
    receivedBuffer_ = false;
    playbackHint_->hide();
    track_->setText(QFileInfo(file).fileName());
    play_->setEnabled(true);
    player_->setSource(QUrl::fromLocalFile(file));
    player_->play();
}

void ZzExampleAudioMeterSource::ensurePlayer()
{
    if (player_)
        return;
    player_ = new QMediaPlayer(this);
    output_ = new QAudioOutput(this);
    output_->setVolume(float(volume_->value()) / 100.0f);
    player_->setAudioOutput(output_);
    auto *buffers = new QAudioBufferOutput(this);
    player_->setAudioBufferOutput(buffers);
    connect(buffers, &QAudioBufferOutput::audioBufferReceived, this, [this](const QAudioBuffer &buffer) {
        if (buffer.isValid()) {
            receivedBuffer_ = true;
            bufferMonitor_.stop();
            playbackHint_->hide();
        }
        if (!simulation_ && page_ && page_->isVisible()
            && player_->playbackState() == QMediaPlayer::PlayingState)
            submit(zzAudioBufferLevels(buffer));
    });
    connect(player_, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        position_->setRange(0, int(std::clamp<qint64>(duration, 0, std::numeric_limits<int>::max())));
        refreshTime();
    });
    connect(player_, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (!position_->isSliderDown())
            position_->setValue(int(std::clamp<qint64>(position, 0, position_->maximum())));
        refreshTime();
    });
    connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        const bool playing = state == QMediaPlayer::PlayingState;
        if (playing && !receivedBuffer_)
            bufferMonitor_.start();
        else
            bufferMonitor_.stop();
        play_->setText(zzAudioText(playing ? "暂停" : "播放"));
        if (!playing && !simulation_)
            clear();
    });
    connect(player_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &error) {
        track_->setText(zzAudioText("播放失败：%1").arg(error));
        play_->setEnabled(false);
        clear();
    });
}

void ZzExampleAudioMeterSource::refreshTime()
{
    const auto format = [](qint64 ms) {
        const auto seconds = std::max<qint64>(0, ms) / 1000;
        return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    };
    time_->setText(QStringLiteral("%1 / %2").arg(format(player_->position()), format(player_->duration())));
}
#endif
} // namespace ZzExample
