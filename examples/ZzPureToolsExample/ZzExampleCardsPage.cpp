#include "ZzExampleCardsPage.h"

#include <QtCore/QEvent>

#include "ZzExampleCardsPagePrivate.h"

namespace ZzExample {

ZzExampleCardsPage::ZzExampleCardsPage(
    const QString &title,
    QAbstractItemModel *carouselModel,
    QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExampleCardsPagePrivate>(this))
{
    d_ptr->initialize(title.trimmed(), carouselModel);
}

ZzExampleCardsPage::~ZzExampleCardsPage() = default;

void ZzExampleCardsPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event != nullptr
        && (event->type() == QEvent::PaletteChange
            || event->type() == QEvent::StyleChange)) {
        d_ptr->refreshPalettePreviews();
    }
}

} // namespace ZzExample
