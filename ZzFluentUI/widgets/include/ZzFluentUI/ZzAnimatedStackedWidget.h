#pragma once

#include <memory>
#include <QtWidgets/QWidget>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzAnimatedStackedWidgetPrivate;

/**
 * @brief 托管单页可见的 QWidget 集合并提供有界快照切换。
 * @details 仅在 GUI 线程使用；接管无父对象或以本容器为父对象的页面。
 * currentChanged 表示逻辑提交，transitionFinished 表示真实页面恢复展示。
 */
class ZZ_FLUENT_UI_EXPORT ZzAnimatedStackedWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzAnimatedStackedWidget)
public:
    /** @brief 选择无动画、水平／垂直滑动或渐变。 */
    enum class ZzTransitionMode { None, SlideHorizontal, SlideVertical, Fade };
    Q_ENUM(ZzTransitionMode)
    /** @brief 指定逻辑方向；水平前进方向随 RTL 镜像。 */
    enum class ZzTransitionDirection { Automatic, Forward, Backward };
    Q_ENUM(ZzTransitionDirection)

    /** @brief 创建空页面容器，默认使用主题时长和水平滑动。 */
    explicit ZzAnimatedStackedWidget(QWidget *parent = nullptr);
    /** @brief 清理观察与动画后销毁仍拥有的页面。 */
    ~ZzAnimatedStackedWidget() override;
    /** @brief 返回全部注册页的建议尺寸上界，避免切页时外部布局跳动。 */
    QSize sizeHint() const override;
    /** @brief 返回全部注册页的最小建议尺寸上界。 */
    QSize minimumSizeHint() const override;
    /** @brief 返回已注册页面数量。 */
    [[nodiscard]] int count() const noexcept;
    /** @brief 返回索引处的页，越界返回 nullptr。 */
    [[nodiscard]] QWidget *widget(int index) const noexcept;
    /** @brief 返回页面索引，未注册返回 -1。 */
    [[nodiscard]] int indexOf(QWidget *page) const noexcept;
    /** @brief 返回逻辑当前索引，空容器为 -1。 */
    [[nodiscard]] int currentIndex() const noexcept;
    /** @brief 返回逻辑当前页，可能仍在呈现该页的入场快照。 */
    [[nodiscard]] QWidget *currentWidget() const noexcept;
    /** @brief 接管页面并追加，重复添加返回原索引，失败返回 -1。 */
    int addWidget(QWidget *page);
    /** @brief 在 [0,count()] 插入页面；失败不改变所有权。 */
    int insertWidget(int index, QWidget *page);
    /** @brief 移除、隐藏并归还无父对象页面；未注册或回调中销毁时返回 nullptr。 */
    [[nodiscard]] QWidget *takeWidget(QWidget *page);
    /** @brief 请求有效索引；动效期间只保留最后一个目标，返回值不表示动效已完成。 */
    bool setCurrentIndex(int index,
        ZzTransitionDirection direction = ZzTransitionDirection::Automatic);
    /** @brief 请求已注册页面，方向语义与 setCurrentIndex 一致。 */
    bool setCurrentWidget(QWidget *page,
        ZzTransitionDirection direction = ZzTransitionDirection::Automatic);
    /** @brief 返回当前动画模式。 */
    [[nodiscard]] ZzTransitionMode transitionMode() const noexcept;
    /** @brief 设置模式，拒绝未知枚举；运行中的动画直接收敛。 */
    void setTransitionMode(ZzTransitionMode mode);
    /** @brief 返回显式时长；-1 表示读取 PageTransition 主题 token。 */
    [[nodiscard]] int duration() const noexcept;
    /** @brief 将时长裁剪到 [-1,10000]；零直接切页。 */
    void setDuration(int milliseconds);
    /** @brief 返回是否存在尚未收敛的已启动动效。 */
    [[nodiscard]] bool isTransitionRunning() const noexcept;
    /** @brief 为注册页显式允许或禁用快照，适用于视频等动态渲染页面。 */
    bool setPageAnimationEnabled(QWidget *page, bool enabled);
    /** @brief 查询页面快照开关，未注册页返回 false。 */
    [[nodiscard]] bool isPageAnimationEnabled(QWidget *page) const noexcept;

Q_SIGNALS:
    /** @brief 逻辑当前索引或页面身份改变后发出。 */
    void currentChanged(int index);
    /** @brief 注册页面数量改变后发出。 */
    void countChanged(int count);
    /** @brief 快照状态安装完成后发出，每段最多一次。 */
    void transitionStarted();
    /** @brief 每个已开始段恢复真实页并释放图像后发出。 */
    void transitionFinished();

protected:
    /** @brief 跟踪影响快照有效性的环境与几何变化。 */
    bool event(QEvent *event) override;

private:
    std::unique_ptr<ZzAnimatedStackedWidgetPrivate> d_ptr;
};

} // namespace ZzFluentUI
