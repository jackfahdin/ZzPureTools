#pragma once

#include <QtCore/QRectF>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzColorToken.h>

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

    /**
     * @brief 绘制按物理像素对齐描边的通用圆角表面。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 表面的逻辑像素外边界。
     * @param snapshot 当前不可变主题快照。
     * @param fill 填充颜色令牌。
     * @param stroke 描边颜色令牌。
     * @param radius 非负逻辑像素圆角。
     * @param strokeWidth 非负逻辑像素描边，0 表示不绘制描边。
     */
    static void drawRoundedSurface(
        QPainter *painter,
        const QRectF &rect,
        const ZzThemeSnapshot &snapshot,
        ZzColorToken fill,
        ZzColorToken stroke,
        qreal radius,
        qreal strokeWidth);

    /**
     * @brief 使用主题遮罩色覆盖指定逻辑区域。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 遮罩区域。
     * @param snapshot 当前不可变主题快照。
     */
    static void drawOverlayScrim(
        QPainter *painter,
        const QRectF &rect,
        const ZzThemeSnapshot &snapshot);

    /**
     * @brief 绘制标准 Fluent 浮层表面。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 浮层逻辑像素边界。
     * @param snapshot 当前不可变主题快照。
     */
    static void drawPopupSurface(
        QPainter *painter,
        const QRectF &rect,
        const ZzThemeSnapshot &snapshot);

    /**
     * @brief 绘制圆形或胶囊形徽章背板。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 徽章逻辑像素边界。
     * @param snapshot 当前不可变主题快照。
     * @param fill 徽章填充颜色令牌。
     */
    static void drawBadgeSurface(
        QPainter *painter,
        const QRectF &rect,
        const ZzThemeSnapshot &snapshot,
        ZzColorToken fill);
};

} // namespace ZzFluentUI
