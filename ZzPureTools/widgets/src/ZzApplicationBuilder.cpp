#include <ZzPureTools/ZzApplicationBuilder.h>

#include <utility>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include "private/ZzApplicationBuilderPrivate.h"

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzMovedBuilderFailure()
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidState,
        QStringLiteral("application builder has been moved")));
}

} // namespace

ZzApplicationBuilder::ZzApplicationBuilder()
    : d_ptr(std::make_unique<ZzApplicationBuilderPrivate>())
{
}

ZzApplicationBuilder::~ZzApplicationBuilder() = default;

ZzApplicationBuilder::ZzApplicationBuilder(
    ZzApplicationBuilder &&other) noexcept = default;

ZzApplicationBuilder &ZzApplicationBuilder::operator=(
    ZzApplicationBuilder &&other) noexcept = default;

ZzCore::ZzResult<void> ZzApplicationBuilder::addModule(
    std::unique_ptr<ZzApplicationModule> module)
{
    return d_ptr
        ? d_ptr->addModule(std::move(module))
        : zzMovedBuilderFailure<void>();
}

ZzCore::ZzResult<void> ZzApplicationBuilder::addPage(
    ZzPageRegistration registration)
{
    return d_ptr
        ? d_ptr->addPage(std::move(registration))
        : zzMovedBuilderFailure<void>();
}

ZzCore::ZzResult<void> ZzApplicationBuilder::addNavigationNode(
    ZzNavigationNode node)
{
    return d_ptr
        ? d_ptr->addNavigationNode(std::move(node))
        : zzMovedBuilderFailure<void>();
}

ZzCore::ZzResult<void> ZzApplicationBuilder::setInitialRoute(
    ZzRouteId routeId)
{
    return d_ptr
        ? d_ptr->setInitialRoute(std::move(routeId))
        : zzMovedBuilderFailure<void>();
}

ZzCore::ZzResult<void> ZzApplicationBuilder::addTranslatorResource(
    QString resourcePath)
{
    return d_ptr
        ? d_ptr->addTranslatorResource(std::move(resourcePath))
        : zzMovedBuilderFailure<void>();
}

ZzCore::ZzResult<void> ZzApplicationBuilder::build(
    ZzPureApplication &application)
{
    return d_ptr
        ? d_ptr->build(application)
        : zzMovedBuilderFailure<void>();
}

bool ZzApplicationBuilder::isFrozen() const noexcept
{
    return !d_ptr || d_ptr->isFrozen();
}

} // namespace ZzPureTools
