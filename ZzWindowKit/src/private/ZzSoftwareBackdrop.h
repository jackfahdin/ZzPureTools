#pragma once

#include <cstddef>
#include <memory>

#include <QtCore/QObject>

class QEvent;
class QWidget;

namespace ZzWindowKit {

class ZzSoftwareBackdropPrivate;

/**
 * @brief 为窗口提供不依赖桌面采样的私有软件材质层。
 *
 * 该对象只绘制宿主窗口内部的缓存背景，不等价于系统 Mica/Acrylic，
 * 也不读取平台原生窗口句柄。宿主窗口和其子控件均由调用方拥有。
 */
class ZzSoftwareBackdrop final : public QObject
{
public:
    /**
     * @brief 创建未绑定宿主的软件材质控制器。
     * @param parent 可选 QObject 所有者。
     */
    explicit ZzSoftwareBackdrop(QObject *parent = nullptr);

    /** @brief 销毁控制器并解除宿主事件过滤。 */
    ~ZzSoftwareBackdrop() override;

    ZzSoftwareBackdrop(const ZzSoftwareBackdrop &) = delete;
    ZzSoftwareBackdrop &operator=(const ZzSoftwareBackdrop &) = delete;
    ZzSoftwareBackdrop(ZzSoftwareBackdrop &&) = delete;
    ZzSoftwareBackdrop &operator=(ZzSoftwareBackdrop &&) = delete;

    /**
     * @brief 绑定一个顶层 QWidget 宿主。
     * @param host 非拥有的顶层窗口。
     * @return 绑定成功返回 true。
     */
    [[nodiscard]] bool attach(QWidget *host);

    /** @brief 解除宿主绑定并隐藏材质层。 */
    void detach();

    /**
     * @brief 设置软件材质层是否可见。
     * @param enabled true 显示，false 隐藏并释放图像缓存。
     * @return 操作成功返回 true。
     */
    [[nodiscard]] bool setEnabled(bool enabled);

    /** @brief 获取软件材质层是否处于启用状态。 */
    [[nodiscard]] bool isEnabled() const noexcept;

    /**
     * @brief 获取缓存重建次数。
     * @return 仅供 WindowKit 私有测试诊断使用的累计计数。
     */
    [[nodiscard]] std::size_t rebuildCount() const noexcept;

protected:
    /** @brief 接收宿主尺寸、调色板和屏幕变化并转发给私有实现。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::unique_ptr<ZzSoftwareBackdropPrivate> d_ptr;
};

} // namespace ZzWindowKit
