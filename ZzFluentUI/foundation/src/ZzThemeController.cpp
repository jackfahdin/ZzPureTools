#include <ZzFluentUI/ZzThemeController.h>

#include "private/ZzThemeControllerPrivate.h"

#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>

#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzThemeController::ZzThemeController(QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<ZzThemeControllerPrivate>(this))
{
}

ZzThemeController::~ZzThemeController() = default;

ZzThemeMode ZzThemeController::mode() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->requestedMode;
}

ZzThemeMode ZzThemeController::resolvedMode() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->activeMode;
}

std::shared_ptr<const ZzThemeSnapshot> ZzThemeController::snapshot()
    const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->currentSnapshot;
}

QColor ZzThemeController::accentColor() const
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->accent;
}

bool ZzThemeController::reducedMotion() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return d_ptr->reduceMotion;
}

void ZzThemeController::setMode(ZzThemeMode mode)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (d_ptr->requestedMode == mode) {
        return;
    }
    d_ptr->requestedMode = mode;
    d_ptr->rebuild(ZzThemeChangeKind::Colors);
}

void ZzThemeController::setAccentColor(const QColor &color)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QColor valid = color.isValid()
        ? color
        : QColor(QStringLiteral("#0067c0"));
    if (d_ptr->accent == valid) {
        return;
    }
    d_ptr->accent = valid;
    d_ptr->rebuild(ZzThemeChangeKind::Colors);
}

void ZzThemeController::setReducedMotion(bool reducedMotion)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (d_ptr->reduceMotion == reducedMotion) {
        return;
    }
    d_ptr->reduceMotion = reducedMotion;
    d_ptr->rebuild(
        ZzThemeChangeKind::Motion
        | ZzThemeChangeKind::Accessibility);
}

bool ZzThemeController::eventFilter(QObject *watched, QEvent *event)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (watched == QGuiApplication::instance()
        && event != nullptr
        && event->type() == QEvent::ApplicationFontChange) {
        d_ptr->rebuild(ZzThemeChangeKind::Geometry);
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzFluentUI
