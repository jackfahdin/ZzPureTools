#pragma once

#include <memory>

#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzDrawerEdge.h>
#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace ZzFluentUI {

class ZzDrawerPrivate;

/**
 * @brief 提供覆盖父控件内容区的临时 Fluent 边缘抽屉。
 *
 * Drawer 是普通 QWidget 子控件，不创建平台顶层窗口。模态模式阻止面板外
 * 输入并支持点击遮罩关闭；非模态模式只占用当前面板区域，面板外输入继续
 * 交给宿主。组件只管理本地展示、焦点和内容所有权，不访问业务模型。
 *
 * setContentWidget() 接管内容所有权；调用方需要保留内容时，必须先调用
 * takeContentWidget()。应使用 openDrawer()/closeDrawer() 改变展示状态。
 */
class ZZ_FLUENT_UI_EXPORT ZzDrawer final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzDrawer)
    Q_PROPERTY(
        ZzFluentUI::ZzDrawerEdge edge
        READ edge
        WRITE setEdge
        NOTIFY edgeChanged)
    Q_PROPERTY(
        bool modal
        READ isModal
        WRITE setModal
        NOTIFY modalChanged)
    Q_PROPERTY(
        int widthHint
        READ widthHint
        WRITE setWidthHint
        NOTIFY widthHintChanged)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(
        QWidget *contentWidget
        READ contentWidget
        WRITE setContentWidget
        NOTIFY contentWidgetChanged)

public:
    /**
     * @brief 创建默认从左侧打开的模态抽屉。
     * @param parent 待覆盖的 QWidget 宿主；为空时打开操作安全无效。
     */
    explicit ZzDrawer(QWidget *parent = nullptr);

    /** @brief 停止过渡、移除事件过滤并销毁仍拥有的内容。 */
    ~ZzDrawer() override;

    /** @brief 返回当前物理边缘。 */
    [[nodiscard]] ZzDrawerEdge edge() const noexcept;

    /**
     * @brief 设置物理边缘，Left/Right 不随 RTL 反转。
     * @param edge 新边缘。
     */
    void setEdge(ZzDrawerEdge edge);

    /** @brief 返回面板外输入是否由 Drawer 拦截。 */
    [[nodiscard]] bool isModal() const noexcept;

    /**
     * @brief 设置模态输入；运行中切换会立即更新遮罩和焦点约束。
     * @param modal 为 true 时覆盖并拦截完整宿主区域。
     */
    void setModal(bool modal);

    /** @brief 返回逻辑宽度提示；0 表示使用主题默认值。 */
    [[nodiscard]] int widthHint() const noexcept;

    /**
     * @brief 设置面板逻辑宽度提示。
     * @param logicalWidth 0 使用主题默认值；负值归零，正值最大为 4096。
     */
    void setWidthHint(int logicalWidth);

    /** @brief 返回逻辑打开状态，关闭动画期间返回 false。 */
    [[nodiscard]] bool isOpen() const noexcept;

    /** @brief 返回当前由 Drawer 拥有的内容控件。 */
    [[nodiscard]] QWidget *contentWidget() const noexcept;

    /**
     * @brief 接管新内容并删除被替换内容。
     * @param widget 可为空；非空对象会重挂到固定面板宿主。
     */
    void setContentWidget(QWidget *widget);

    /**
     * @brief 解除当前内容 parent 并把所有权交回调用方。
     * @return 原内容；为空表示当前没有内容。
     */
    [[nodiscard]] QWidget *takeContentWidget();

public Q_SLOTS:
    /** @brief 覆盖父控件、保存焦点并从当前进度打开。 */
    void openDrawer();

    /** @brief 从当前进度关闭，完成后隐藏并恢复先前焦点。 */
    void closeDrawer();

Q_SIGNALS:
    /** @brief 物理边缘实际变化后发出。 */
    void edgeChanged(ZzDrawerEdge edge);

    /** @brief 模态输入状态实际变化后发出。 */
    void modalChanged(bool modal);

    /** @brief 逻辑宽度提示实际变化后发出。 */
    void widthHintChanged(int logicalWidth);

    /** @brief 逻辑打开状态实际变化后发出。 */
    void openChanged(bool open);

    /** @brief 内容所有权实际变化后发出。 */
    void contentWidgetChanged(QWidget *widget);

protected:
    /** @brief 监听 ParentChange 并迁移固定宿主事件过滤器。 */
    bool event(QEvent *event) override;

    /** @brief 绘制按进度淡入的遮罩和边缘面板表面。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在主题、字体或 DPR 变化后刷新面板几何。 */
    void changeEvent(QEvent *event) override;

    /** @brief 面板焦点链内按 Escape 时关闭。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 模态模式下单击面板外遮罩时关闭。 */
    void mousePressEvent(QMouseEvent *event) override;

    /** @brief 外部隐藏时停止动画、同步状态并恢复焦点。 */
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzDrawerPrivate> d_ptr;
};

} // namespace ZzFluentUI
