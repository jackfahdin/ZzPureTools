#pragma once

#include <memory>
#include <QtCore/QList>
#include <QtCore/QString>

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

    /** @brief 返回标签是否固定。 */
    [[nodiscard]] bool isTabPinned(int index) const;
    /** @brief 设置标签固定状态。 */
    void setTabPinned(int index, bool pinned);
    /** @brief 返回标签是否有未保存修改。 */
    [[nodiscard]] bool isTabModified(int index) const;
    /** @brief 设置标签脏状态。 */
    void setTabModified(int index, bool modified);
    /** @brief 返回标签是否需要注意提示。 */
    [[nodiscard]] bool hasTabAttention(int index) const;
    /** @brief 设置标签注意提示状态。 */
    void setTabAttention(int index, bool attention);
    /** @brief 返回标签是否允许关闭。 */
    [[nodiscard]] bool isTabCloseEnabled(int index) const;
    /** @brief 设置标签关闭能力。 */
    void setTabCloseEnabled(int index, bool enabled);
    /** @brief 同步页面 windowTitle 与标签文本。 */
    void setPageTitle(int index, const QString &title);
    /** @brief 按页面指针同步页面标题。 */
    void setPageTitle(QWidget *page, const QString &title);
    /** @brief 发出关闭其他可关闭标签的意图。 */
    void closeOtherTabs(int index);
    /** @brief 发出关闭右侧可关闭标签的意图。 */
    void closeTabsToRight(int index);

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

    /** @brief 标签固定状态变化。 */
    void tabPinnedChanged(int index, bool pinned);
    /** @brief 标签脏状态变化。 */
    void tabModifiedChanged(int index, bool modified);
    /** @brief 标签注意状态变化。 */
    void tabAttentionChanged(int index, bool attention);
    /** @brief 标签关闭能力变化。 */
    void tabCloseEnabledChanged(int index, bool enabled);
    /** @brief 请求创建新标签页。 */
    void newTabRequested();
    /** @brief 请求调用方批量关闭给定页面，控件不删除页面。 */
    void tabsCloseRequested(const QList<QWidget *> &pages);

private:
    friend class ZzTabWidgetPrivate;
    std::unique_ptr<ZzTabWidgetPrivate> d_ptr;
};

} // namespace ZzFluentUI
