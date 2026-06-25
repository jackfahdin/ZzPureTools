#ifndef ZZTHEME_H
#define ZZTHEME_H

#include <QObject>

#include "ZzDesignTokens.h"
#include "ZzFluentCoreGlobal.h"

class ZZ_CORE_EXPORT ZzTheme : public QObject
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

#endif // ZZTHEME_H
