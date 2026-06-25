#ifndef ZZFLUENT_CORE_ZZPALETTE_HPP
#define ZZFLUENT_CORE_ZZPALETTE_HPP

#include <QPalette>

#include "ZzFluent/Core/ZzFluentGlobal.hpp"

class QWidget;
class ZzTheme;

class ZZ_FLUENT_EXPORT ZzPalette
{
public:
    [[nodiscard]] static QPalette createWidgetPalette(const ZzTheme& theme);
    static void applyTo(QWidget* widget, const ZzTheme& theme);
    static void syncWithSystem(QWidget* widget, const ZzTheme& theme);
};

#endif // ZZFLUENT_CORE_ZZPALETTE_HPP
