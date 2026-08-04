#pragma once

#include <memory>

#include <ZzWindowKit/ZzWindowAgent.h>

class QPushButton;
class QWidget;
class ZzWindowKitDemoWindow;

/**
 * @brief 构建示例内容并持有无边框窗口代理。
 */
class ZzWindowKitDemoWindowPrivate final
{
public:
    explicit ZzWindowKitDemoWindowPrivate(ZzWindowKitDemoWindow *window);

    void initialize();

    ZzWindowKitDemoWindow *const q_ptr;
    std::unique_ptr<ZzWindowKit::ZzWindowAgent> agent;
    QWidget *titleBar = nullptr;
    QPushButton *minimizeButton = nullptr;
    QPushButton *maximizeButton = nullptr;
    QPushButton *closeButton = nullptr;
};
