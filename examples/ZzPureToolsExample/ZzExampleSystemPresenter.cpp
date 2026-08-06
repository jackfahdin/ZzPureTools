#include "ZzExampleSystemPresenter.h"

#include <utility>

#include "ZzExampleSystemPresenterPrivate.h"

namespace ZzExample {

ZzExampleSystemPresenter::ZzExampleSystemPresenter(
    ZzExampleSystemPageKind kind,
    ZzExampleSystemPage *view,
    ZzExampleSystemViewModel *viewModel,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzPureTools::ZzApplicationWindow *window,
    ZzExampleWindowShell *shell)
    : d_ptr(std::make_unique<ZzExampleSystemPresenterPrivate>(
          this,
          view,
          viewModel,
          std::move(context),
          application,
          window,
          shell))
{
    d_ptr->initialize(kind);
}

ZzExampleSystemPresenter::~ZzExampleSystemPresenter() = default;

} // namespace ZzExample
