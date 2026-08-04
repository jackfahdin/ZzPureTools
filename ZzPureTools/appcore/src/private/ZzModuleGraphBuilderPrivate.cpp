#include "ZzModuleGraphBuilderPrivate.h"

#include <cstddef>
#include <deque>
#include <exception>
#include <typeinfo>
#include <utility>
#include <vector>

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationRuntime.h>
#include <ZzPureTools/ZzModuleDescriptor.h>

#include "ZzApplicationRuntimePrivate.h"

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzGraphFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), std::move(context)));
}

[[nodiscard]] QString zzModuleContext(
    qsizetype registrationIndex,
    const ZzModuleId &moduleId)
{
    return QStringLiteral("registrationIndex=%1; moduleId=%2")
        .arg(registrationIndex)
        .arg(moduleId.value());
}

[[nodiscard]] QString zzDescriptorExceptionContext(
    qsizetype registrationIndex,
    const ZzApplicationModule &module,
    QString exception)
{
    return QStringLiteral("registrationIndex=%1; type=%2; exception=%3")
        .arg(registrationIndex)
        .arg(QString::fromLatin1(typeid(module).name()))
        .arg(std::move(exception));
}

} // namespace

ZzCore::ZzResult<void> ZzModuleGraphBuilderPrivate::addModule(
    std::unique_ptr<ZzApplicationModule> module)
{
    if (frozen_) {
        return zzGraphFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("module graph builder is frozen"));
    }
    if (!module) {
        return zzGraphFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("module must not be null"));
    }

    modules_.push_back(std::move(module));
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<std::unique_ptr<ZzApplicationRuntime>>
ZzModuleGraphBuilderPrivate::build()
{
    using ZzRuntimePointer = std::unique_ptr<ZzApplicationRuntime>;

    if (frozen_) {
        return zzGraphFailure<ZzRuntimePointer>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("module graph builder is frozen"));
    }
    frozen_ = true;

    std::vector<ZzModuleDescriptor> descriptors;
    descriptors.reserve(modules_.size());
    QHash<ZzModuleId, qsizetype> moduleIndexes;
    moduleIndexes.reserve(static_cast<qsizetype>(modules_.size()));

    for (std::size_t index = 0; index < modules_.size(); ++index) {
        auto &module = modules_[index];
        if (!module) {
            return zzGraphFailure<ZzRuntimePointer>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("module must not be null"),
                QStringLiteral("registrationIndex=%1").arg(index));
        }

        ZzModuleDescriptor descriptor;
        try {
            descriptor = module->descriptor();
        } catch (const std::exception &exception) {
            return zzGraphFailure<ZzRuntimePointer>(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("module descriptor threw an exception"),
                zzDescriptorExceptionContext(
                    static_cast<qsizetype>(index),
                    *module,
                    QString::fromLocal8Bit(exception.what())));
        } catch (...) {
            return zzGraphFailure<ZzRuntimePointer>(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("module descriptor threw an unknown exception"),
                zzDescriptorExceptionContext(
                    static_cast<qsizetype>(index),
                    *module,
                    QStringLiteral("unknown")));
        }

        const auto registrationIndex = static_cast<qsizetype>(index);
        if (!descriptor.id.isValid()) {
            return zzGraphFailure<ZzRuntimePointer>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("module id must not be empty"),
                QStringLiteral("registrationIndex=%1")
                    .arg(registrationIndex));
        }
        if (descriptor.version.trimmed().isEmpty()) {
            return zzGraphFailure<ZzRuntimePointer>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("module version must not be empty"),
                zzModuleContext(registrationIndex, descriptor.id));
        }
        if (moduleIndexes.contains(descriptor.id)) {
            return zzGraphFailure<ZzRuntimePointer>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("module id must be unique"),
                zzModuleContext(registrationIndex, descriptor.id));
        }

        QSet<ZzModuleId> uniqueDependencies;
        uniqueDependencies.reserve(descriptor.dependencies.size());
        for (const auto &dependency : descriptor.dependencies) {
            if (!dependency.isValid()) {
                return zzGraphFailure<ZzRuntimePointer>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("module dependency id must not be empty"),
                    zzModuleContext(registrationIndex, descriptor.id));
            }
            if (dependency == descriptor.id) {
                return zzGraphFailure<ZzRuntimePointer>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("module must not depend on itself"),
                    zzModuleContext(registrationIndex, descriptor.id));
            }
            if (uniqueDependencies.contains(dependency)) {
                return zzGraphFailure<ZzRuntimePointer>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("module dependency must be unique"),
                    zzModuleContext(registrationIndex, descriptor.id));
            }
            uniqueDependencies.insert(dependency);
        }

        moduleIndexes.insert(descriptor.id, registrationIndex);
        descriptors.push_back(std::move(descriptor));
    }

    std::vector<std::vector<qsizetype>> dependents(modules_.size());
    std::vector<qsizetype> indegrees(modules_.size(), 0);
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto &descriptor = descriptors[index];
        for (const auto &dependency : descriptor.dependencies) {
            const auto dependencyIterator = moduleIndexes.constFind(dependency);
            if (dependencyIterator == moduleIndexes.cend()) {
                return zzGraphFailure<ZzRuntimePointer>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("module dependency was not registered"),
                    QStringLiteral("moduleId=%1; dependencyId=%2")
                        .arg(descriptor.id.value(), dependency.value()));
            }
            dependents[static_cast<std::size_t>(dependencyIterator.value())]
                .push_back(static_cast<qsizetype>(index));
            ++indegrees[index];
        }
    }

    std::deque<qsizetype> ready;
    for (std::size_t index = 0; index < indegrees.size(); ++index) {
        if (indegrees[index] == 0) {
            ready.push_back(static_cast<qsizetype>(index));
        }
    }

    std::vector<qsizetype> orderedIndexes;
    orderedIndexes.reserve(modules_.size());
    while (!ready.empty()) {
        const auto index = ready.front();
        ready.pop_front();
        orderedIndexes.push_back(index);
        for (const auto dependent :
             dependents[static_cast<std::size_t>(index)]) {
            auto &indegree = indegrees[static_cast<std::size_t>(dependent)];
            --indegree;
            if (indegree == 0) {
                ready.push_back(dependent);
            }
        }
    }

    if (orderedIndexes.size() != modules_.size()) {
        QStringList cycleModules;
        for (std::size_t index = 0; index < indegrees.size(); ++index) {
            if (indegrees[index] > 0) {
                cycleModules.append(descriptors[index].id.value());
            }
        }
        return zzGraphFailure<ZzRuntimePointer>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("module dependency graph contains a cycle"),
            QStringLiteral("moduleIds=%1").arg(cycleModules.join(',')));
    }

    std::vector<std::unique_ptr<ZzApplicationModule>> orderedModules;
    std::vector<ZzModuleId> orderedIds;
    orderedModules.reserve(modules_.size());
    orderedIds.reserve(modules_.size());
    for (const auto index : orderedIndexes) {
        const auto storageIndex = static_cast<std::size_t>(index);
        orderedModules.push_back(std::move(modules_[storageIndex]));
        orderedIds.push_back(descriptors[storageIndex].id);
    }
    modules_.clear();

    auto runtime = ZzRuntimePointer(
        new ZzApplicationRuntime(std::move(orderedModules)));
    runtime->d_ptr->setModuleIds(std::move(orderedIds));
    return ZzCore::ZzResult<ZzRuntimePointer>::success(
        std::move(runtime));
}

bool ZzModuleGraphBuilderPrivate::isFrozen() const noexcept
{
    return frozen_;
}

} // namespace ZzPureTools
