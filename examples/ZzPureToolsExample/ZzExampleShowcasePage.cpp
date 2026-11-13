#include "ZzExampleShowcasePage.h"

#include "ZzExampleShowcasePagePrivate.h"

#include "ZzExampleLanguageController.h"

namespace ZzExample {

ZzExampleShowcasePage::ZzExampleShowcasePage(
    ZzPageKind kind,
    const QString &title,
    QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExampleShowcasePagePrivate>(this))
{
    ZzExampleTranslationScope translationScope(this);
    d_ptr->initialize(kind, title.trimmed());
}

ZzExampleShowcasePage::~ZzExampleShowcasePage() = default;

} // namespace ZzExample
