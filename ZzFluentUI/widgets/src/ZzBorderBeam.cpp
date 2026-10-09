#include "private/ZzBorderBeamPrivate.h"
#include <ZzFluentUI/ZzBorderBeam.h>

#include <QEvent>
#include <QPainter>
#include <QPointer>
#include <QStyleOptionButton>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {

namespace {

/** @brief 浅色主题的默认背景色。 */
constexpr char zzBeamBackgroundLight[] = "#F9F9F9";
/** @brief 浅色主题的默认静态边框色。 */
constexpr char zzBeamBorderLight[] = "#E5E5E5";
/** @brief 浅色主题的默认光束拖尾色。 */
constexpr char zzBeamTailLight[] = "#005FB8";
/** @brief 浅色主题的默认光束头部色。 */
constexpr char zzBeamHeadLight[] = "#60CDFF";
/** @brief 深色主题的默认背景色。 */
constexpr char zzBeamBackgroundDark[] = "#272727";
/** @brief 深色主题的默认静态边框色。 */
constexpr char zzBeamBorderDark[] = "#454545";
/** @brief 深色主题的默认光束拖尾色。 */
constexpr char zzBeamTailDark[] = "#60CDFF";
/** @brief 深色主题的默认光束头部色。 */
constexpr char zzBeamHeadDark[] = "#A5E5FF";

} // namespace

ZzBorderBeam::ZzBorderBeam(QWidget *parent)
    : QFrame(parent)
    , d_ptr(std::make_unique<ZzBorderBeamPrivate>(this))
{
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
}

ZzBorderBeam::~ZzBorderBeam() = default;

qreal ZzBorderBeam::beamLength() const { return d_ptr->beamLength; }

void ZzBorderBeam::setBeamLength(qreal value)
{
    if (!std::isfinite(value))
        return;
    value = std::clamp(value, 0.0, 10000.0);
    if (d_ptr->beamLength == value)
        return;
    d_ptr->beamLength = value;
    update();
    const QPointer<ZzBorderBeam> guard(this);
    updateAnimationState();
    if (!guard || d_ptr->beamLength != value)
        return;
    Q_EMIT beamLengthChanged(value);
}

qreal ZzBorderBeam::beamWidth() const { return d_ptr->beamWidth; }

void ZzBorderBeam::setBeamWidth(qreal value)
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

qreal ZzBorderBeam::cornerRadius() const { return d_ptr->cornerRadius; }

void ZzBorderBeam::setCornerRadius(qreal value)
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

QColor ZzBorderBeam::backgroundColor() const { return d_ptr->backgroundColor; }

void ZzBorderBeam::setBackgroundColor(QColor value)
{
    if (d_ptr->backgroundColor == value)
        return;
    d_ptr->backgroundColor = value;
    update();
    Q_EMIT backgroundColorChanged(value);
}

QColor ZzBorderBeam::borderColor() const { return d_ptr->borderColor; }

void ZzBorderBeam::setBorderColor(QColor value)
{
    if (d_ptr->borderColor == value)
        return;
    d_ptr->borderColor = value;
    update();
    Q_EMIT borderColorChanged(value);
}

QColor ZzBorderBeam::startColor() const { return d_ptr->startColor; }

void ZzBorderBeam::setStartColor(QColor value)
{
    if (d_ptr->startColor == value)
        return;
    d_ptr->startColor = value;
    update();
    Q_EMIT startColorChanged(value);
}

QColor ZzBorderBeam::endColor() const { return d_ptr->endColor; }

void ZzBorderBeam::setEndColor(QColor value)
{
    if (d_ptr->endColor == value)
        return;
    d_ptr->endColor = value;
    update();
    Q_EMIT endColorChanged(value);
}

int ZzBorderBeam::animationDuration() const { return d_ptr->animationDuration; }

void ZzBorderBeam::setAnimationDuration(int value)
{
    value = std::clamp(value, 100, 600000);
    if (d_ptr->animationDuration == value)
        return;
    d_ptr->advance();
    d_ptr->animationDuration = value;
    update();
    Q_EMIT animationDurationChanged(value);
}

qreal ZzBorderBeam::initialProgress() const { return d_ptr->initialProgress; }

void ZzBorderBeam::setInitialProgress(qreal value)
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

