#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QHash>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/Qt>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtWidgets/QStyleOption>

#include "ZzStyleCache.h"

#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeChangeKind.h>

class QAbstractItemView;
class QProgressBar;
class QTabBar;
class QVariantAnimation;

namespace ZzFluentUI {

class ZzFluentStyle;
class ZzThemeController;
class ZzThemeSnapshot;
class ZzItemSelectionAnimation;
class ZzTabIndicatorAnimation;

/** @brief 菜单正文与快捷键列之间保留的最小逻辑像素间距。 */
inline constexpr int zzMenuShortcutSpacing = 12;

/** @brief 子菜单自绘 chevron 独占的尾部逻辑像素宽度。 */
inline constexpr int zzMenuTrailingIndicatorWidth = 28;

/** @brief 组合框标签区域的起始逻辑像素内边距。 */
inline constexpr int zzComboBoxLeadingInset = 12;

/** @brief 组合框箭头独占的尾部逻辑像素宽度。 */
inline constexpr int zzComboBoxArrowWidth = 32;

/** @brief 基础样式标签在编辑区域内消耗的水平逻辑像素。 */
inline constexpr int zzComboBoxLabelHorizontalMargin = 4;

/** @brief 持有主题快照、非拥有控制器引用和 Widgets 私有缓存。 */
class ZzFluentStylePrivate final
{
public:
    /** @brief 获取由样式拥有、随视图销毁回收的唯一选择动画。 */
    ZzItemSelectionAnimation *itemAnimation(QAbstractItemView *view);
    QHash<QAbstractItemView *, ZzItemSelectionAnimation *> itemAnimations;
    mutable QHash<QTabBar *, ZzTabIndicatorAnimation *> tabAnimations;
    /** @brief 绑定控制器并用首个快照初始化固定视觉槽。 */
    ZzFluentStylePrivate(
        ZzFluentStyle *q,
        ZzThemeController *controller);
    ~ZzFluentStylePrivate();

    /** @brief 在 GUI 线程执行有界 SVG 资源渲染和缓存。 */
    [[nodiscard]] QPixmap iconPixmap(
        const ZzIconDescriptor &descriptor,
        QSize logicalSize,
        qreal devicePixelRatio,
        QColor color,
        Qt::LayoutDirection direction);

    /** @brief 解析一次 SVG 或字体字形并生成与颜色无关的物理像素轮廓。 */
    [[nodiscard]] QImage renderIconShape(
        const ZzIconDescriptor &descriptor,
        QSize physicalSize,
        bool mirrored);

    /** @brief 同步新快照并按变更分类刷新绘制或几何。 */
    void applySnapshot(ZzThemeChangeKinds changes);

    /** @brief 根据应用输入事件更新键盘焦点视觉状态。 */
    void handleInputEvent(QObject *watched, QEvent *event);

    /** @brief 判断控件是否位于当前键盘焦点视觉层级。 */
    [[nodiscard]] bool isFocusVisualVisible(
        const QWidget *widget) const noexcept;

    /** @brief 以细线折角绘制导航树展开标记，保留原有分支位置与命中区域。 */
    void drawNavigationBranch(
        const QStyleOption *option,
        QPainter *painter) const;

