#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

class QWidget;
class QLabel;
class QPushButton;
class QSlider;
class QMediaPlayer;
class QAudioOutput;
namespace ZzFluentUI {
class ZzAudioLevelMeter;
}

namespace ZzExample {
/** @brief 示例专用输入源：模拟电平或本地音乐，隐藏页面时停止输入和播放。 */
class ZzExampleAudioMeterSource final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool simulationEnabled READ simulationEnabled WRITE setSimulationEnabled)
public:
    ZzExampleAudioMeterSource(QWidget *page, ZzFluentUI::ZzAudioLevelMeter *mono,
        ZzFluentUI::ZzAudioLevelMeter *stereo, ZzFluentUI::ZzAudioLevelMeter *preview);
    QWidget *createTransport(QWidget *parent);
    [[nodiscard]] bool simulationEnabled() const { return simulation_; }
    void setSimulationEnabled(bool enabled);
#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
    /** @brief 工具栏创建后打开本地文件；可与文件选择器或拖放入口共用。 */
    void openFile(const QString &file);
#endif

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void synchronize();
    void simulate();
    void submit(const QVector<qreal> &levels);
    void clear();
#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
    void ensurePlayer();
    void refreshTime();
#endif
    QPointer<QWidget> page_;
    QPointer<ZzFluentUI::ZzAudioLevelMeter> mono_, stereo_, preview_;
    QTimer timer_;
    bool simulation_ = true;
    qreal phase_ = 0;
    QLabel *track_ = nullptr;
    QPushButton *play_ = nullptr;
    QPushButton *simulationButton_ = nullptr;
    QSlider *position_ = nullptr;
    QSlider *volume_ = nullptr;
    QLabel *time_ = nullptr;
#ifdef ZZ_EXAMPLE_HAS_MULTIMEDIA
    QMediaPlayer *player_ = nullptr;
    QAudioOutput *output_ = nullptr;
    QLabel *playbackHint_ = nullptr;
    QTimer bufferMonitor_;
    bool receivedBuffer_ = false;
#endif
};
} // namespace ZzExample
