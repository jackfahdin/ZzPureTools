#ifndef ZZPURETOOLS_WIDGETS_ZZCOMBOBOX_HPP
#define ZZPURETOOLS_WIDGETS_ZZCOMBOBOX_HPP

#include <QComboBox>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Style/ZzStyleDelegate.hpp"

class ZzAnimator;
class ZzComboBoxDelegate;
class ZzComboBoxItemDelegate;

/**
 * @brief Fluent UI 风格下拉框控件。
 *
 * 继承自 QComboBox，重写绘制逻辑以提供 Fluent UI 风格外观，
 * 支持悬停、按下动画以及弹出列表项的主题化渲染。
 */
class ZZ_PURE_TOOLS_EXPORT ZzComboBox : public QComboBox
{
    Q_OBJECT

public:
    /**
     * @brief 构造下拉框。
     * @param parent 父控件，默认为 nullptr。
     */
    explicit ZzComboBox(QWidget* parent = nullptr);

    /// 析构函数。
    ~ZzComboBox() override;

protected:
    /// 重写绘制事件，实现 Fluent UI 风格渲染。
    void paintEvent(QPaintEvent* event) override;

    /// 重写弹出显示逻辑，同步样式与动画状态。
    void showPopup() override;

    /// 重写弹出隐藏逻辑，同步样式与动画状态。
    void hidePopup() override;

    /// 鼠标进入事件，触发悬停动画。
    void enterEvent(QEnterEvent* event) override;

    /// 鼠标离开事件，触发悬停动画。
    void leaveEvent(QEvent* event) override;

    /// 尺寸变化事件，刷新弹出层样式。
    void resizeEvent(QResizeEvent* event) override;

    /// 状态变化事件，响应主题或启用状态变化。
    void changeEvent(QEvent* event) override;

private:
    /// 初始化控件及内部代理对象。
    void initialize();

    /// 绑定全局主题变化信号。
    void bindTheme();

    /// 刷新弹出列表的样式。
    void refreshPopupStyle();

    /**
     * @brief 构建当前绘制上下文。
     * @return 绘制上下文对象。
     */
    [[nodiscard]] ZzStyleContext buildStyleContext() const;

    bool m_popupVisible = false;              ///< 弹出层当前是否可见。
    ZzAnimator* m_animator = nullptr;         ///< 交互动画驱动。
    ZzComboBoxDelegate* m_delegate = nullptr;     ///< 主体样式绘制代理。
    ZzComboBoxItemDelegate* m_itemDelegate = nullptr; ///< 弹出项样式绘制代理。
};

#endif // ZZPURETOOLS_WIDGETS_ZZCOMBOBOX_HPP