    /** @brief 绘制复选框或单选框指示器。 */
    void drawCheckIndicator(
        const QStyleOption *option,
        QPainter *painter,
        bool radio) const;
    /** @brief 绘制按钮面板并委托平台样式绘制标签。 */
    void drawPushButton(
        const QStyleOptionButton *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制标准工具按钮的轻量交互表面。 */
    void drawToolButtonPanel(
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制标准工具栏背景和停靠方向边界。 */
    void drawToolBarPanel(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 使用固定六点绘制标准工具栏拖动柄。 */
    void drawToolBarHandle(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制与工具栏方向正交的物理像素分隔线。 */
    void drawToolBarSeparator(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制标准状态栏的安静表面和顶部边界。 */
    void drawStatusBarPanel(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制输入控件面板，不接触文本和输入法状态。 */
    void drawInputPanel(
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制标准数字显示控件的 Fluent 圆角表面。 */
    void drawDigitalDisplayFrame(
        const QStyleOptionFrame *option,
        QPainter *painter) const;
    /** @brief 绘制组合框面板、箭头和平台标签。 */
    void drawComboBox(
        const QStyleOptionComboBox *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 计算组合框 frame、edit 与 arrow 的方向安全矩形。 */
    [[nodiscard]] QRect comboBoxSubControlRect(
        const QStyleOptionComboBox *option,
        QStyle::SubControl subControl) const;
    /** @brief 使用组合框稳定矩形执行命中测试。 */
    [[nodiscard]] QStyle::SubControl hitTestComboBox(
        const QStyleOptionComboBox *option,
        const QPoint &position) const;
    /** @brief 判断绘制上下文是否由标准组合框或其 popup 子控件发起。 */
    [[nodiscard]] bool isComboBoxPopupContext(
        const QWidget *widget) const noexcept;
    /** @brief 绘制组合框弹出项状态并委托平台绘制内容。 */
    void drawComboBoxPopupItem(
        const QStyleOptionViewItem *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制使用菜单 delegate 的组合框弹出项并保留平台内容。 */
    void drawComboBoxPopupMenuItem(
        const QStyleOptionMenuItem *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制数值输入框面板、按钮状态和符号。 */
    void drawSpinBox(
        const QStyleOptionSpinBox *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 计算数值输入框 frame、edit、up 和 down 稳定矩形。 */
    [[nodiscard]] QRect spinBoxSubControlRect(
        const QStyleOptionSpinBox *option,
        QStyle::SubControl subControl) const;
    /** @brief 使用数值输入框稳定矩形执行命中测试。 */
    [[nodiscard]] QStyle::SubControl hitTestSpinBox(
        const QStyleOptionSpinBox *option,
        const QPoint &position) const;
    /** @brief 绘制标签页表面、选中指示和平台标签。 */
    void drawTabBarTab(
        const QStyleOptionTab *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制工具提示面板。 */
    void drawToolTipPanel(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制标准菜单的圆角弹出表面。 */
    void drawMenuPanel(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制标准菜单未被 action 覆盖的区域。 */
    void drawMenuEmptyArea(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制与应用窗口一致的菜单栏背景。 */
    void drawMenuBarPanel(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制菜单栏未被 action 覆盖的区域。 */
    void drawMenuBarEmptyArea(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制菜单栏项交互表面并委托平台绘制内容。 */
    void drawMenuBarItem(
        const QStyleOptionMenuItem *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制确定和不确定进度条。 */
    void drawProgressBar(
        const QStyleOptionProgressBar *option,
        QPainter *painter,
        const QWidget *widget);
    /** @brief 绘制滑块轨道、活动区和手柄。 */
    void drawSlider(
        const QStyleOptionSlider *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制无箭头 Fluent 滚动条轨道和滑块。 */
    void drawScrollBar(
        const QStyleOptionSlider *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 计算滚动条 slider、groove 和 page 稳定矩形。 */
    [[nodiscard]] QRect scrollBarSubControlRect(
        const QStyleOptionSlider *option,
        QStyle::SubControl subControl) const;
    /** @brief 使用滚动条稳定矩形执行命中测试。 */
    [[nodiscard]] QStyle::SubControl hitTestScrollBar(
        const QStyleOptionSlider *option,
        const QPoint &position) const;
    /** @brief 绘制菜单状态、标记和箭头并委托平台绘制内容。 */
    void drawMenuItem(
        const QStyleOptionMenuItem *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 从分支区原语恢复树形完整可见行并绘制交互背板。 */
    void drawItemViewRow(
        const QStyleOptionViewItem *option,
        QPainter *painter,
        const QWidget *widget) const;

    /** @brief 幂等注册由本样式驱动的可见忙碌进度条。 */
    void registerBusyProgressBar(QProgressBar *progressBar);
    /** @brief 清除销毁或已不满足动画条件的弱引用。 */
    void removeIneligibleBusyProgressBars();
    /** @brief 停止共享动画并按需刷新注册控件的静态外观。 */
    void stopBusyProgressAnimation(bool refreshWidgets);

    ZzFluentStyle *const q_ptr;
    QPointer<ZzThemeController> controller;
    std::shared_ptr<const ZzThemeSnapshot> snapshot;
    QPointer<QWidget> focusVisualWidget;
    /** @brief 本样式所有线性忙碌进度条复用的唯一循环动画。 */
    QVariantAnimation *busyProgressAnimation = nullptr;
    /** @brief 仅保存控件弱引用，避免样式延长控件生命周期。 */
    QList<QPointer<QProgressBar>> busyProgressBars;
    /** @brief 当前循环相位，由共享动画值直接更新。 */
    qreal busyProgressPhase = 0.0;
    bool keyboardFocusVisuals = false;
    quint64 iconRevision = 0;
    ZzStyleCache cache{4 * 1024 * 1024};
};

} // namespace ZzFluentUI
