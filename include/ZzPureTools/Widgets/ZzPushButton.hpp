#ifndef ZZPURETOOLS_WIDGETS_ZZPUSHBUTTON_HPP
#define ZZPURETOOLS_WIDGETS_ZZPUSHBUTTON_HPP

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Style/ZzStyleDelegate.hpp"

#include <QPushButton>

class ZzAnimator;
class ZzPushButtonDelegate;

/**
 * @brief Fluent UI 风格按钮控件。
 *
 * 继承自 QPushButton，提供标准（Standard）和强调（Accent）两种预设样式，
 * 支持悬停、按下动画以及主题切换时的视觉同步。
 */
class ZZ_PURE_TOOLS_EXPORT ZzPushButton : public QPushButton
{
    Q_OBJECT

public:
    /// 按钮样式类型。
    enum class ZzButtonStyle
    {
        Standard,  ///< 标准样式。
        Accent,    ///< 强调样式。
    };
    Q_ENUM(ZzButtonStyle)

    /**
     * @brief 构造按钮。
     * @param parent 父控件，默认为 nullptr。
     */
    explicit ZzPushButton(QWidget* parent = nullptr);

    /**
     * @brief 构造带有文本的按钮。
     * @param text 按钮文本。
     * @param parent 父控件，默认为 nullptr。
     */
    explicit ZzPushButton(const QString& text, QWidget* parent = nullptr);

    /// 析构函数。
    ~ZzPushButton() override;

    /**
     * @brief 设置按钮样式。
     * @param style 目标按钮样式。
     */
    void setButtonStyle(ZzButtonStyle style);

    /**
     * @brief 获取当前按钮样式。
     * @return 当前按钮样式。
     */
    [[nodiscard]] ZzButtonStyle buttonStyle() const noexcept;

protected:
    /// 重写绘制事件，实现 Fluent UI 风格渲染。
    void paintEvent(QPaintEvent* event) override;

    /// 鼠标进入事件，触发悬停动画。
    void enterEvent(QEnterEvent* event) override;

    /// 鼠标离开事件，触发悬停动画。
    void leaveEvent(QEvent* event) override;

    /// 鼠标按下事件，触发按下动画。
    void mousePressEvent(QMouseEvent* event) override;

    /// 鼠标释放事件，触发按下动画。
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// 状态变化事件，响应主题或启用状态变化。
    void changeEvent(QEvent* event) override;

private:
    /// 初始化控件及内部代理对象。
    void initialize();

    /// 绑定全局主题变化信号。
    void bindTheme();

    /**
     * @brief 构建当前绘制上下文。
     * @return 绘制上下文对象。
     */
    [[nodiscard]] ZzStyleContext buildStyleContext() const;

    ZzButtonStyle m_buttonStyle = ZzButtonStyle::Standard;  ///< 当前按钮样式。
    ZzAnimator* m_animator = nullptr;                       ///< 交互动画驱动。
    ZzPushButtonDelegate* m_delegate = nullptr;             ///< 样式绘制代理。
};

#endif  // ZZPURETOOLS_WIDGETS_ZZPUSHBUTTON_HPP
