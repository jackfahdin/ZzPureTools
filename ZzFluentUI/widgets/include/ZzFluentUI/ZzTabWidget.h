#pragma once

#include <memory>

#include <QtWidgets/QTabWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzTabBar;
class ZzTabWidgetPrivate;

/**
 * @brief 保存标签页并提供同步、可回滚的容器间转移。
 *
 * 控件不会创建顶层窗口，也不会在关闭请求中删除页面。应用层可以在
 * tearOffRequested 信号中创建新宿主，再调用 transferTabTo 完成移动。
 */
class ZZ_FLUENT_UI_EXPORT ZzTabWidget final : public QTabWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzTabWidget)

public:
    /**
     * @brief 创建使用 ZzTabBar 的可移动标签容器。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzTabWidget(QWidget *parent = nullptr);

    /** @brief 销毁容器及仍由容器拥有的页面。 */
    ~ZzTabWidget() override;

    /**
     * @brief 返回本容器拥有的公开标签栏。
     * @return 生命周期与本容器一致的非空指针。
     */
    [[nodiscard]] ZzTabBar *fluentTabBar() const noexcept;

    /**
     * @brief 将指定标签同步移动到目标容器。
     * @param target 目标标签容器。
     * @param sourceIndex 来源逻辑索引。
     * @param targetIndex 目标插入槽位，负数表示末尾。
     * @return 成功移动或完成同容器重排时返回 true。
     */
    bool transferTabTo(
        ZzTabWidget *target,
        int sourceIndex,
        int targetIndex = -1);

Q_SIGNALS:
    /**
     * @brief 请求调用方为仍在本容器中的页面创建新宿主。
     * @param index 发出信号时的来源索引。
     * @param page 仍由本容器拥有的页面，仅供同步识别。
     * @param globalPosition 建议的新宿主屏幕位置。
     */
    void tearOffRequested(
        int index,
        QWidget *page,
        const QPoint &globalPosition);

    /**
     * @brief 页面成功移入本容器后发出。
     * @param source 来源容器。
     * @param sourceIndex 转移开始时的来源索引。
     * @param targetIndex 实际目标索引。
     * @param page 已由本容器拥有的页面。
     */
    void tabTransferred(
        ZzTabWidget *source,
        int sourceIndex,
        int targetIndex,
        QWidget *page);

private:
    std::unique_ptr<ZzTabWidgetPrivate> d_ptr;
};

} // namespace ZzFluentUI
