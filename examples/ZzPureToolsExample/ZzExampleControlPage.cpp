#include "ZzExampleControlPage.h"
#include "ZzExampleControlPagePrivate.h"
#include <QtCore/QEvent>

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

bool ZzExampleControlPage::event(QEvent *event)
{
    const bool handled = QWidget::event(event);
    if (d_ptr != nullptr && (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::StyleChange || event->type() == QEvent::DevicePixelRatioChange)) {
        d_ptr->refreshToolIcons();
    }
    return handled;
}

} // namespace ZzExample
