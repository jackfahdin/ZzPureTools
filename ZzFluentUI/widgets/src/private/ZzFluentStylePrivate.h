#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/Qt>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtWidgets/QStyleOption>

#include "ZzStyleCache.h"

#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeChangeKind.h>

namespace ZzFluentUI {

class ZzFluentStyle;
class ZzThemeController;
class ZzThemeSnapshot;

/** @brief 持有主题快照、非拥有控制器引用和 Widgets 私有缓存。 */
class ZzFluentStylePrivate final
{
public:
    /** @brief 绑定控制器并用首个快照初始化固定视觉槽。 */
    ZzFluentStylePrivate(
        ZzFluentStyle *q,
        ZzThemeController *controller);

    /** @brief 在 GUI 线程执行有界 SVG 资源渲染和缓存。 */
    [[nodiscard]] QPixmap iconPixmap(
        const ZzIconDescriptor &descriptor,
        QSize logicalSize,
        qreal devicePixelRatio,
        QColor color,
        Qt::LayoutDirection direction);

    /** @brief 同步新快照并按变更分类刷新绘制或几何。 */
    void applySnapshot(ZzThemeChangeKinds changes);

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
    /** @brief 绘制输入控件面板，不接触文本和输入法状态。 */
    void drawInputPanel(
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制组合框面板、箭头和平台标签。 */
    void drawComboBox(
        const QStyleOptionComboBox *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制标签页表面、选中指示和平台标签。 */
    void drawTabBarTab(
        const QStyleOptionTab *option,
        QPainter *painter,
        const QWidget *widget) const;
    /** @brief 绘制工具提示面板。 */
    void drawToolTipPanel(
        const QStyleOption *option,
        QPainter *painter) const;
    /** @brief 绘制确定和不确定进度条。 */
    void drawProgressBar(
        const QStyleOptionProgressBar *option,
        QPainter *painter,
        const QWidget *widget) const;
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
    /** @brief 绘制菜单选中背景并委托平台样式绘制内容。 */
    void drawMenuItem(
        const QStyleOptionMenuItem *option,
        QPainter *painter,
        const QWidget *widget) const;

    ZzFluentStyle *const q_ptr;
    QPointer<ZzThemeController> controller;
    std::shared_ptr<const ZzThemeSnapshot> snapshot;
    quint64 iconRevision = 0;
    ZzStyleCache cache{4 * 1024 * 1024};
};

} // namespace ZzFluentUI
