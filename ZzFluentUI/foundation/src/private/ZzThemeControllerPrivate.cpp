#include "ZzThemeControllerPrivate.h"

#include <QtCore/QThread>
#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>

#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzThemeControllerPrivate::ZzThemeControllerPrivate(
    ZzThemeController *q)
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(QGuiApplication::instance() != nullptr);
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());

    activeMode = resolveSystemMode();
    currentSnapshot = std::make_shared<const ZzThemeSnapshot>(
        ZzThemeSnapshot::create(
            activeMode,
            accent,
            revision,
            reduceMotion));
    if (QGuiApplication::instance() != nullptr) {
        QGuiApplication::instance()->installEventFilter(q_ptr);
    }
    if (QGuiApplication::styleHints() != nullptr) {
        colorSchemeConnection = QObject::connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            q_ptr,
            [this] {
                if (requestedMode == ZzThemeMode::System) {
                    rebuild(ZzThemeChangeKind::Colors);
                }
            });
    }
}

ZzThemeMode ZzThemeControllerPrivate::resolveSystemMode() const noexcept
{
    if (requestedMode != ZzThemeMode::System) {
        return requestedMode;
    }
    const QStyleHints *hints = QGuiApplication::styleHints();
    return hints != nullptr
            && hints->colorScheme() == Qt::ColorScheme::Dark
        ? ZzThemeMode::Dark
        : ZzThemeMode::Light;
}

void ZzThemeControllerPrivate::rebuild(ZzThemeChangeKinds changes)
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    activeMode = resolveSystemMode();
    const quint64 nextRevision = revision + 1;
    auto next = std::make_shared<const ZzThemeSnapshot>(
        ZzThemeSnapshot::create(
            activeMode,
            accent,
            nextRevision,
            reduceMotion));
    currentSnapshot.swap(next);
    revision = nextRevision;
    Q_EMIT q_ptr->snapshotChanged(revision, changes);
}

} // namespace ZzFluentUI
