#include "ZzFluentControlsGallery.h"

#include <memory>

#include "ZzFluentControlsGalleryPrivate.h"

namespace ZzExamples {

ZzFluentControlsGallery::ZzFluentControlsGallery(
    ZzFluentUI::ZzThemeController *themeController,
    QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzFluentControlsGalleryPrivate>(
          this,
          themeController))
{
    setWindowTitle(QStringLiteral("ZzFluentUI Controls"));
    setMinimumSize(960, 680);
    resize(1280, 840);
}

ZzFluentControlsGallery::~ZzFluentControlsGallery() = default;

} // namespace ZzExamples
