#include "ZzExampleNavigationPresenter.h"

#include <utility>

#include <QtCore/QString>

#include "ZzExampleGalleryPage.h"

namespace ZzExample {

ZzExampleNavigationPresenter::ZzExampleNavigationPresenter(
    ZzExampleGalleryPage *view,
    ZzNavigateCallback navigate)
{
    Q_ASSERT(view != nullptr);
    Q_ASSERT(static_cast<bool>(navigate));
    QObject::connect(
        view,
        &ZzExampleGalleryPage::routeRequested,
        this,
        [navigate = std::move(navigate)](const QString &routeId) {
            if (!routeId.trimmed().isEmpty()) {
                navigate(ZzPureTools::ZzRouteId(routeId));
            }
        });
}

ZzExampleNavigationPresenter::~ZzExampleNavigationPresenter() = default;

} // namespace ZzExample
