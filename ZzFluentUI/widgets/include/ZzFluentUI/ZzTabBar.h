#pragma once

#include <memory>
#include <QtCore/QPoint>

#include <QtWidgets/QTabBar>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;
class QPaintEvent;
class QContextMenuEvent;

namespace ZzFluentUI {

class ZzTabBarPrivate;
class ZzTabWidget;

/**
 * @brief 提供进程内标签转移和拖出意图的 Fluent 标签栏。
 *
 * 标签栏只使用 Qt 公开 API。页面所有权由宿主 ZzTabWidget 管理，
 * 拖拽成功前不会从来源容器移除页面。
 */
class ZZ_FLUENT_UI_EXPORT ZzTabBar final : public QTabBar
{
    Q_OBJECT
    Q_PROPERTY(
        bool tearOffEnabled
        READ isTearOffEnabled
        WRITE setTearOffEnabled
        NOTIFY tearOffEnabledChanged)
    Q_PROPERTY(
        bool tabTransferEnabled
        READ isTabTransferEnabled
        WRITE setTabTransferEnabled
        NOTIFY tabTransferEnabledChanged)
    Q_DISABLE_COPY_MOVE(ZzTabBar)

public:
    /**
     * @brief 创建支持移动和进程内拖拽的标签栏。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzTabBar(QWidget *parent = nullptr);

    /** @brief 销毁私有拖拽状态。 */
    ~ZzTabBar() override;

    /**
     * @brief 返回释放到兼容目标外时是否发出拖出意图。
     * @return 启用时返回 true。
     */
    [[nodiscard]] bool isTearOffEnabled() const noexcept;

    /**
     * @brief 设置是否允许发出拖出意图。
     * @param enabled 是否启用。
     */
    void setTearOffEnabled(bool enabled);

    /**
     * @brief 返回是否接受其他 ZzTabWidget 的标签。
     * @return 接受时返回 true。
     */
    [[nodiscard]] bool isTabTransferEnabled() const noexcept;

    /**
     * @brief 设置是否接受进程内标签转移。
     * @param enabled 是否接受。
     */
    void setTabTransferEnabled(bool enabled);

    /**
     * @brief 返回稳定的新建标签按钮。
     * @return 由标签栏或宿主标签控件管理的按钮指针。
     */
    [[nodiscard]] QWidget *newTabButton() const noexcept;

Q_SIGNALS:
    /**
     * @brief 拖出能力变化后发出。
     * @param enabled 新状态。
     */
    void tearOffEnabledChanged(bool enabled);

    /**
     * @brief 标签转移接收能力变化后发出。
     * @param enabled 新状态。
     */
    void tabTransferEnabledChanged(bool enabled);

    /**
     * @brief 标签释放到兼容目标外时发出，不改变页面所有权。
     * @param index 发出信号时的来源逻辑索引。
     * @param globalPosition 建议的新宿主屏幕位置。
     */
    void tearOffRequested(int index, const QPoint &globalPosition);

    /** @brief 请求创建新标签页。 */
    void newTabRequested();

    /**
     * @brief 请求关闭当前标签之外的标签。
     * @param index 当前标签索引。
     */
    void closeOtherTabsRequested(int index);

    /**
     * @brief 请求关闭当前标签右侧的标签。
     * @param index 当前标签索引。
     */
    void closeTabsToRightRequested(int index);

protected:
    /** @brief 记录公开 tabAt() 命中的按下标签。 */
    void mousePressEvent(QMouseEvent *event) override;

    /** @brief 超过平台拖拽阈值后启动一次 Qt MoveAction。 */
    void mouseMoveEvent(QMouseEvent *event) override;

    /** @brief 清理尚未开始拖拽的按下状态。 */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /** @brief 仅接受有效进程内标签载荷。 */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /** @brief 更新按布局方向计算的插入位置。 */
    void dragMoveEvent(QDragMoveEvent *event) override;

    /** @brief 将有效标签移动到当前宿主。 */
    void dropEvent(QDropEvent *event) override;

    /** @brief 离开目标时清除插入位置。 */
    void dragLeaveEvent(QDragLeaveEvent *event) override;

    /** @brief 绘制轻量插入指示线并保留 Qt 标签绘制。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 弹出标签操作上下文菜单。 */
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    friend class ZzTabWidget;
    std::unique_ptr<ZzTabBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
