#include <ZzFluentUI/ZzColorPicker.h>

#include <utility>

#include <QtCore/QEvent>

#include "private/ZzColorPickerPrivate.h"

namespace ZzFluentUI {

ZzColorPicker::ZzColorPicker(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzColorPickerPrivate>(this))
{
}

ZzColorPicker::~ZzColorPicker() = default;

QColor ZzColorPicker::currentColor() const noexcept
{
    return d_ptr->currentColor;
}

void ZzColorPicker::setCurrentColor(QColor color)
{
    if (d_ptr->applyCurrentColor(color)) {
        Q_EMIT currentColorChanged(d_ptr->currentColor);
    }
}

bool ZzColorPicker::isAlphaEnabled() const noexcept
{
    return d_ptr->alphaEnabled;
}

void ZzColorPicker::setAlphaEnabled(bool enabled)
{
    if (d_ptr->alphaEnabled == enabled) {
        return;
    }
    d_ptr->alphaEnabled = enabled;
    d_ptr->syncAlphaPresentation();
    Q_EMIT alphaEnabledChanged(enabled);
}

QList<QColor> ZzColorPicker::paletteColors() const
{
    return d_ptr->paletteColors();
}

void ZzColorPicker::setPaletteColors(QList<QColor> colors)
{
    if (d_ptr->applyPaletteColors(std::move(colors))) {
        Q_EMIT paletteColorsChanged();
    }
}

int ZzColorPicker::paletteColorCount() const noexcept
{
    return d_ptr->paletteColorCount();
}

void ZzColorPicker::resetPaletteColors()
{
    setPaletteColors(ZzColorPickerPrivate::defaultPaletteColors());
}

void ZzColorPicker::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::LanguageChange:
        d_ptr->refreshAccessibleText();
        break;
    case QEvent::FontChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        d_ptr->refreshTheme();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
