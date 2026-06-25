#include "ZzFluent/Core/ZzTheme.hpp"

#include <QGuiApplication>
#include <QStyleHints>

ZzTheme::ZzTheme(QObject* parent)
    : QObject(parent)
{
    syncFromSystem();

    if (QGuiApplication::styleHints() != nullptr) {
        connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            this,
            [this]() {
                if (m_mode == ZzThemeMode::System) {
                    applyResolvedMode(resolveSystemMode());
                }
            });
    }
}

ZzThemeMode ZzTheme::mode() const noexcept
{
    return m_mode;
}

ZzThemeMode ZzTheme::effectiveMode() const noexcept
{
    return m_effectiveMode;
}

const ZzColorTokens& ZzTheme::colors() const noexcept
{
    return m_colors;
}

const ZzMetricTokens& ZzTheme::metrics() const noexcept
{
    return m_metrics;
}

void ZzTheme::setMode(ZzThemeMode mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    emit modeChanged(m_mode);

    if (mode == ZzThemeMode::System) {
        applyResolvedMode(resolveSystemMode());
        return;
    }

    applyResolvedMode(mode);
}

void ZzTheme::syncFromSystem()
{
    if (m_mode == ZzThemeMode::System) {
        applyResolvedMode(resolveSystemMode());
    }
}

QColor ZzTheme::controlFill(ZzControlState state) const
{
    switch (state) {
    case ZzControlState::Disabled:
        return m_colors.controlFillDisabled;
    case ZzControlState::Pressed:
        return m_colors.controlFillPressed;
    case ZzControlState::Hovered:
        return m_colors.controlFillHover;
    case ZzControlState::Normal:
    default:
        return m_colors.controlFill;
    }
}

QColor ZzTheme::controlStroke(ZzControlState state) const
{
    if (state == ZzControlState::Hovered) {
        return m_colors.controlStrokeHover;
    }
    return m_colors.controlStroke;
}

QColor ZzTheme::textColor(ZzControlState state) const
{
    if (state == ZzControlState::Disabled) {
        return m_colors.textDisabled;
    }
    return m_colors.textPrimary;
}

void ZzTheme::setAccentColor(const QColor& color)
{
    m_customAccent = color;
    m_hasCustomAccent = color.isValid();
    applyResolvedMode(m_effectiveMode);
}

void ZzTheme::applyResolvedMode(ZzThemeMode resolvedMode)
{
    const bool effectiveModeChanged = m_effectiveMode != resolvedMode;
    m_effectiveMode = resolvedMode;

    m_colors = resolvedMode == ZzThemeMode::Dark ? zzDarkColorTokens() : zzLightColorTokens();
    if (m_hasCustomAccent) {
        m_colors.accent = m_customAccent;
        m_colors.accentHover = m_customAccent.lighter(108);
        m_colors.accentPressed = m_customAccent.darker(112);
    }

    if (effectiveModeChanged) {
        emit modeChanged(m_mode);
    }
    emit colorsChanged();
}

ZzThemeMode ZzTheme::resolveSystemMode() const
{
    if (QGuiApplication::styleHints() == nullptr) {
        return ZzThemeMode::Light;
    }

    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
        ? ZzThemeMode::Dark
        : ZzThemeMode::Light;
}
