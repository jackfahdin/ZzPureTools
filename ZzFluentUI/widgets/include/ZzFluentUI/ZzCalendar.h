#pragma once

#include <memory>

#include <QtWidgets/QCalendarWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QPainter;

namespace ZzFluentUI {

class ZzCalendarPrivate;

/**
 * @brief 保留 Qt 日期语义并提供 Fluent 单元格呈现的日历。
 *
 * 控件必须在 GUI 线程创建和访问。日期范围、区域设置、键盘导航、
 * 选择信号和无障碍表格语义由 QCalendarWidget 提供。
 */
class ZZ_FLUENT_UI_EXPORT ZzCalendar final : public QCalendarWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzCalendar)

public:
    /**
     * @brief 创建默认显示当前月份和当前日期的日历。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzCalendar(QWidget *parent = nullptr);

    /** @brief 销毁私有绘制缓存，Qt 子对象由 parent 关系释放。 */
    ~ZzCalendar() override;

protected:
    /**
     * @brief 使用当前 palette 绘制单个可见日期。
     * @param painter 非空且已激活的目标 painter。
     * @param rect 日期单元格的逻辑像素边界。
     * @param date 当前单元格对应的有效日期。
     */
    void paintCell(
        QPainter *painter,
        const QRect &rect,
        QDate date) const override;

    /**
     * @brief 在 palette、style、字体或启用状态变化后刷新可见日期。
     * @param event 可为空的 Qt 状态变化事件。
     */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzCalendarPrivate> d_ptr;
};

} // namespace ZzFluentUI
