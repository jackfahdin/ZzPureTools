#pragma once

#include <array>

#include <QtCore/QDate>
#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QString>

class QPainter;
class QEvent;
class QWidget;

namespace ZzFluentUI {

class ZzCalendar;

/** @brief 持有固定日期文本缓存并执行无子控件分配的单元格绘制。 */
class ZzCalendarPrivate final : public QObject
{
public:
    /**
     * @brief 创建 1 到 31 的固定文本缓存。
     * @param q 非空、非拥有的公开日历。
     */
    explicit ZzCalendarPrivate(ZzCalendar *q);

    /** @brief 清除 hover 单元格并请求旧区域重绘。 */
    void clearHover();

    /**
     * @brief 按日历 palette 和当前日期状态绘制一个单元格。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 日期单元格边界。
     * @param date 有效日期。
     */
    void paintCell(
        QPainter *painter,
        const QRect &rect,
        QDate date) const;

    bool eventFilter(QObject *watched, QEvent *event) override;

    ZzCalendar *const q_ptr;
    std::array<QString, 31> dayTexts;

private:
    void updateHover(const QRect &cell);

    QRect hoveredCellRect;
    QWidget *hoverViewport = nullptr;
};

} // namespace ZzFluentUI
