#include <ZzPureTools/ZzModuleGraphBuilder.h>

#include <utility>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationRuntime.h>

#include "private/ZzModuleGraphBuilderPrivate.h"

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzMovedBuilderFailure()
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidState,
        QStringLiteral("module graph builder has been moved")));
}

} // namespace

ZzModuleGraphBuilder::ZzModuleGraphBuilder()
    : d_ptr(std::make_unique<ZzModuleGraphBuilderPrivate>())
{
}

ZzModuleGraphBuilder::~ZzModuleGraphBuilder() = default;

ZzModuleGraphBuilder::ZzModuleGraphBuilder(
    ZzModuleGraphBuilder &&other) noexcept = default;

ZzModuleGraphBuilder &ZzModuleGraphBuilder::operator=(
    ZzModuleGraphBuilder &&other) noexcept = default;

ZzCore::ZzResult<void> ZzModuleGraphBuilder::addModule(
    std::unique_ptr<ZzApplicationModule> module)
{
    if (!d_ptr) {
        return zzMovedBuilderFailure<void>();
    }
    return d_ptr->addModule(std::move(module));
}

ZzCore::ZzResult<std::unique_ptr<ZzApplicationRuntime>>
ZzModuleGraphBuilder::build()
{
    if (!d_ptr) {
        return zzMovedBuilderFailure<
            std::unique_ptr<ZzApplicationRuntime>>();
    }
    return d_ptr->build();
}

bool ZzModuleGraphBuilder::isFrozen() const noexcept
{
    return !d_ptr || d_ptr->isFrozen();
}

} // namespace ZzPureTools