ZzBorderBeam::ZzBeamDirection ZzBorderBeam::direction() const { return d_ptr->direction; }

void ZzBorderBeam::setDirection(ZzBeamDirection value)
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

int ZzBorderBeam::beamCount() const { return d_ptr->beamCount; }

void ZzBorderBeam::setBeamCount(int value)
{
    value = std::clamp(value, 1, 8);
    if (d_ptr->beamCount == value)
        return;
    d_ptr->beamCount = value;
    update();
    Q_EMIT beamCountChanged(value);
}

bool ZzBorderBeam::isAnimationEnabled() const { return d_ptr->animationEnabled; }

void ZzBorderBeam::setAnimationEnabled(bool value)
{
    if (d_ptr->animationEnabled == value)
        return;
    d_ptr->animationEnabled = value;
    update();
    const QPointer<ZzBorderBeam> guard(this);
    updateAnimationState();
    if (!guard || d_ptr->animationEnabled != value)
        return;
    Q_EMIT animationEnabledChanged(value);
}

ZzBorderBeam::ZzBeamThemeMode ZzBorderBeam::themeMode() const { return d_ptr->themeMode; }

void ZzBorderBeam::setThemeMode(ZzBeamThemeMode value)
{
    if (value != AutoTheme && value != LightTheme && value != DarkTheme)
        return;
    if (d_ptr->themeMode == value)
        return;
    d_ptr->themeMode = value;
    update();
    Q_EMIT themeModeChanged(value);
}

qreal ZzBorderBeam::progress() const { return d_ptr->progress; }
bool ZzBorderBeam::isRunning() const { return d_ptr->timer.isActive(); }
void ZzBorderBeam::restartAnimation() { d_ptr->restart(); }
QSize ZzBorderBeam::sizeHint() const { return QFrame::sizeHint().expandedTo(QSize(320, 180)); }
QSize ZzBorderBeam::minimumSizeHint() const { return QFrame::minimumSizeHint().expandedTo(QSize(40, 40)); }

ZzBorderBeam::ZzBeamThemeConfig ZzBorderBeam::defaultLightTheme()
{
    return { QColor::fromString(QLatin1String(zzBeamBackgroundLight)),
             QColor::fromString(QLatin1String(zzBeamBorderLight)),
             QColor::fromString(QLatin1String(zzBeamTailLight)),
             QColor::fromString(QLatin1String(zzBeamHeadLight)) };
}

ZzBorderBeam::ZzBeamThemeConfig ZzBorderBeam::defaultDarkTheme()
{
    return { QColor::fromString(QLatin1String(zzBeamBackgroundDark)),
             QColor::fromString(QLatin1String(zzBeamBorderDark)),
             QColor::fromString(QLatin1String(zzBeamTailDark)),
             QColor::fromString(QLatin1String(zzBeamHeadDark)) };
}

ZzBorderBeam::ZzBeamThemeConfig ZzBorderBeam::lightTheme() const { return d_ptr->lightTheme; }
void ZzBorderBeam::setLightTheme(const ZzBeamThemeConfig &config)
{
    if (d_ptr->lightTheme == config)
        return;
    d_ptr->lightTheme = config;
    update();
    Q_EMIT lightThemeChanged();
}

ZzBorderBeam::ZzBeamThemeConfig ZzBorderBeam::darkTheme() const { return d_ptr->darkTheme; }
void ZzBorderBeam::setDarkTheme(const ZzBeamThemeConfig &config)
{
    if (d_ptr->darkTheme == config)
        return;
    d_ptr->darkTheme = config;
    update();
    Q_EMIT darkThemeChanged();
}

ZzBorderBeam::ZzBeamThemeConfig ZzBorderBeam::activeTheme() const { return d_ptr->activeTheme(); }

void ZzBorderBeam::updateAnimationState()
{
    const bool wasRunning = isRunning();
    d_ptr->synchronize();
    if (wasRunning != isRunning())
        Q_EMIT runningChanged(isRunning());
}

void ZzBorderBeam::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    updateAnimationState();
}

void ZzBorderBeam::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    updateAnimationState();
}

void ZzBorderBeam::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::EnabledChange
        || event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        update();
        updateAnimationState();
    }
}

void ZzBorderBeam::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    d_ptr->drawSurface(painter);
    d_ptr->drawBeam(painter);
}

} // namespace ZzFluentUI
