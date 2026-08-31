#include <ZzWindowKit/ZzWindowAgent.h>

#include <utility>

#include "private/ZzWindowAgentPrivate.h"
#include "private/ZzQWindowKitBackend.h"

namespace ZzWindowKit {

ZzWindowAgent::ZzWindowAgent(QObject *parent)
    : ZzWindowAgent(std::make_unique<ZzQWindowKitBackend>(), parent)
{
}

ZzWindowAgent::ZzWindowAgent(
    std::unique_ptr<ZzWindowBackend> backend,
    QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<ZzWindowAgentPrivate>(
          this, std::move(backend)))
{
}

ZzWindowAgent::~ZzWindowAgent() = default;

ZzCore::ZzResult<void> ZzWindowAgent::attach(QWidget *window)
{
    return d_ptr->attach(window);
}

ZzCore::ZzResult<void> ZzWindowAgent::configureChrome(
    const ZzWindowChromeConfiguration &configuration)
{
    return d_ptr->configureChrome(configuration);
}

ZzWindowAgentState ZzWindowAgent::state() const noexcept
{
    return d_ptr->state;
}

ZzWindowCapabilities ZzWindowAgent::capabilities() const noexcept
{
    return d_ptr->capabilities();
}

ZzCore::ZzResult<ZzWindowApplyState> ZzWindowAgent::setBackdrop(
    ZzWindowBackdrop backdrop)
{
    return d_ptr->setBackdrop(backdrop);
}

ZzCore::ZzResult<ZzWindowApplyState> ZzWindowAgent::setColorScheme(
    ZzWindowColorScheme colorScheme)
{
    return d_ptr->setColorScheme(colorScheme);
}

ZzCore::ZzResult<void> ZzWindowAgent::setAlwaysOnTop(bool alwaysOnTop)
{
    return d_ptr->setAlwaysOnTop(alwaysOnTop);
}

ZzCore::ZzResult<void> ZzWindowAgent::showSystemMenu(
    const QPoint &globalPosition)
{
    return d_ptr->showSystemMenu(globalPosition);
}

} // namespace ZzWindowKit
