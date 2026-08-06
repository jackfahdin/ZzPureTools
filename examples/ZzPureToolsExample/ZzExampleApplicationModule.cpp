#include "ZzExampleApplicationModule.h"

#include <utility>

#include <QtCore/QString>

#include <ZzPureTools/ZzModuleId.h>

#include "ZzExampleApplicationModulePrivate.h"

namespace ZzExample {

ZzExampleApplicationModule::ZzExampleApplicationModule(
    std::shared_ptr<ZzExampleApplicationContext> context)
    : d_ptr(std::make_unique<ZzExampleApplicationModulePrivate>(
          std::move(context)))
{
}

ZzExampleApplicationModule::~ZzExampleApplicationModule()
{
    d_ptr->stop();
}

ZzPureTools::ZzModuleDescriptor
ZzExampleApplicationModule::descriptor() const
{
    return {
        ZzPureTools::ZzModuleId(
            QStringLiteral("example.application")),
        QStringLiteral("1.0.0"),
        {}};
}

ZzCore::ZzResult<void> ZzExampleApplicationModule::start()
{
    return d_ptr->start();
}

void ZzExampleApplicationModule::requestStop() noexcept
{
    d_ptr->requestStop();
}

void ZzExampleApplicationModule::stop() noexcept
{
    d_ptr->stop();
}

} // namespace ZzExample
