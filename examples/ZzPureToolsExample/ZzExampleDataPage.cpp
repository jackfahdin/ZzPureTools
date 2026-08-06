#include "ZzExampleDataPage.h"

#include "ZzExampleDataPagePrivate.h"

namespace ZzExample {

ZzExampleDataPage::ZzExampleDataPage(
    ZzExampleDataPageKind kind,
    const QString &title,
    QAbstractItemModel *model,
    QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExampleDataPagePrivate>(this))
{
    d_ptr->initialize(kind, title.trimmed(), model);
}

ZzExampleDataPage::~ZzExampleDataPage() = default;

void ZzExampleDataPage::setStatusText(const QString &text)
{
    d_ptr->setStatusText(text);
}

} // namespace ZzExample
