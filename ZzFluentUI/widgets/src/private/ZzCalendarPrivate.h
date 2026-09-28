#pragma once

#include <array>

#include <QtCore/QDate>
#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtGui/QColor>

class QPainter;
class QEvent;
class QWidget;
class QToolButton;
class QSpinBox;
class QTableView;

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

    /** @brief 同步导航、星期表头与字体尺寸，主题切换时复用已有对象。 */
    void refreshVisuals();

    /** @brief 返回按字体测量且包含内边距的网格自然尺寸。 */
    [[nodiscard]] QSize minimumSize() const;

    /** @brief 绘制日历的圆角细边框和统一背景。 */
    void paintSurface(QPainter *painter) const;

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
    /** @brief 重排原生导航，保留 Qt 用于年份编辑的占位对象。 */
    void configureNavigation();
    /** @brief 绘制导航按钮，避免 Qt 内部按钮强制使用强调色文字。 */
    void paintNavigationButton(QToolButton *button) const;
    void updateHover(const QRect &cell);

    QWidget *navigation = nullptr;
    QToolButton *previousButton = nullptr;
    QToolButton *nextButton = nullptr;
    QToolButton *monthButton = nullptr;
    QToolButton *yearButton = nullptr;
    QSpinBox *yearEditor = nullptr;
    QTableView *dateView = nullptr;
    int panelPadding = 12;
    int dayExtent = 40;
    std::array<bool, 7> workingDays{};
    qreal panelRadius = 8.0;
    qreal strokeWidth = 1.0;
    QColor hoverFill;
    QColor pressedFill;
    QRect hoveredCellRect;
    QWidget *hoverViewport = nullptr;
};

} // namespace ZzFluentUI
