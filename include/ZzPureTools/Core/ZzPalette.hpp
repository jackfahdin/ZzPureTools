#ifndef ZZPURETOOLS_CORE_ZZPALETTE_HPP
#define ZZPURETOOLS_CORE_ZZPALETTE_HPP

#include <QPalette>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"

class QWidget;
class ZzTheme;

/**
 * @brief 调色板工具类。
 *
 * 根据当前主题生成 QPalette，并将其应用到 QWidget 上，
 * 同时支持跟随系统主题变化同步更新。
 */
class ZZ_PURE_TOOLS_EXPORT ZzPalette
{
public:
    /**
     * @brief 根据主题创建 QPalette。
     * @param theme 主题对象。
     * @return 适配主题颜色的 QPalette。
     */
    [[nodiscard]] static QPalette createWidgetPalette(const ZzTheme& theme);

    /**
     * @brief 将主题调色板应用到指定控件。
     * @param widget 目标控件。
     * @param theme 主题对象。
     */
    static void applyTo(QWidget* widget, const ZzTheme& theme);

    /**
     * @brief 使控件跟随系统主题变化同步更新。
     * @param widget 目标控件。
     * @param theme 主题对象。
     */
    static void syncWithSystem(QWidget* widget, const ZzTheme& theme);
};

#endif // ZZPURETOOLS_CORE_ZZPALETTE_HPP
