#ifndef ZZPURETOOLS_STYLE_ZZSTYLEDELEGATE_HPP
#define ZZPURETOOLS_STYLE_ZZSTYLEDELEGATE_HPP

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Core/ZzToken.hpp"

#include <QRectF>
#include <QSize>
#include <QString>

class QPainter;
class ZzTheme;

/**
 * @brief 样式绘制上下文。
 *
 * 聚合控件绘制时所需的主题、状态、动画进度及文本等信息，
 * 供 ZzStyleDelegate 实现统一绘制。
 */
struct ZZ_PURE_TOOLS_EXPORT ZzStyleContext
{
    const ZzTheme* theme = nullptr;                 ///< 当前主题对象。
    ZzControlState state = ZzControlState::Normal;  ///< 控件状态。
    qreal hoverProgress = 0.0;                      ///< 悬停动画进度，范围 [0, 1]。
    qreal pressProgress = 0.0;                      ///< 按下动画进度，范围 [0, 1]。
    bool enabled = true;                            ///< 控件是否可用。
    QString text;                                   ///< 控件文本。
    bool popupVisible = false;                      ///< 弹出层是否可见。
    bool itemSelected = false;                      ///< 条目是否被选中。
};

/**
 * @brief 样式绘制代理接口。
 *
 * 定义控件的外观绘制协议。各控件通过实现该接口，
 * 在不同平台和主题下实现一致的 Fluent UI 风格渲染。
 */
class ZZ_PURE_TOOLS_EXPORT ZzStyleDelegate
{
public:
    virtual ~ZzStyleDelegate() = default;

    /**
     * @brief 绘制控件外观。
     * @param painter 绘图器。
     * @param rect 控件绘制区域。
     * @param context 绘制上下文。
     */
    virtual void paint(QPainter& painter, const QRectF& rect,
                       const ZzStyleContext& context) const = 0;

    /**
     * @brief 获取控件的建议尺寸。
     * @param context 绘制上下文。
     * @param available 可用尺寸。
     * @return 控件的建议尺寸。
     */
    [[nodiscard]] virtual QSize sizeHint(const ZzStyleContext& context,
                                         const QSize& available) const;
};

#endif  // ZZPURETOOLS_STYLE_ZZSTYLEDELEGATE_HPP
