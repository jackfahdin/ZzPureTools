#include "ZzApplicationRuntimePrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QString>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

namespace ZzPureTools {

namespace {

[[nodiscard]] QString zzModuleContext(
    const ZzModuleId &moduleId,
    QString detail = {})
{
    QString context = QStringLiteral("moduleId=%1").arg(moduleId.value());
    if (!detail.isEmpty()) {
        context.append(QStringLiteral("; "));
        context.append(std::move(detail));
    }
    return context;
}

[[nodiscard]] ZzCore::ZzResult<void> zzInvalidRuntimeState()
{
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidState,
        QStringLiteral("application runtime can only start once")));
}

} // namespace

ZzApplicationRuntimePrivate::ZzApplicationRuntimePrivate(
    std::vector<std::unique_ptr<ZzApplicationModule>> modules)
    : modules_(std::move(modules))
{
}

void ZzApplicationRuntimePrivate::setModuleIds(
    std::vector<ZzModuleId> moduleIds)
{
    Q_ASSERT(moduleIds.size() == modules_.size());
    if (moduleIds.size() != modules_.size()) {
        std::terminate();
    }
    moduleIds_ = std::move(moduleIds);
}

ZzCore::ZzResult<void> ZzApplicationRuntimePrivate::start()
{
    if (state_ != ZzApplicationRuntimeState::Built) {
        return zzInvalidRuntimeState();
    }
    Q_ASSERT(moduleIds_.size() == modules_.size());
    if (moduleIds_.size() != modules_.size()) {
        state_ = ZzApplicationRuntimeState::Stopped;
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application runtime module metadata is incomplete")));
    }

    state_ = ZzApplicationRuntimeState::Starting;
    for (std::size_t index = 0; index < modules_.size(); ++index) {
        try {
            auto result = modules_[index]->start();
            if (!result) {
                const auto errorCode = result.error().code();
                const auto technicalMessage =
                    result.error().technicalMessage();
                const auto context = zzModuleContext(
                    moduleIds_[index], result.error().context());
                rollbackStartedModules();
                return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                    errorCode, technicalMessage, context));
            }
        } catch (const std::exception &exception) {
            const auto context = zzModuleContext(
                moduleIds_[index],
                QStringLiteral("exception=%1").arg(
                    QString::fromLocal8Bit(exception.what())));
            rollbackStartedModules();
            return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("module start threw an exception"),
                context));
        } catch (...) {
            const auto context = zzModuleContext(
                moduleIds_[index], QStringLiteral("exception=unknown"));
            rollbackStartedModules();
            return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("module start threw an unknown exception"),
                context));
        }
        ++startedCount_;
    }

    state_ = ZzApplicationRuntimeState::Running;
    return ZzCore::ZzResult<void>::success();
}

void ZzApplicationRuntimePrivate::requestStop() noexcept
{
    if (state_ != ZzApplicationRuntimeState::Running) {
        return;
    }

    for (std::size_t index = startedCount_; index > 0; --index) {
        modules_[index - 1]->requestStop();
    }
    state_ = ZzApplicationRuntimeState::StopRequested;
}

void ZzApplicationRuntimePrivate::stop() noexcept
{
    if (state_ == ZzApplicationRuntimeState::Running) {
        requestStop();
    }
    if (state_ == ZzApplicationRuntimeState::Built) {
        state_ = ZzApplicationRuntimeState::Stopped;
        return;
    }
    if (state_ != ZzApplicationRuntimeState::StopRequested) {
        return;
    }

    for (std::size_t index = startedCount_; index > 0; --index) {
        modules_[index - 1]->stop();
    }
    startedCount_ = 0;
    state_ = ZzApplicationRuntimeState::Stopped;
}

bool ZzApplicationRuntimePrivate::isRunning() const noexcept
{
    return state_ == ZzApplicationRuntimeState::Running;
}

qsizetype ZzApplicationRuntimePrivate::moduleCount() const noexcept
{
    return static_cast<qsizetype>(modules_.size());
}

void ZzApplicationRuntimePrivate::rollbackStartedModules() noexcept
{
    for (std::size_t index = startedCount_; index > 0; --index) {
        modules_[index - 1]->requestStop();
    }
    for (std::size_t index = startedCount_; index > 0; --index) {
        modules_[index - 1]->stop();
    }
    startedCount_ = 0;
    state_ = ZzApplicationRuntimeState::Stopped;
}

} // namespace ZzPureTools
