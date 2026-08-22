#pragma once

#include <memory>

#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

class QEvent;

namespace ZzFluentUI {

class ZzBottomPanePrivate;

/**
 * @brief 提供可切换、可折叠且可调整高度的中央底部工具区。
 *
 * addWidget() 只接管无父对象的页面；takeWidget() 解除父子关系并归还所有权。
 * 关闭按钮只发送关闭意图，不会删除、隐藏或转移当前工具页面。
 */
class ZZ_FLUENT_UI_EXPORT ZzBottomPane final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzBottomPane)
    Q_PROPERTY(bool collapsed READ isCollapsed WRITE setCollapsed NOTIFY collapsedChanged)
    Q_PROPERTY(int paneHeight READ paneHeight WRITE setPaneHeight NOTIFY paneHeightChanged)

public:
    /**
     * @brief 创建未折叠的空中央底部工具区。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzBottomPane(QWidget *parent = nullptr);

    /** @brief 销毁固定标题区、页面堆栈、把手及仍被接管的工具页面。 */
    ~ZzBottomPane() override;

    /** @brief 返回当前已注册工具数量。 */
    [[nodiscard]] int widgetCount() const noexcept;

    /** @brief 返回当前工具；没有工具时返回 nullptr。 */
    [[nodiscard]] QWidget *currentWidget() const noexcept;

    /**
     * @brief 接管无父对象工具页面并使其成为当前工具。
     * @param widget 必须非空、无 QObject 父对象且尚未注册。
     * @param title Pivot 中展示的工具标题。
     * @param icon 通过 Foundation 图标描述生成的 Pivot 图标。
     * @return 接管成功时为 true；失败不改变页面所有权或当前工具。
     */
    bool addWidget(
        QWidget *widget,
        const QString &title,
        const ZzIconDescriptor &icon = {});

    /**
     * @brief 移除指定工具并归还其所有权。
     * @param widget 当前已注册工具。
     * @return 已解除父对象的工具；未注册时返回 nullptr。
     */
    [[nodiscard]] QWidget *takeWidget(QWidget *widget);

    /**
     * @brief 切换到指定已注册工具。
     * @param widget 当前工具区中已注册的页面。
     * @return 目标存在时为 true；失败不改变当前工具。
     */
    bool setCurrentWidget(QWidget *widget);

    /** @brief 返回工具区是否已折叠。 */
    [[nodiscard]] bool isCollapsed() const noexcept;

    /**
     * @brief 折叠或恢复工具区；恢复时使用最后一次合法展开高度。
     * @param collapsed 为 true 时隐藏内容区但保留工具和展开高度。
     */
    void setCollapsed(bool collapsed);

    /** @brief 返回最小展开高度。 */
    [[nodiscard]] int minimumPaneHeight() const noexcept;

    /** @brief 设置最小展开高度，并钳制当前与最近展开高度。 */
    void setMinimumPaneHeight(int height);

    /** @brief 返回最大展开高度。 */
    [[nodiscard]] int maximumPaneHeight() const noexcept;

    /** @brief 设置最大展开高度，并钳制当前与最近展开高度。 */
    void setMaximumPaneHeight(int height);

    /** @brief 返回当前或最近一次展开时的合法高度。 */
    [[nodiscard]] int paneHeight() const noexcept;

    /** @brief 设置展开高度，自动钳制到最小和最大范围。 */
    void setPaneHeight(int height);

    /** @brief 返回折叠前记录的合法展开高度。 */
    [[nodiscard]] int lastExpandedHeight() const noexcept;

Q_SIGNALS:
    /** @brief 当前工具变化或被外部销毁后发出。 */
    void currentWidgetChanged(QWidget *widget);

    /** @brief 当前工具的关闭按钮被激活时发出，不转移页面所有权。 */
    void widgetCloseRequested(QWidget *widget);

    /** @brief 折叠状态实际变化后发出。 */
    void collapsedChanged(bool collapsed);

    /** @brief 最小展开高度变化后发出。 */
    void minimumPaneHeightChanged(int height);

    /** @brief 最大展开高度变化后发出。 */
    void maximumPaneHeightChanged(int height);

    /** @brief 当前或最近展开高度变化后发出。 */
    void paneHeightChanged(int height);

protected:
    /** @brief 以 O(1) 方式处理固定 4 px 把手的垂直拖拽。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class ZzBottomPanePrivate;
    std::unique_ptr<ZzBottomPanePrivate> d_ptr;
};

} // namespace ZzFluentUI
