#pragma once

#include <QtCore/QRect>

class QEvent;
class QObject;
class QPainter;
class QStyleOptionHeader;
class QStyleOptionViewItem;
class QWidget;

namespace ZzFluentUI {

class ZzFluentStyle;

/** @brief 普通数据视图的 Fluent 外观；不接管模型、选择或编辑行为。 */
class ZzDataViewStylePrivate final
{
public:
    /** @brief 排除导航、图标网格、日历和组合框的专用视图。 */
    [[nodiscard]] static bool applies(const QWidget *widget);
    /** @brief 返回绘制、复选框命中与编辑器共用的内容区域。 */
    [[nodiscard]] static QRect contentRect(const QStyleOptionViewItem &option);
    /** @brief 绘制数据项状态，再通过样式绘制文字、图标和复选框。 */
    static void drawItem(const ZzFluentStyle &style,
        const QStyleOptionViewItem &option, QPainter *painter);
    /** @brief 拼接树的分支背景与内容背景，不覆盖相邻列。 */
    static void drawTreeRow(const ZzFluentStyle &style,
        const QStyleOptionViewItem &option, QPainter *painter, const QWidget *widget);
    /** @brief 绘制平面表头和与方向一致的分隔线。 */
    static void drawHeader(const ZzFluentStyle &style,
        const QStyleOptionHeader &option, QPainter *painter);
    /** @brief 启用并可逆恢复表格整行悬停所需的鼠标跟踪。 */
    static void polish(QWidget *widget, bool enabled);
    /** @brief 跟踪表格视口的悬停位置，重绘可见区域。 */
    static void handleEvent(QObject *watched, QEvent *event);
};

} // namespace ZzFluentUI
