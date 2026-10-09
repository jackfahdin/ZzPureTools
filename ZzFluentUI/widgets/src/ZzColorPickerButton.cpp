#include <ZzFluentUI/ZzColorPickerButton.h>

#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>

#include <QtCore/QEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QStyleOptionToolButton>
#include <QtWidgets/QStylePainter>

#include "private/ZzColorPickerButtonPrivate.h"

namespace ZzFluentUI {

namespace {

/** @brief 透明棋盘格的浅格颜色。 */
constexpr char zzCheckerLightColor[] = "#ffffff";
/** @brief 透明棋盘格的深格颜色。 */
constexpr char zzCheckerDarkColor[] = "#b8b8b8";
/** @brief 色块与描边路径的圆角半径，单位逻辑像素。 */
constexpr qreal zzSwatchCornerRadius = 3.0;

} // namespace

ZzColorPickerButton::ZzColorPickerButton(QWidget *parent)
    : QToolButton(parent)
    , d_ptr(std::make_unique<ZzColorPickerButtonPrivate>(this))
{
}

ZzColorPickerButton::~ZzColorPickerButton() = default;

QColor ZzColorPickerButton::selectedColor() const
{
    return d_ptr->picker->currentColor();
}

void ZzColorPickerButton::setSelectedColor(QColor color)
{
    d_ptr->picker->setCurrentColor(color);
}

ZzColorPicker *ZzColorPickerButton::colorPicker() const noexcept
{
    return d_ptr->picker;
}

void ZzColorPickerButton::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QStyleOptionToolButton option;
    initStyleOption(&option);
    option.icon = QIcon();
    option.text.clear();
    option.arrowType = Qt::NoArrow;
    option.features &= ~QStyleOptionToolButton::HasMenu;
    if (d_ptr->popup->isVisible()) option.state |= QStyle::State_Sunken;
    painter.drawComplexControl(QStyle::CC_ToolButton, option);
    const int arrowWidth = style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &option, this);
    QRect arrowArea(width() - arrowWidth - 3, 0, arrowWidth, height());
    QRect swatch(8, 5, arrowArea.left() - 12, height() - 10);
    arrowArea = QStyle::visualRect(layoutDirection(), rect(), arrowArea);
    swatch = QStyle::visualRect(layoutDirection(), rect(), swatch);
    painter.save();
    QPainterPath swatchPath;
    swatchPath.addRoundedRect(swatch, zzSwatchCornerRadius, zzSwatchCornerRadius);
    painter.setClipPath(swatchPath);
    constexpr int cell = 5;
    for (int y = swatch.top(); y <= swatch.bottom(); y += cell) {
        for (int x = swatch.left(); x <= swatch.right(); x += cell) {
            const int parity = ((x - swatch.left()) / cell + (y - swatch.top()) / cell) % 2;
            painter.fillRect(QRect(x, y, cell, cell),
                QColor::fromString(QLatin1String(parity ? zzCheckerDarkColor : zzCheckerLightColor)));
        }
    }
    painter.restore();
    painter.setPen(Qt::NoPen);
    const QColor color = selectedColor();
    if (!isEnabled()) painter.setOpacity(.45);
    painter.setBrush(color);
    painter.drawRoundedRect(swatch, zzSwatchCornerRadius, zzSwatchCornerRadius);
    painter.setOpacity(1);
    painter.setFont(ZzSegoeIconFont::font(11));
    painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
    if (isDown() || d_ptr->popup->isVisible()) arrowArea.translate(0, 2);
    painter.drawText(arrowArea, Qt::AlignCenter, QString(QChar(static_cast<char16_t>(ZzSegoeIcon::ChevronDown))));
}

void ZzColorPickerButton::changeEvent(QEvent *event)
{
    QToolButton::changeEvent(event);
    if (!d_ptr) return;
    if (event->type() == QEvent::LanguageChange) d_ptr->refreshText();
    if (event->type() == QEvent::LayoutDirectionChange) d_ptr->popup->setLayoutDirection(layoutDirection());
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        d_ptr->popup->setPalette(palette());
        update();
    }
}

} // namespace ZzFluentUI
