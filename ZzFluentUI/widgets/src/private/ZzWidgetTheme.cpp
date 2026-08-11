#include "ZzWidgetTheme.h"

#include <QtCore/QtGlobal>
#include <QtGui/QPalette>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzWidgetTheme::ZzWidgetTheme(QWidget *widget)
    : widget_(widget)
{
    Q_ASSERT(widget_ != nullptr);
    refreshFallback();
}

std::shared_ptr<const ZzThemeSnapshot> ZzWidgetTheme::snapshot() const
{
    if (const auto *fluentStyle =
            qobject_cast<const ZzFluentStyle *>(widget_->style())) {
        return fluentStyle->themeSnapshot();
    }
    return fallback_;
}

void ZzWidgetTheme::refreshFallback()
{
    const QPalette palette = widget_->palette();
    const ZzThemeMode mode = palette.color(QPalette::Window).lightness() < 128
        ? ZzThemeMode::Dark
        : ZzThemeMode::Light;
    ++fallbackRevision_;
    fallback_ = std::make_shared<const ZzThemeSnapshot>(
        ZzThemeSnapshot::create(
            mode,
            palette.color(QPalette::Highlight),
            fallbackRevision_,
            false));
}

} // namespace ZzFluentUI
