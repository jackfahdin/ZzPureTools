#pragma once

#include <QtCore/QRectF>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QPainter;

namespace ZzFluentUI {

class ZzThemeSnapshot;

/** @brief 提供不拥有状态、不读取文件的 Fluent 绘制原语。 */
class ZZ_FLUENT_UI_EXPORT ZzFluentPainter final
{
public:
    ZzFluentPainter() = delete;

    /**
     * @brief 绘制带边框的控件背景。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 控件逻辑像素边界。
     * @param snapshot 当前不可变主题快照。
     * @param hovered 是否处于悬停状态。
     * @param pressed 是否处于按下状态。
     * @param enabled 是否处于启用状态。
     * @pre 调用线程必须是控件线程。
     */
    static void drawControlBackground(
        QPainter *painter,
        const QRectF &rect,
        const ZzThemeSnapshot &snapshot,
        bool hovered,
        bool pressed,
        bool enabled);

    /**
     * @brief 绘制按物理像素对齐的可见焦点环。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 焦点区域逻辑像素边界。
     * @param snapshot 当前不可变主题快照。
     * @param devicePixelRatio 设备像素比；无效值按 1.0 处理。
     * @pre 调用线程必须是控件线程。
     */
    static void drawFocusRing(
        QPainter *painter,
        const QRectF &rect,
        const ZzThemeSnapshot &snapshot,
        qreal devicePixelRatio);
};

} // namespace ZzFluentUI
