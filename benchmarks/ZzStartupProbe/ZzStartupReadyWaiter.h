#pragma once

#include <functional>
#include <utility>

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

/** @brief 等待首帧后的窗口曝光与焦点，不改变首次绘制时间。 */
class ZzStartupReadyWaiter final : public QObject
{
public:
    /** @brief 在就绪或超时后仅调用一次完成回调；销毁观察器会取消等待。 */
    ZzStartupReadyWaiter(QWidget *window, std::function<void(bool)> finished,
        QObject *parent = nullptr, int timeoutMs = 5000)
        : QObject(parent), window_(window), finished_(std::move(finished)),
          timeoutMs_(timeoutMs)
    {
        timer_.setSingleShot(true);
        connect(&timer_, &QTimer::timeout, this, [this] { check(); });
    }

    /** @brief 从下一次事件循环开始检查，重复启动无效。 */
    void start()
    {
        if (started_) {
            return;
        }
        started_ = true;
        elapsed_.start();
        timer_.start(0);
    }

private:
    void check()
    {
        const bool usable = !window_.isNull() && window_->isVisible()
            && window_->isEnabled() && window_->windowHandle() != nullptr
            && window_->windowHandle()->isExposed()
            && QApplication::focusWidget() != nullptr;
        if (!usable && !window_.isNull() && elapsed_.elapsed() < timeoutMs_) {
            timer_.start(1);
            return;
        }
        // The callback may destroy this observer; do not access members after it.
        auto finished = std::move(finished_);
        finished(usable);
    }

    QPointer<QWidget> window_;
    std::function<void(bool)> finished_;
    QTimer timer_;
    QElapsedTimer elapsed_;
    int timeoutMs_;
    bool started_ = false;
};
