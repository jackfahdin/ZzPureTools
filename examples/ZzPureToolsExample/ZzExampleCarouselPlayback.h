#pragma once

#include <QPointer>
#include <QTimer>
#include <ZzFluentUI/ZzCarouselView.h>

namespace ZzExample {

/** @brief 由示例页面拥有的播放控制器，不改变轮播视图的模型和所有权。 */
class ZzExampleCarouselPlayback final : public QObject {
public:
    explicit ZzExampleCarouselPlayback(ZzFluentUI::ZzCarouselView* view, QObject* parent);
    [[nodiscard]] bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled);
    [[nodiscard]] int interval() const { return interval_; }
    void setInterval(int milliseconds);
    [[nodiscard]] bool pauseOnHover() const { return pauseOnHover_; }
    void setPauseOnHover(bool enabled);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void restart();
    [[nodiscard]] bool canPlay() const;
    QPointer<ZzFluentUI::ZzCarouselView> view_;
    QTimer timer_;
    bool enabled_ = false;
    bool pauseOnHover_ = true;
    bool hovered_ = false;
    int interval_ = 3500;
};

} // namespace ZzExample
