#include "ZzExampleControlPage.h"
#include "ZzExampleControlPagePrivate.h"

namespace ZzExample {

ZzExampleControlPage::ZzExampleControlPage(
    ZzExampleControlKind kind, const QString &routeId, const QString &title, QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExampleControlPagePrivate>(this))
{
    setObjectName(QStringLiteral("zzExampleControlPage_%1").arg(routeId));
    setAccessibleName(title);
    d_ptr->initialize(kind, title);
}

ZzExampleControlPage::~ZzExampleControlPage() = default;

} // namespace ZzExample
