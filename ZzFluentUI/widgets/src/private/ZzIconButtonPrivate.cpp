#include "ZzIconButtonPrivate.h"

#include <algorithm>

#include <QtGui/QIcon>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>

namespace ZzFluentUI {

ZzIconButtonPrivate::ZzIconButtonPrivate(
    ZzIconButton *publicObject) noexcept
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzIconButtonPrivate::refreshIcon()
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        return;
    }
    if (!hasDescriptor) {
        q_ptr->setIcon(QIcon());
        return;
    }
    auto *fluentStyle = qobject_cast<ZzFluentStyle *>(q_ptr->style());
    if (fluentStyle == nullptr) {
        q_ptr->setIcon(QIcon());
        return;
    }

    const int extent = std::max(
        1,
        std::min(q_ptr->width(), q_ptr->height()) - 12);
    const QSize logicalSize(extent, extent);
    const QPalette::ColorGroup group = q_ptr->isEnabled()
        ? QPalette::Normal
        : QPalette::Disabled;
    const QColor paletteColor = q_ptr->palette().color(
        group,
        QPalette::ButtonText);
    ZzIconDescriptor effectiveDescriptor = descriptor;
    if (iconColor.isValid()) {
        effectiveDescriptor.colorMode = ZzIconColorMode::Custom;
        effectiveDescriptor.customColor = iconColor;
    }
    const QPixmap pixmap = fluentStyle->iconPixmap(
        effectiveDescriptor,
        logicalSize,
        q_ptr->devicePixelRatioF(),
        paletteColor,
        q_ptr->layoutDirection());
    q_ptr->setIcon(pixmap.isNull() ? QIcon() : QIcon(pixmap));
    q_ptr->setIconSize(logicalSize);
}

} // namespace ZzFluentUI
