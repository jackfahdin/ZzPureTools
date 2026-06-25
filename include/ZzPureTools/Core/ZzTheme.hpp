#ifndef ZZPURETOOLS_CORE_ZZTHEME_HPP
#define ZZPURETOOLS_CORE_ZZTHEME_HPP

#include <QObject>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Core/ZzToken.hpp"

class ZZ_PURE_TOOLS_EXPORT ZzTheme : public QObject
{
    Q_OBJECT

public:
    explicit ZzTheme(QObject* parent = nullptr);

    [[nodiscard]] ZzThemeMode mode() const noexcept;
    [[nodiscard]] ZzThemeMode effectiveMode() const noexcept;
    [[nodiscard]] const ZzColorTokens& colors() const noexcept;
    [[nodiscard]] const ZzMetricTokens& metrics() const noexcept;

    void setMode(ZzThemeMode mode);
    void syncFromSystem();

    [[nodiscard]] QColor controlFill(ZzControlState state) const;
    [[nodiscard]] QColor controlStroke(ZzControlState state) const;
    [[nodiscard]] QColor textColor(ZzControlState state) const;

public slots:
    void setAccentColor(const QColor& color);

signals:
    void modeChanged(ZzThemeMode mode);
    void colorsChanged();
    void metricsChanged();

private:
    void applyResolvedMode(ZzThemeMode resolvedMode);
    [[nodiscard]] ZzThemeMode resolveSystemMode() const;

    ZzThemeMode m_mode = ZzThemeMode::System;
    ZzThemeMode m_effectiveMode = ZzThemeMode::Light;
    ZzColorTokens m_colors = zzLightColorTokens();
    ZzMetricTokens m_metrics = zzDefaultMetricTokens();
    QColor m_customAccent;
    bool m_hasCustomAccent = false;
};

#endif // ZZPURETOOLS_CORE_ZZTHEME_HPP
