#pragma once

#include <memory>

#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QKeyEvent;
class QMouseEvent;

namespace ZzFluentUI {

class ZzSplitButtonPrivate;

/**
 * @brief 在单个 QPushButton 中提供主命令区和菜单打开区。
 *
 * 主区保留 QPushButton 的 click、checkable、default、shortcut 和无障碍
 * 按钮语义；逻辑 trailing 菜单区只发出打开意图，不改变 checked，并借用
 * 调用方提供的 QMenu。控件不拥有菜单、QAction 或业务命令。
 */
class ZZ_FLUENT_UI_EXPORT ZzSplitButton final : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSplitButton)
    Q_PROPERTY(
        ZzFluentUI::ZzButtonAppearance appearance
        READ appearance
        WRITE setAppearance
        NOTIFY appearanceChanged)
    Q_PROPERTY(
        QMenu *menu
        READ menu
        WRITE setMenu
        NOTIFY menuChanged)

public:
    /**
     * @brief 创建无文本的分割按钮。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzSplitButton(QWidget *parent = nullptr);

    /**
     * @brief 创建显示指定文本的分割按钮。
     * @param text 可本地化的主命令文本。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzSplitButton(
        const QString &text,
        QWidget *parent = nullptr);

    /** @brief 销毁私有命中状态，不销毁借用的菜单。 */
    ~ZzSplitButton() override;

    /**
     * @brief 返回当前视觉强调级别。
     * @return Standard、Accent 或 Subtle。
     */
    [[nodiscard]] ZzButtonAppearance appearance() const noexcept;

    /**
     * @brief 设置视觉强调级别，重复值不发信号。
     * @param appearance 新外观。
     */
    void setAppearance(ZzButtonAppearance appearance);

    /**
     * @brief 返回当前借用的菜单。
     * @return 菜单已销毁或尚未设置时返回 nullptr。
     */
    [[nodiscard]] QMenu *menu() const noexcept;

    /**
     * @brief 借用菜单供 trailing 区打开，不接管所有权。
     * @param menu 可为空；生命周期由调用方管理。
     */
    void setMenu(QMenu *menu);

    /**
     * @brief 返回包含主命令和菜单命中区的建议尺寸。
     * @return 不小于 Fluent 最小高度的逻辑像素尺寸。
     */
    [[nodiscard]] QSize sizeHint() const override;

public Q_SLOTS:
    /**
     * @brief 请求调用方准备菜单，并在按钮下方非阻塞打开。
     *
     * 控件禁用时不发请求；未设置菜单时只发 menuRequested，允许
     * 调用方在该信号处理中同步设置菜单。
     */
    void showMenu();

Q_SIGNALS:
    /**
     * @brief 视觉强调级别实际变化后发出。
     * @param appearance 新外观。
     */
    void appearanceChanged(ZzButtonAppearance appearance);

    /**
     * @brief 借用菜单发生变化或被外部销毁后发出。
     * @param menu 新菜单；外部销毁时为 nullptr。
     */
    void menuChanged(QMenu *menu);

    /** @brief 用户或调用方请求打开菜单时发出。 */
    void menuRequested();

protected:
    /** @brief 绘制单一 Fluent 表面、分隔线、主标签和下拉图标。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 仅把逻辑 leading 主区交给 QPushButton 激活状态机。 */
    [[nodiscard]] bool hitButton(const QPoint &position) const override;

    /** @brief 更新两个命中区的悬停与拖动状态。 */
    void mouseMoveEvent(QMouseEvent *event) override;

    /** @brief 把左键按下分派到主命令区或菜单区。 */
    void mousePressEvent(QMouseEvent *event) override;

    /** @brief 仅在原命中区内释放时执行对应动作。 */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /** @brief 离开控件时清除悬停和菜单 armed 状态。 */
    void leaveEvent(QEvent *event) override;

    /** @brief 处理 Down、Alt+Down 和 Enter，其余按键沿用 QPushButton。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 在主题、方向、字体或启用状态变化后刷新派生展示。 */
    void changeEvent(QEvent *event) override;

private:
    friend class ZzSplitButtonPrivate;
    std::unique_ptr<ZzSplitButtonPrivate> d_ptr;
};

} // namespace ZzFluentUI
