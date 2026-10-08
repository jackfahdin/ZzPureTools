#include "private/ZzBorderBeamPrivate.h"
#include <ZzFluentUI/ZzBorderBeamButton.h>

#include <QEvent>
#include <QPainter>
#include <QPointer>
#include <QStyleOptionButton>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {

ZzBorderBeamButton::ZzBorderBeamButton(QWidget *parent)
    : QPushButton(parent)
    , d_ptr(std::make_unique<ZzBorderBeamPrivate>(this))
{
    setAttribute(Qt::WA_Hover);
}

ZzBorderBeamButton::ZzBorderBeamButton(const QString &text, QWidget *parent)
    : ZzBorderBeamButton(parent)
{
    setText(text);
}

ZzBorderBeamButton::~ZzBorderBeamButton() = default;

qreal ZzBorderBeamButton::beamLength() const { return d_ptr->beamLength; }

void ZzBorderBeamButton::setBeamLength(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp(value, 0.0, 10000.0);
    if (d_ptr->beamLength == value)
        return;
    d_ptr->beamLength = value;
    update();
    const QPointer<ZzBorderBeamButton> guard(this);
    updateAnimationState();
    if (!guard || d_ptr->beamLength != value)
        return;
    Q_EMIT beamLengthChanged(value);
}

qreal ZzBorderBeamButton::beamWidth() const { return d_ptr->beamWidth; }

void ZzBorderBeamButton::setBeamWidth(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp(value, 0.5, 32.0);
    if (d_ptr->beamWidth == value)
        return;
    d_ptr->beamWidth = value;
    d_ptr->pathDirty = true;
    update();
    Q_EMIT beamWidthChanged(value);
}

qreal ZzBorderBeamButton::cornerRadius() const { return d_ptr->cornerRadius; }

void ZzBorderBeamButton::setCornerRadius(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp(value, 0.0, 1000.0);
    if (d_ptr->cornerRadius == value)
        return;
    d_ptr->cornerRadius = value;
    d_ptr->pathDirty = true;
    update();
    Q_EMIT cornerRadiusChanged(value);
}

QColor ZzBorderBeamButton::backgroundColor() const { return d_ptr->backgroundColor; }

void ZzBorderBeamButton::setBackgroundColor(QColor value)
{
    if (d_ptr->backgroundColor == value)
        return;
    d_ptr->backgroundColor = value;
    update();
    Q_EMIT backgroundColorChanged(value);
}

QColor ZzBorderBeamButton::borderColor() const { return d_ptr->borderColor; }

void ZzBorderBeamButton::setBorderColor(QColor value)
{
    if (d_ptr->borderColor == value)
        return;
    d_ptr->borderColor = value;
    update();
    Q_EMIT borderColorChanged(value);
}

QColor ZzBorderBeamButton::startColor() const { return d_ptr->startColor; }

void ZzBorderBeamButton::setStartColor(QColor value)
{
    if (d_ptr->startColor == value)
        return;
    d_ptr->startColor = value;
    update();
    Q_EMIT startColorChanged(value);
}

QColor ZzBorderBeamButton::endColor() const { return d_ptr->endColor; }

void ZzBorderBeamButton::setEndColor(QColor value)
{
    if (d_ptr->endColor == value)
        return;
    d_ptr->endColor = value;
    update();
    Q_EMIT endColorChanged(value);
}

int ZzBorderBeamButton::animationDuration() const { return d_ptr->animationDuration; }

void ZzBorderBeamButton::setAnimationDuration(int value)
{
    value = std::clamp(value, 100, 600000);
    if (d_ptr->animationDuration == value)
        return;
    d_ptr->advance();
    d_ptr->animationDuration = value;
    update();
    Q_EMIT animationDurationChanged(value);
}

qreal ZzBorderBeamButton::initialProgress() const { return d_ptr->initialProgress; }

void ZzBorderBeamButton::setInitialProgress(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp(value, 0.0, 1.0);
    if (d_ptr->initialProgress == value)
        return;
    d_ptr->initialProgress = value;
    d_ptr->restart();
    update();
    Q_EMIT initialProgressChanged(value);
}

ZzBorderBeamButton::Direction ZzBorderBeamButton::direction() const { return d_ptr->direction; }

void ZzBorderBeamButton::setDirection(Direction value)
{
    if (value != Clockwise && value != CounterClockwise)
        return;
    if (d_ptr->direction == value)
        return;
    d_ptr->advance();
    d_ptr->direction = value;
    update();
    Q_EMIT directionChanged(value);
}

int ZzBorderBeamButton::beamCount() const { return d_ptr->beamCount; }

void ZzBorderBeamButton::setBeamCount(int value)
{
    value = std::clamp(value, 1, 8);
    if (d_ptr->beamCount == value)
        return;
    d_ptr->beamCount = value;
    update();
    Q_EMIT beamCountChanged(value);
}

