#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QStyleOptionTab>
#include <ZzFluentUI/ZzTabBarAppearance.h>

namespace ZzFluentUI {

class ZzFluentStyle;

/** @brief 标签外观的公开 Qt 绘制与可重定向动画；每个标签栏仅一个实例。 */
class ZzTabBarStylePrivate final : public QObject
{
public:
    ZzTabBarStylePrivate(QTabBar *bar, ZzFluentStyle *style);
    static ZzTabBarAppearance appearance(const QWidget *widget);
    static bool vertical(QTabBar::Shape shape);
    static QSize sizeHint(const QStyleOptionTab &tab, QSize base, const QWidget *widget);
    void draw(const QStyleOptionTab &tab, QPainter *painter);
    void settle();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QRectF selectionRect(int index) const;
    QRectF itemRect(int index) const;
    void select();
    void advance(qreal progress);
    void press(int index);
    void synchronize();
    void drawLabel(const QStyleOptionTab &tab, QPainter *painter, const QColor &text);

    QPointer<QTabBar> bar_;
    ZzFluentStyle *style_;
    QVariantAnimation selection_;
    QVariantAnimation pressure_;
    ZzTabBarAppearance appearance_ = ZzTabBarAppearance::Standard;
    QRectF from_;
    QRectF target_;
    QRectF current_;
    QList<QRect> layout_;
    QList<qreal> weights_;
    QList<qreal> fromWeights_;
    qreal pressureValue_ = 0;
    int pressed_ = -1;
};

} // namespace ZzFluentUI
