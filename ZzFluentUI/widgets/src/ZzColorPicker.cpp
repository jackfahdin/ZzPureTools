#include <ZzFluentUI/ZzColorPicker.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QPointer>

#include "private/ZzColorPickerPrivate.h"

namespace ZzFluentUI {

ZzColorPicker::ZzColorPicker(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzColorPickerPrivate>(this))
{
}

ZzColorPicker::~ZzColorPicker() = default;

ZzColorPicker::Appearance ZzColorPicker::appearance() const noexcept
{
    return d_ptr->appearance;
}

void ZzColorPicker::setAppearance(Appearance value)
{
    if (d_ptr->appearance == value || (value != Compact && value != Fluent)) {
        return;
    }
    d_ptr->appearance = value;
    d_ptr->syncAppearance();
    Q_EMIT appearanceChanged(value);
}

ZzColorPicker::ColorRepresentation ZzColorPicker::colorRepresentation() const noexcept
{
    return d_ptr->representation;
}

void ZzColorPicker::setColorRepresentation(ColorRepresentation value)
{
    if (d_ptr->representation == value || (value != Rgba && value != Hsva)) {
        return;
    }
    d_ptr->representation = value;
    d_ptr->syncDerivedState();
    Q_EMIT colorRepresentationChanged(value);
}

ZzColorPicker::ColorSpectrumShape ZzColorPicker::colorSpectrumShape() const noexcept
{
    return d_ptr->shape;
}

void ZzColorPicker::setColorSpectrumShape(ColorSpectrumShape value)
{
    if (d_ptr->shape == value || (value != Box && value != Ring)) {
        return;
    }
    d_ptr->shape = value;
    d_ptr->syncDerivedState();
    Q_EMIT colorSpectrumShapeChanged(value);
}

bool ZzColorPicker::isColorSpectrumVisible() const noexcept
{
    return d_ptr->spectrumVisible;
}

void ZzColorPicker::setColorSpectrumVisible(bool value)
{
    if (d_ptr->spectrumVisible == value) {
        return;
    }
    d_ptr->spectrumVisible = value;
    d_ptr->syncVisibility();
    Q_EMIT colorSpectrumVisibleChanged(value);
}

bool ZzColorPicker::isColorPaletteVisible() const noexcept
{
    return d_ptr->paletteVisible;
}

void ZzColorPicker::setColorPaletteVisible(bool value)
{
    if (d_ptr->paletteVisible == value) {
        return;
    }
    d_ptr->paletteVisible = value;
    d_ptr->syncVisibility();
    Q_EMIT colorPaletteVisibleChanged(value);
}

bool ZzColorPicker::isColorPreviewVisible() const noexcept
{
    return d_ptr->previewVisible;
}

void ZzColorPicker::setColorPreviewVisible(bool value)
{
    if (d_ptr->previewVisible == value) {
        return;
    }
    d_ptr->previewVisible = value;
    d_ptr->syncVisibility();
    Q_EMIT colorPreviewVisibleChanged(value);
}

bool ZzColorPicker::isAlphaSliderVisible() const noexcept
{
    return d_ptr->alphaSliderVisible;
}

void ZzColorPicker::setAlphaSliderVisible(bool value)
{
    if (d_ptr->alphaSliderVisible == value) {
        return;
    }
    d_ptr->alphaSliderVisible = value;
    d_ptr->syncVisibility();
    Q_EMIT alphaSliderVisibleChanged(value);
}

bool ZzColorPicker::isColorSliderVisible() const noexcept
{
    return d_ptr->sliderVisible;
}

void ZzColorPicker::setColorSliderVisible(bool value)
{
    if (d_ptr->sliderVisible == value) {
        return;
    }
    d_ptr->sliderVisible = value;
    d_ptr->syncVisibility();
    Q_EMIT colorSliderVisibleChanged(value);
}

bool ZzColorPicker::isColorChannelTextInputVisible() const noexcept
{
    return d_ptr->channelTextInputVisible;
}

void ZzColorPicker::setColorChannelTextInputVisible(bool value)
{
    if (d_ptr->channelTextInputVisible == value) {
        return;
    }
    d_ptr->channelTextInputVisible = value;
    d_ptr->syncVisibility();
    Q_EMIT colorChannelTextInputVisibleChanged(value);
}

QColor ZzColorPicker::currentColor() const noexcept
{
    return d_ptr->currentColor;
}

void ZzColorPicker::setCurrentColor(QColor color)
{
    if (d_ptr->applyCurrentColor(color)) {
        d_ptr->notifyCurrentColorChanged();
    }
}

void ZzColorPicker::commitPendingEdits()
{
    const QPointer<ZzColorPicker> guard(this);
    d_ptr->commitHexEditor();
    if (guard) {
        d_ptr->flushColorNotifications();
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
