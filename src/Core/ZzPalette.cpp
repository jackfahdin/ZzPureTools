#include "ZzPureTools/Core/ZzPalette.hpp"

#include <QWidget>

#include "ZzPureTools/Core/ZzTheme.hpp"

QPalette ZzPalette::createWidgetPalette(const ZzTheme& theme)
{
    const ZzColorTokens& colors = theme.colors();
    QPalette palette;
    palette.setColor(QPalette::Window, colors.windowBase);
    palette.setColor(QPalette::WindowText, colors.textPrimary);
    palette.setColor(QPalette::Base, colors.controlFill);
    palette.setColor(QPalette::AlternateBase, colors.controlFillHover);
    palette.setColor(QPalette::Text, colors.textPrimary);
    palette.setColor(QPalette::Button, colors.controlFill);
    palette.setColor(QPalette::ButtonText, colors.textPrimary);
    palette.setColor(QPalette::Highlight, colors.accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, colors.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, colors.textDisabled);
    return palette;
}

void ZzPalette::applyTo(QWidget* widget, const ZzTheme& theme)
{
    if (widget == nullptr) {
        return;
    }

    widget->setPalette(createWidgetPalette(theme));
    widget->setAutoFillBackground(true);
}

void ZzPalette::syncWithSystem(QWidget* widget, const ZzTheme& theme)
{
    applyTo(widget, theme);
}
