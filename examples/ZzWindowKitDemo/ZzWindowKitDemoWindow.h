#pragma once

#include <memory>

#include <QtWidgets/QMainWindow>

class ZzWindowKitDemoWindowPrivate;

/**
 * @brief 展示默认无边框后端基本窗口交互的最小主窗口。
 */
class ZzWindowKitDemoWindow final : public QMainWindow
{
public:
    /**
     * @brief 创建并完整配置示例标题栏。
     * @param parent 可选 QWidget 所有者。
     */
    explicit ZzWindowKitDemoWindow(QWidget *parent = nullptr);

    /** @brief 销毁窗口私有实现和窗口代理。 */
    ~ZzWindowKitDemoWindow() override;

    ZzWindowKitDemoWindow(const ZzWindowKitDemoWindow &) = delete;
    ZzWindowKitDemoWindow &operator=(const ZzWindowKitDemoWindow &) = delete;
    ZzWindowKitDemoWindow(ZzWindowKitDemoWindow &&) = delete;
    ZzWindowKitDemoWindow &operator=(ZzWindowKitDemoWindow &&) = delete;

private:
    std::unique_ptr<ZzWindowKitDemoWindowPrivate> d_ptr;
};