bool ZzBorderBeamButton::isAnimationEnabled() const { return d_ptr->animationEnabled; }

void ZzBorderBeamButton::setAnimationEnabled(bool value)
{
    if (d_ptr->animationEnabled == value)
        return;
    d_ptr->animationEnabled = value;
    update();
    const QPointer<ZzBorderBeamButton> guard(this);
    updateAnimationState();
    if (!guard || d_ptr->animationEnabled != value)
        return;
    Q_EMIT animationEnabledChanged(value);
}

ZzBorderBeamButton::ThemeMode ZzBorderBeamButton::themeMode() const { return d_ptr->themeMode; }

void ZzBorderBeamButton::setThemeMode(ThemeMode value)
{
    if (value != AutoTheme && value != LightTheme && value != DarkTheme)
        return;
    if (d_ptr->themeMode == value)
        return;
    d_ptr->themeMode = value;
    update();
    Q_EMIT themeModeChanged(value);
}

qreal ZzBorderBeamButton::progress() const { return d_ptr->progress; }
bool ZzBorderBeamButton::isRunning() const { return d_ptr->timer.isActive(); }
void ZzBorderBeamButton::restartAnimation() { d_ptr->restart(); }
QSize ZzBorderBeamButton::sizeHint() const { return QPushButton::sizeHint().expandedTo(QSize(120, 40)); }

ZzBorderBeamButton::ThemeConfig ZzBorderBeamButton::defaultLightTheme()
{
    return ZzBorderBeam::defaultLightTheme();
}

ZzBorderBeamButton::ThemeConfig ZzBorderBeamButton::defaultDarkTheme()
{
    return ZzBorderBeam::defaultDarkTheme();
}

ZzBorderBeamButton::ThemeConfig ZzBorderBeamButton::lightTheme() const { return d_ptr->lightTheme; }
void ZzBorderBeamButton::setLightTheme(const ThemeConfig &config)
{
    if (d_ptr->lightTheme == config)
        return;
    d_ptr->lightTheme = config;
    update();
    Q_EMIT lightThemeChanged();
}

ZzBorderBeamButton::ThemeConfig ZzBorderBeamButton::darkTheme() const { return d_ptr->darkTheme; }
void ZzBorderBeamButton::setDarkTheme(const ThemeConfig &config)
{
    if (d_ptr->darkTheme == config)
        return;
    d_ptr->darkTheme = config;
    update();
    Q_EMIT darkThemeChanged();
}

ZzBorderBeamButton::ThemeConfig ZzBorderBeamButton::activeTheme() const { return d_ptr->activeTheme(); }

void ZzBorderBeamButton::updateAnimationState()
{
    const bool wasRunning = isRunning();
    d_ptr->synchronize();
    if (wasRunning != isRunning())
        Q_EMIT runningChanged(isRunning());
}

void ZzBorderBeamButton::showEvent(QShowEvent *event)
{
    QPushButton::showEvent(event);
    updateAnimationState();
}

void ZzBorderBeamButton::hideEvent(QHideEvent *event)
{
    QPushButton::hideEvent(event);
    updateAnimationState();
}

void ZzBorderBeamButton::changeEvent(QEvent *event)
{
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::EnabledChange
        || event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        update();
        updateAnimationState();
    }
}

void ZzBorderBeamButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QStyleOptionButton option;
    initStyleOption(&option);
    // 独立主题及背景覆盖应保持默认文字对比度，显式 ButtonText 仍优先。
    if (d_ptr->themeMode != AutoTheme || d_ptr->backgroundColor.isValid()) {
        const auto background = d_ptr->resolvedTheme().backgroundColor;
        const auto linear = [](qreal channel) {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        };
        const qreal luminance = 0.2126 * linear(background.redF()) + 0.7152 * linear(background.greenF())
            + 0.0722 * linear(background.blueF());
        for (auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
            if (testAttribute(Qt::WA_SetPalette) && palette().isBrushSet(group, QPalette::ButtonText))
                continue;
            QColor textColor(luminance > 0.179 ? Qt::black : Qt::white);
            if (group == QPalette::Disabled)
                textColor.setAlphaF(0.46f);
            option.palette.setColor(group, QPalette::ButtonText, textColor);
        }
    }
    d_ptr->drawSurface(painter,
        option.state.testFlag(QStyle::State_Sunken) || option.state.testFlag(QStyle::State_On),
        option.state.testFlag(QStyle::State_MouseOver));
    style()->drawControl(QStyle::CE_PushButtonLabel, &option, &painter, this);
    d_ptr->drawBeam(painter);
    if (option.state.testFlag(QStyle::State_HasFocus)
        && option.state.testFlag(QStyle::State_KeyboardFocusChange)) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect().adjusted(4, 4, -4, -4);
        focus.backgroundColor = d_ptr->resolvedTheme().backgroundColor;
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
    }
}

} // namespace ZzFluentUI
