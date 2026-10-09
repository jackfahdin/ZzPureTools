#pragma once

#include <QPainter>
#include <QWidget>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzExample {

/** @brief 绘制圆角卡片背景，颜色跟随当前 palette 与主题模式。 */
class ZzExampleCustomCard final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        const auto *fluent = qobject_cast<const ZzFluentUI::ZzFluentStyle *>(style());
        const bool hc = fluent && fluent->themeSnapshot()->mode() == ZzFluentUI::ZzThemeMode::HighContrast;
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(hc ? palette().color(QPalette::WindowText) : QColor(dark ? "#252525" : "#e9e9e9"));
        QColor base = palette().color(QPalette::Base);
        if (!base.alpha())
            base = palette().color(QPalette::Window);
        const qreal alpha = (dark ? 13.0 : 179.0) / 255.0;
        painter.setBrush(hc ? base
                            : QColor(qRound(base.red() * (1 - alpha) + 255 * alpha),
                                  qRound(base.green() * (1 - alpha) + 255 * alpha),
                                  qRound(base.blue() * (1 - alpha) + 255 * alpha)));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
};

} // namespace ZzExample
