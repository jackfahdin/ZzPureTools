#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzSidePaneMode.h>

class QEvent;

namespace ZzFluentUI {

class ZzSidePanePrivate;
class ZzPanelStack;

/**
 * @brief 提供标题、页面堆栈、折叠和物理边缘宽度调整的可复用侧面板。
 *
 * addWidget() 只接管无父对象页面；takeWidget() 解除父子关系并归还所有权。
 * 因此失败的页面转移不会改变原有 QWidget 的父子关系。
 */
class ZZ_FLUENT_UI_EXPORT ZzSidePane final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSidePane)
    Q_PROPERTY(
        ZzFluentUI::ZzSidePaneEdge edge
        READ edge
        WRITE setEdge
        NOTIFY edgeChanged)
    Q_PROPERTY(bool collapsed READ isCollapsed WRITE setCollapsed NOTIFY collapsedChanged)
    Q_PROPERTY(int paneWidth READ paneWidth WRITE setPaneWidth NOTIFY paneWidthChanged)
    Q_PROPERTY(ZzFluentUI::ZzSidePaneMode mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    /**
     * @brief 创建默认位于左侧、未折叠的 Side Pane。
     * @param edge 物理边缘；不随 RTL 变化。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzSidePane(
        ZzSidePaneEdge edge = ZzSidePaneEdge::Left,
        QWidget *parent = nullptr);

    /** @brief 销毁固定标题区、页面堆栈、把手及仍被接管的页面。 */
    ~ZzSidePane() override;

    /** @brief 返回物理边缘。 */
    [[nodiscard]] ZzSidePaneEdge edge() const noexcept;

    /** @brief 切换物理边缘并移动固定 4 px 把手，不改变页面所有权。 */
    void setEdge(ZzSidePaneEdge edge);

    /** @brief 返回当前已注册页面数量。 */
    [[nodiscard]] int pageCount() const noexcept;

    /** @brief 返回当前页面；没有页面时返回 nullptr。 */
    [[nodiscard]] QWidget *currentWidget() const noexcept;

    /** @brief 返回固定的多面板容器；调用方不得转移其所有权。 */
    [[nodiscard]] ZzPanelStack *panelStack() const noexcept;

    /** @brief 返回按稳定布局顺序排列的逻辑可见页面。 */
    [[nodiscard]] QList<QWidget *> visibleWidgets() const;

    /** @brief 返回当前页面展示模式。 */
    [[nodiscard]] ZzSidePaneMode mode() const noexcept;

    /** @brief 切换单页或多页堆叠模式，并保留多页模式的可见集合。 */
    void setMode(ZzSidePaneMode mode);

    /**
     * @brief 接管一个无父对象页面并使其成为当前页。
     * @param widget 必须非空且没有 QObject 父对象。
     * @param title 显示在固定标题区的页面标题。
     * @return 页面接管成功时为 true；失败时 widget 所有权和当前页保持不变。
     */
    bool addWidget(QWidget *widget, const QString &title);

    /**
     * @brief 从页面堆栈移除并归还页面所有权。
     * @param widget 必须是当前已注册页面。
     * @return 已解除父对象的页面；未注册时返回 nullptr。
     */
    [[nodiscard]] QWidget *takeWidget(QWidget *widget);

    /**
     * @brief 切换到已注册页面。
     * @param widget 必须为当前堆栈中的页面。
     * @return 页面存在时为 true；失败不会改变当前页。
     */
    bool setCurrentWidget(QWidget *widget);

    /** @brief 设置已注册页面的逻辑可见性。 */
    bool setWidgetVisible(QWidget *widget, bool visible);

    /** @brief 返回面板是否已折叠隐藏。 */
    [[nodiscard]] bool isCollapsed() const noexcept;

    /**
     * @brief 折叠或恢复面板；恢复时使用最后一次合法展开宽度。
     * @param collapsed 为 true 时隐藏面板但保留展开宽度。
     */
    void setCollapsed(bool collapsed);

    /** @brief 返回当前最小展开宽度。 */
    [[nodiscard]] int minimumPaneWidth() const noexcept;

    /** @brief 设置最小展开宽度，并钳制当前与最近展开宽度。 */
    void setMinimumPaneWidth(int width);

    /** @brief 返回当前最大展开宽度。 */
    [[nodiscard]] int maximumPaneWidth() const noexcept;

    /** @brief 设置最大展开宽度，并钳制当前与最近展开宽度。 */
    void setMaximumPaneWidth(int width);

    /** @brief 返回当前或最近一次展开时的合法宽度。 */
    [[nodiscard]] int paneWidth() const noexcept;

    /** @brief 设置展开宽度，自动钳制到最小和最大范围。 */
    void setPaneWidth(int width);

    /** @brief 返回折叠前记录的合法展开宽度。 */
    [[nodiscard]] int lastExpandedWidth() const noexcept;

Q_SIGNALS:
    /** @brief 物理边缘变化后发出。 */
    void edgeChanged(ZzSidePaneEdge edge);

    /** @brief 当前页面变化或被销毁后发出。 */
    void currentWidgetChanged(QWidget *widget);

    /** @brief 折叠状态实际变化后发出。 */
    void collapsedChanged(bool collapsed);

    /** @brief 最小展开宽度变化后发出。 */
    void minimumPaneWidthChanged(int width);

    /** @brief 最大展开宽度变化后发出。 */
    void maximumPaneWidthChanged(int width);

    /** @brief 当前或最近展开宽度变化后发出。 */
    void paneWidthChanged(int width);

    /** @brief 页面展示模式实际变化后发出。 */
    void modeChanged(ZzSidePaneMode mode);

protected:
    /** @brief 以 O(1) 方式处理固定 4 px 把手的拖拽。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class ZzSidePanePrivate;
    std::unique_ptr<ZzSidePanePrivate> d_ptr;
};

} // namespace ZzFluentUI
