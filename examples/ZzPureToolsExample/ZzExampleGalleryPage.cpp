#include "ZzExampleGalleryPage.h"

#include "ZzExampleGalleryPagePrivate.h"

namespace ZzExample {

ZzExampleGalleryPage::ZzExampleGalleryPage(
    ZzPageKind kind,
    const QString &title,
    QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExampleGalleryPagePrivate>(this))
{
    d_ptr->initialize(kind, title.trimmed());
}

ZzExampleGalleryPage::~ZzExampleGalleryPage() = default;

} // namespace ZzExample
