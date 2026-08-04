#include "ZzFluentStylePrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconCacheKey.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzFluentStylePrivate::ZzFluentStylePrivate(
    ZzFluentStyle *q,
    ZzThemeController *themeController)
    : q_ptr(q)
    , controller(themeController)
{
    auto *application = qobject_cast<QApplication *>(
        QCoreApplication::instance());
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(themeController != nullptr);
    Q_ASSERT(themeController != nullptr
             && themeController->thread() == q_ptr->thread());
    Q_ASSERT(application != nullptr);
    if (q_ptr == nullptr
        || themeController == nullptr
        || themeController->thread() != q_ptr->thread()
        || application == nullptr) {
        std::terminate();
    }

    snapshot = themeController->snapshot();
    iconRevision = snapshot->revision();
    cache.rebuildVisuals(*snapshot);
    QObject::connect(
        themeController,
        &ZzThemeController::snapshotChanged,
        q_ptr,
        [this](quint64, ZzThemeChangeKinds changes) {
            applySnapshot(changes);
        });
}

QPixmap ZzFluentStylePrivate::iconPixmap(
    const ZzIconDescriptor &descriptor,
    QSize logicalSize,
    qreal devicePixelRatio,
    QColor color,
    Qt::LayoutDirection direction)
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    if (!descriptor.resourceId.startsWith(QStringLiteral(":/"))
        || logicalSize.isEmpty()
        || !color.isValid()) {
        return {};
    }

    const quint16 dprBucket = ZzDpiScale::bucket(devicePixelRatio);
    const qreal effectiveDpr = static_cast<qreal>(dprBucket) / 100.0;
    const bool mirrored = descriptor.mirroredInRightToLeft
        && direction == Qt::RightToLeft;
    const ZzIconCacheKey key(
        descriptor.resourceId,
        mirrored,
        logicalSize,
        dprBucket,
        color.rgba(),
        iconRevision);
    if (const QPixmap *cached = cache.icon(key); cached != nullptr) {
        return *cached;
    }

    const QSize physicalSize(
        ZzDpiScale::physicalPixels(
            logicalSize.width(), effectiveDpr),
        ZzDpiScale::physicalPixels(
            logicalSize.height(), effectiveDpr));
    if (!cache.canCacheIcon(physicalSize)) {
        return {};
    }

    QSvgRenderer renderer(descriptor.resourceId);
    if (!renderer.isValid()) {
        return {};
    }

    QImage image(
        physicalSize,
        QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return {};
    }
    image.fill(Qt::transparent);
    QPainter painter(&image);
    if (mirrored) {
        painter.translate(physicalSize.width(), 0);
        painter.scale(-1.0, 1.0);
    }
    renderer.render(
        &painter,
        QRectF(
            0.0,
            0.0,
            physicalSize.width(),
            physicalSize.height()));
    painter.end();

    QPainter tintPainter(&image);
    tintPainter.setCompositionMode(
        QPainter::CompositionMode_SourceIn);
    tintPainter.fillRect(image.rect(), color);
    tintPainter.end();

    QPixmap rendered = QPixmap::fromImage(std::move(image));
    if (rendered.isNull()) {
        return {};
    }
    rendered.setDevicePixelRatio(effectiveDpr);
    cache.insertIcon(key, rendered);
    return rendered;
}

void ZzFluentStylePrivate::applySnapshot(ZzThemeChangeKinds changes)
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    Q_ASSERT(controller != nullptr);
    if (controller == nullptr) {
        return;
    }

    snapshot = controller->snapshot();
    const bool colorsChanged = changes.testFlag(
        ZzThemeChangeKind::Colors);
    const bool geometryChanged = changes.testFlag(
        ZzThemeChangeKind::Geometry);

    if (colorsChanged) {
        cache.rebuildVisuals(*snapshot);
        cache.clearIcons();
        iconRevision = snapshot->revision();
        QApplication::setPalette(q_ptr->standardPalette());
    }
    if (!colorsChanged && !geometryChanged) {
        return;
    }

    const auto widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        if (geometryChanged) {
            QEvent event(QEvent::StyleChange);
            QCoreApplication::sendEvent(widget, &event);
            widget->updateGeometry();
        }
        widget->update();
    }
}

} // namespace ZzFluentUI
