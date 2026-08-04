#include "ZzDemoModule.h"

#include <QtCore/QString>

#include <ZzPureTools/ZzModuleId.h>

ZzPureTools::ZzModuleDescriptor ZzDemoModule::descriptor() const
{
    return {
        ZzPureTools::ZzModuleId(QStringLiteral("demo.presentation")),
        QStringLiteral("1.0.0"),
        {}};
}

ZzCore::ZzResult<void> ZzDemoModule::start()
{
    started_ = true;
    stopRequested_ = false;
    return ZzCore::ZzResult<void>::success();
}

void ZzDemoModule::requestStop() noexcept
{
    if (started_) {
        stopRequested_ = true;
    }
}

void ZzDemoModule::stop() noexcept
{
    if (started_ && !stopRequested_) {
        stopRequested_ = true;
    }
    started_ = false;
}
