#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzTeachingTipPlacement.h>

class QEvent;
class QHideEvent;
class QKeyEvent;
class QPaintEvent;
class QShowEvent;

namespace ZzFluentUI {

class ZzTeachingTipPrivate;

/**
 * @brief 在目标控件附近显示非模态说明、内容和操作意图。
 *
 * targetWidget 始终为非拥有观察指针；自定义 contentWidget 默认由提示接管。
 * 控件只发出操作意图，不读取业务模型，也不会因 Action 自动关闭。
 */
class ZZ_FLUENT_UI_EXPORT ZzTeachingTip final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzTeachingTip)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(
        QWidget *contentWidget
        READ contentWidget
        WRITE setContentWidget
        NOTIFY contentWidgetChanged)
    Q_PROPERTY(
        QWidget *targetWidget
        READ targetWidget
        WRITE setTargetWidget
        NOTIFY targetWidgetChanged)
    Q_PROPERTY(
        ZzTeachingTipPlacement preferredPlacement
        READ preferredPlacement
        WRITE setPreferredPlacement
        NOTIFY preferredPlacementChanged)
    Q_PROPERTY(
        ZzTeachingTipPlacement effectivePlacement
        READ effectivePlacement
        NOTIFY effectivePlacementChanged)
    Q_PROPERTY(
        bool lightDismissEnabled
        READ isLightDismissEnabled
        WRITE setLightDismissEnabled
        NOTIFY lightDismissEnabledChanged)
    Q_PROPERTY(
        QString actionText
        READ actionText
        WRITE setActionText
        NOTIFY actionTextChanged)
    Q_PROPERTY(
        bool actionEnabled
        READ isActionEnabled
        WRITE setActionEnabled
        NOTIFY actionEnabledChanged)
    Q_PROPERTY(
        bool actionVisible
        READ isActionVisible
        WRITE setActionVisible
        NOTIFY actionVisibleChanged)
    Q_PROPERTY(
        bool closeButtonVisible
        READ isCloseButtonVisible
        WRITE setCloseButtonVisible
        NOTIFY closeButtonVisibleChanged)

public:
    /**
     * @brief 创建尚未绑定目标的无边框工具提示窗口。
     * @param parent 可为空的 QObject 所有者，不改变提示的顶层窗口身份。
     */
    explicit ZzTeachingTip(QWidget *parent = nullptr);

    /** @brief 注销目标与应用事件过滤器并销毁固定动画对象。 */
    ~ZzTeachingTip() override;

    /** @brief 返回标题文本。 */
    [[nodiscard]] QString title() const;

    /** @brief 设置标题文本；重复设置不发信号。 */
    void setTitle(QString title);

    /** @brief 返回正文文本。 */
    [[nodiscard]] QString text() const;

    /** @brief 设置可换行正文；重复设置不发信号。 */
    void setText(QString text);

    /** @brief 返回当前由提示拥有的自定义内容。 */
    [[nodiscard]] QWidget *contentWidget() const noexcept;

    /** @brief 接管新内容并删除被替换的旧内容。 */
    void setContentWidget(QWidget *widget);

    /** @brief 解除内容 parent 并把所有权交回调用者。 */
    [[nodiscard]] QWidget *takeContentWidget();

    /** @brief 返回非拥有的目标控件。 */
    [[nodiscard]] QWidget *targetWidget() const noexcept;

    /** @brief 设置非拥有目标并监听其几何、可见性和屏幕生命周期。 */
    void setTargetWidget(QWidget *target);

    /** @brief 返回候选定位顺序的首选方向。 */
    [[nodiscard]] ZzTeachingTipPlacement preferredPlacement() const noexcept;

    /** @brief 设置首选方向；Auto 依次尝试下、上、右、左。 */
    void setPreferredPlacement(ZzTeachingTipPlacement placement);

    /** @brief 返回最近一次成功定位采用的实际方向。 */
    [[nodiscard]] ZzTeachingTipPlacement effectivePlacement() const noexcept;

    /** @brief 返回是否点击提示和目标之外区域时关闭。 */
    [[nodiscard]] bool isLightDismissEnabled() const noexcept;

    /** @brief 设置轻量关闭策略并同步应用事件过滤器。 */
    void setLightDismissEnabled(bool enabled);

    /** @brief 返回 Action 按钮文本。 */
    [[nodiscard]] QString actionText() const;

    /** @brief 设置 Action 按钮文本。 */
    void setActionText(QString text);

    /** @brief 返回 Action 是否启用。 */
    [[nodiscard]] bool isActionEnabled() const noexcept;

    /** @brief 设置 Action 是否启用。 */
    void setActionEnabled(bool enabled);

    /** @brief 返回 Action 是否可见。 */
    [[nodiscard]] bool isActionVisible() const noexcept;

    /** @brief 设置 Action 是否可见。 */
    void setActionVisible(bool visible);

    /** @brief 返回关闭图标按钮是否可见。 */
    [[nodiscard]] bool isCloseButtonVisible() const noexcept;

    /** @brief 设置关闭图标按钮是否可见。 */
    void setCloseButtonVisible(bool visible);

    /** @brief 校验目标、计算屏幕内位置并显示或反转当前隐藏动画。 */
    void showForTarget();

    /** @brief 通过复用动画关闭，并在完成后发出一次 dismissed。 */
    void dismiss();

Q_SIGNALS:
    /** @brief 标题实际变化后发出。 */
    void titleChanged(const QString &title);

    /** @brief 正文实际变化后发出。 */
    void textChanged(const QString &text);

    /** @brief 内容所有权实际变化后发出。 */
    void contentWidgetChanged(QWidget *widget);

    /** @brief 非拥有目标实际变化或被销毁后发出。 */
    void targetWidgetChanged(QWidget *target);

    /** @brief 首选方向实际变化后发出。 */
    void preferredPlacementChanged(ZzTeachingTipPlacement placement);

    /** @brief 定位算法选择的实际方向变化后发出。 */
    void effectivePlacementChanged(ZzTeachingTipPlacement placement);

    /** @brief 轻量关闭策略实际变化后发出。 */
    void lightDismissEnabledChanged(bool enabled);

    /** @brief Action 文本实际变化后发出。 */
    void actionTextChanged(const QString &text);

    /** @brief Action 启用状态实际变化后发出。 */
    void actionEnabledChanged(bool enabled);

    /** @brief Action 可见性实际变化后发出。 */
    void actionVisibleChanged(bool visible);

    /** @brief 关闭按钮可见性实际变化后发出。 */
    void closeButtonVisibleChanged(bool visible);

    /** @brief 用户触发 Action 时发出；提示保持可见。 */
    void actionTriggered();

    /** @brief 一次关闭流程完成且窗口已隐藏后发出。 */
    void dismissed();

protected:
    /** @brief 使用主题浮层和目标指向箭头绘制提示。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在语言、主题、字体和 DPR 变化时刷新展示与定位。 */
    void changeEvent(QEvent *event) override;

    /** @brief 将非自动重复 Escape 映射为关闭。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 可见时按策略注册应用级轻量关闭过滤器。 */
    void showEvent(QShowEvent *event) override;

    /** @brief 隐藏时无条件注销应用级过滤器并收敛动画状态。 */
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzTeachingTipPrivate> d_ptr;
};

} // namespace ZzFluentUI
