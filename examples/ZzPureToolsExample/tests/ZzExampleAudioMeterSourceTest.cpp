#include <ZzTestEventLoop.h>
#include "../ZzExampleAudioMeterSource.h"
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTemporaryFile>
#include <QTest>
#include <QVBoxLayout>
#include <ZzFluentUI/ZzAudioLevelMeter.h>
#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QMediaPlayer>
#endif

using ZzExample::ZzExampleAudioMeterSource;
using Meter = ZzFluentUI::ZzAudioLevelMeter;

class ZzExampleAudioMeterSourceTest final : public QObject
{
    Q_OBJECT
private slots:
    void simulationFollowsPageVisibility()
    {
        QWidget page;
        auto *layout = new QVBoxLayout(&page);
        auto *meter = new Meter(&page);
        meter->setAnimationEnabled(false);
        layout->addWidget(meter);
        auto *source = new ZzExampleAudioMeterSource(&page, meter, meter, meter);
        layout->addWidget(source->createTransport(&page));
        page.show();
        QTest::qWait(90);
        const auto initial = meter->levels();
        ZZ_VERIFY_EVENTUALLY(meter->levels() != initial);
        page.hide();
        const auto hidden = meter->levels();
        QTest::qWait(150);
        QCOMPARE(meter->levels(), hidden);
        page.show();
        ZZ_VERIFY_EVENTUALLY(meter->levels() != hidden);
        page.setEnabled(false);
        const auto disabled = meter->levels();
        QTest::qWait(150);
        QCOMPARE(meter->levels(), disabled);
        page.setEnabled(true);
        source->setSimulationEnabled(false);
        const auto stopped = meter->levels();
        QTest::qWait(150);
        QCOMPARE(meter->levels(), stopped);
#ifndef ZZ_EXAMPLE_HAS_MULTIMEDIA
        QVERIFY(!page.findChild<QPushButton *>(QStringLiteral("zzAudioOpen"))->isEnabled());
        QVERIFY(!page.findChild<QLabel *>(QStringLiteral("zzAudioTrack"))->text().isEmpty());
#else
        QVERIFY(!source->findChild<QMediaPlayer *>());
#endif
    }

#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
    void playsIndependentChannelsAndPausesWhenHidden()
    {
        // 三秒钟恒定双声道 PCM：L=0.5，R=0.125，验证真实解码帧而非模拟回调。
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/zz-meter-XXXXXX.wav"));
        QVERIFY(file.open());
        constexpr quint32 samples = 48000 * 3;
        constexpr quint32 bytes = samples * 4;
        QDataStream data(&file);
        data.setByteOrder(QDataStream::LittleEndian);
        data.writeRawData("RIFF", 4);
        data << quint32(36 + bytes);
        data.writeRawData("WAVEfmt ", 8);
        data << quint32(16) << quint16(1) << quint16(2) << quint32(48000) << quint32(192000) << quint16(4)
             << quint16(16);
        data.writeRawData("data", 4);
        data << bytes;
        for (quint32 i = 0; i < samples; ++i)
            data << qint16(16384) << qint16(4096);
        QVERIFY(file.flush());
        file.close();
        {
            // Qt 可回退到原生后端；isAvailable 不代表支持公开解码帧接口。
            // 独立探针成功后才测试输入源，避免把组件回归当作环境限制跳过。
            QAudioOutput output;
            output.setVolume(0);
            QAudioBufferOutput buffers;
            QMediaPlayer probe;
            if (!probe.isAvailable())
                QSKIP("No Qt media backend installed; simulation and PCM tests remain available.");
            bool received = false;
            connect(&buffers, &QAudioBufferOutput::audioBufferReceived, &probe,
                [&received](const QAudioBuffer &buffer) { received = received || buffer.isValid(); });
            probe.setAudioOutput(&output);
            probe.setAudioBufferOutput(&buffers);
            probe.setSource(QUrl::fromLocalFile(file.fileName()));
            probe.play();
            QElapsedTimer elapsed;
            elapsed.start();
            while (!received && elapsed.elapsed() < 4000)
                QTest::qWait(20);
            probe.stop();
            if (!received)
                QSKIP("Qt backend cannot deliver decoded WAV buffers; FFmpeg backend required.");
        }
        QWidget page;
        auto *layout = new QVBoxLayout(&page);
        auto *mono = new Meter(&page);
        mono->setChannelCount(1);
        auto *stereo = new Meter(&page);
        auto *preview = new Meter(&page);
        preview->setChannelCount(4);
        for (auto *meter : { mono, stereo, preview }) {
            meter->setAnimationEnabled(false);
            layout->addWidget(meter);
        }
        auto *source = new ZzExampleAudioMeterSource(&page, mono, stereo, preview);
        layout->addWidget(source->createTransport(&page));
        page.findChild<QSlider *>(QStringLiteral("zzAudioVolume"))->setValue(0);
        page.show();
        source->openFile(file.fileName());
        auto *player = source->findChild<QMediaPlayer *>();
        QVERIFY(player);
        QVERIFY(player->isAvailable());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(stereo->levels().at(0) > -7 && stereo->levels().at(0) < -5, 5000);
        QVERIFY(qAbs(stereo->levels().at(1) + 18.0618) < 0.1);
        QCOMPARE(mono->levels().at(0), stereo->levels().at(0));
        QCOMPARE(preview->channelCount(), 4);
        QCOMPARE(preview->levels().at(2), preview->minimumDecibels());
        page.hide();
        QCOMPARE(player->playbackState(), QMediaPlayer::PausedState);
        QCOMPARE(stereo->levels().at(0), stereo->minimumDecibels());
        page.show();
        QCOMPARE(player->playbackState(), QMediaPlayer::PausedState);
        page.findChild<QPushButton *>(QStringLiteral("zzAudioPlay"))->click();
        ZZ_COMPARE_EVENTUALLY(player->playbackState(), QMediaPlayer::PlayingState);
        source->setSimulationEnabled(true);
        QCOMPARE(player->playbackState(), QMediaPlayer::PausedState);
    }
#endif
};

QTEST_MAIN(ZzExampleAudioMeterSourceTest)
#include "ZzExampleAudioMeterSourceTest.moc"
