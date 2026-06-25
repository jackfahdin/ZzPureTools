#ifndef ZZPURETOOLS_CORE_ZZPALETTE_HPP
#define ZZPURETOOLS_CORE_ZZPALETTE_HPP

#include <QPalette>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"

class QWidget;
class ZzTheme;

class ZZ_PURE_TOOLS_EXPORT ZzPalette
{
public:
    [[nodiscard]] static QPalette createWidgetPalette(const ZzTheme& theme);
    static void applyTo(QWidget* widget, const ZzTheme& theme);
    static void syncWithSystem(QWidget* widget, const ZzTheme& theme);
};

#endif // ZZPURETOOLS_CORE_ZZPALETTE_HPP
