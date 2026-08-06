#include "ZzApplicationBuilderPrivate.h"

#include <cstddef>
#include <exception>
#include <utility>

#include <QtCore/QFile>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QTranslator>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzNavigationPlacement.h>

#include <ZzPureTools/ZzApplicationRuntime.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzModuleGraphBuilder.h>
#include <ZzPureTools/ZzPureApplication.h>

#include "ZzPureApplicationPrivate.h"

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzBuilderFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), std::move(context)));
}

[[nodiscard]] bool zzIsValidPolicy(
    ZzPageLifetimePolicy policy) noexcept
{
    switch (policy) {
    case ZzPageLifetimePolicy::Persistent:
    case ZzPageLifetimePolicy::WhileActive:
    case ZzPageLifetimePolicy::Recreatable:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzIsValidNavigationPlacement(
    ZzFluentUI::ZzNavigationPlacement placement) noexcept
{
    switch (placement) {
    case ZzFluentUI::ZzNavigationPlacement::Primary:
    case ZzFluentUI::ZzNavigationPlacement::Footer:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzNavigationBadgeContainsLineBreak(
    const QString &text) noexcept
{
    for (const QChar character : text) {
        if (character == QLatin1Char('\n')
            || character == QLatin1Char('\r')
            || character == QChar::LineSeparator
            || character == QChar::ParagraphSeparator) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool zzIsValidNavigationBadge(
    const QString &badgeText) noexcept
{
    return badgeText == badgeText.trimmed()
        && badgeText.size() <= 8
        && !zzNavigationBadgeContainsLineBreak(badgeText);
}

} // namespace

ZzCore::ZzResult<void> ZzApplicationBuilderPrivate::addModule(
    std::unique_ptr<ZzApplicationModule> module)
{
    if (frozen_) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application builder is frozen"));
    }
    if (!module) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("application module must not be null"));
    }
    modules_.push_back(std::move(module));
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzApplicationBuilderPrivate::addPage(
    ZzPageRegistration registration)
{
    if (frozen_) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application builder is frozen"));
    }
    pages_.append(std::move(registration));
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzApplicationBuilderPrivate::addNavigationNode(
    ZzNavigationNode node)
{
    if (frozen_) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application builder is frozen"));
    }
    nodes_.append(std::move(node));
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzApplicationBuilderPrivate::setInitialRoute(
    ZzRouteId routeId)
{
    if (frozen_) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application builder is frozen"));
    }
    if (initialRouteSet_) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("initial route can only be set once"));
    }
    initialRoute_ = std::move(routeId);
    initialRouteSet_ = true;
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void>
ZzApplicationBuilderPrivate::addTranslatorResource(QString resourcePath)
{
    if (frozen_) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application builder is frozen"));
    }
    translatorResources_.append(std::move(resourcePath));
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzApplicationBuilderPrivate::build(
    ZzPureApplication &application)
{
    if (frozen_) {
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application builder is frozen"));
    }
    frozen_ = true;

    std::unique_ptr<ZzApplicationRuntime> stagedRuntime;
    std::vector<std::unique_ptr<QTranslator>> stagedTranslators;
    std::size_t installedTranslatorCount = 0;
    bool runtimeStarted = false;
    const auto rollback = [&] {
        if (runtimeStarted && stagedRuntime) {
            stagedRuntime->requestStop();
            stagedRuntime->stop();
            runtimeStarted = false;
        }
        while (installedTranslatorCount > 0) {
            --installedTranslatorCount;
            static_cast<void>(application.removeTranslator(
                stagedTranslators[installedTranslatorCount].get()));
        }
    };

    try {
        if (QThread::currentThread() != application.thread()) {
            return zzBuilderFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("application builder called from a non-owner thread"));
        }
        if (application.d_ptr->built
            || application.d_ptr->shuttingDown
            || application.d_ptr->hasEverBuilt) {
            return zzBuilderFailure<void>(
                ZzCore::ZzErrorCode::InvalidState,
                QStringLiteral("application cannot be built in its current state"));
        }

        QSet<ZzRouteId> pageRoutes;
        pageRoutes.reserve(pages_.size());
        for (qsizetype index = 0; index < pages_.size(); ++index) {
            const auto &page = pages_.at(index);
            if (!page.routeId.isValid()) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("page route must not be empty"),
                    QStringLiteral("index=%1").arg(index));
            }
            if (!zzIsValidPolicy(page.lifetime)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("page lifetime policy is invalid"),
                    QStringLiteral("routeId=%1")
                        .arg(page.routeId.value()));
            }
            if (!page.factory) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("page factory must not be empty"),
                    QStringLiteral("routeId=%1")
                        .arg(page.routeId.value()));
            }
            if (pageRoutes.contains(page.routeId)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("page route must be unique"),
                    QStringLiteral("routeId=%1")
                        .arg(page.routeId.value()));
            }
            pageRoutes.insert(page.routeId);
        }

        QSet<ZzRouteId> nodeRoutes;
        nodeRoutes.reserve(nodes_.size());
        qsizetype footerCount = 0;
        for (qsizetype index = 0; index < nodes_.size(); ++index) {
            const auto &node = nodes_.at(index);
            if (!node.routeId.isValid()
                || node.titleTranslationContext.trimmed().isEmpty()
                || node.titleSourceText.trimmed().isEmpty()) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("navigation node is invalid"),
                    QStringLiteral("index=%1").arg(index));
            }
            const bool hasSectionContext =
                !node.sectionTranslationContext.isEmpty();
            const bool hasSectionSource =
                !node.sectionSourceText.isEmpty();
            if (hasSectionContext != hasSectionSource
                || (hasSectionContext
                    && (node.sectionTranslationContext.trimmed().isEmpty()
                        || node.sectionSourceText.trimmed().isEmpty()))) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("navigation section translation keys must be paired and non-empty"),
                    QStringLiteral("routeId=%1")
                        .arg(node.routeId.value()));
            }
            if (!zzIsValidNavigationPlacement(node.placement)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("navigation placement is invalid"),
                    QStringLiteral("routeId=%1")
                        .arg(node.routeId.value()));
            }
            if (node.placement
                    == ZzFluentUI::ZzNavigationPlacement::Footer
                && hasSectionContext) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("footer navigation node cannot start a section"),
                    QStringLiteral("routeId=%1")
                        .arg(node.routeId.value()));
            }
            if (!zzIsValidNavigationBadge(node.badgeText)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("navigation badge must be trimmed, single-line, and at most eight UTF-16 code units"),
                    QStringLiteral("routeId=%1")
                        .arg(node.routeId.value()));
            }
            if (node.placement
                == ZzFluentUI::ZzNavigationPlacement::Footer) {
                ++footerCount;
                if (footerCount > 6) {
                    return zzBuilderFailure<void>(
                        ZzCore::ZzErrorCode::InvalidArgument,
                        QStringLiteral("navigation footer cannot contain more than six nodes"));
                }
            }
            if (nodeRoutes.contains(node.routeId)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("navigation route must be unique"),
                    QStringLiteral("routeId=%1")
                        .arg(node.routeId.value()));
            }
            if (!pageRoutes.contains(node.routeId)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::InvalidArgument,
                    QStringLiteral("navigation node has no registered page"),
                    QStringLiteral("routeId=%1")
                        .arg(node.routeId.value()));
            }
            nodeRoutes.insert(node.routeId);
        }

        if (!initialRouteSet_ || !initialRoute_.isValid()
            || !pageRoutes.contains(initialRoute_)) {
            return zzBuilderFailure<void>(
                ZzCore::ZzErrorCode::InvalidArgument,
                QStringLiteral("initial route must reference a registered page"));
        }
        for (qsizetype index = 0;
             index < translatorResources_.size(); ++index) {
            const auto &resource = translatorResources_.at(index);
            if (resource.trimmed().isEmpty() || !QFile::exists(resource)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::NotFound,
                    QStringLiteral("translator resource does not exist"),
                    QStringLiteral("index=%1; path=%2")
                        .arg(index)
                        .arg(resource));
            }
        }

        ZzModuleGraphBuilder moduleBuilder;
        for (auto &module : modules_) {
            auto addResult = moduleBuilder.addModule(std::move(module));
            if (!addResult) {
                return addResult;
            }
        }
        modules_.clear();
        auto runtimeResult = moduleBuilder.build();
        if (!runtimeResult) {
            return ZzCore::ZzResult<void>::failure(runtimeResult.error());
        }
        stagedRuntime = std::move(runtimeResult).value();

        stagedTranslators.reserve(
            static_cast<std::size_t>(translatorResources_.size()));
        for (const auto &resource : translatorResources_) {
            auto translator = std::make_unique<QTranslator>();
            if (!translator->load(resource)) {
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::Backend,
                    QStringLiteral("failed to load translator resource"),
                    resource);
            }
            stagedTranslators.push_back(std::move(translator));
        }
        for (auto &translator : stagedTranslators) {
            if (!application.installTranslator(translator.get())) {
                rollback();
                return zzBuilderFailure<void>(
                    ZzCore::ZzErrorCode::Backend,
                    QStringLiteral("failed to install translator"));
            }
            ++installedTranslatorCount;
        }

        auto startResult = stagedRuntime->start();
        if (!startResult) {
            rollback();
            return startResult;
        }
        runtimeStarted = true;

        auto windowResult = ZzApplicationWindow::create(
            pages_, nodes_, initialRoute_, application.d_ptr->theme.get());
        if (!windowResult) {
            rollback();
            return ZzCore::ZzResult<void>::failure(windowResult.error());
        }
        auto stagedWindow = std::move(windowResult).value();

        std::vector<std::unique_ptr<ZzApplicationWindow>> stagedWindows;
        stagedWindows.reserve(1);
        const auto closeConnection =
            application.d_ptr->connectWindowCloseProtocol(
                stagedWindow.get());
        if (!closeConnection) {
            rollback();
            return zzBuilderFailure<void>(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("failed to connect initial window close protocol"));
        }
        stagedWindows.push_back(std::move(stagedWindow));

        application.d_ptr->commitBuild(
            std::move(stagedRuntime),
            std::move(pages_),
            std::move(nodes_),
            std::move(initialRoute_),
            std::move(stagedTranslators),
            std::move(stagedWindows));
        runtimeStarted = false;
        installedTranslatorCount = 0;
        application.d_ptr->showInitialWindow();
        return ZzCore::ZzResult<void>::success();
    } catch (const std::exception &exception) {
        rollback();
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("application build threw an exception"),
            QString::fromLocal8Bit(exception.what()));
    } catch (...) {
        rollback();
        return zzBuilderFailure<void>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("application build threw an unknown exception"));
    }
}

bool ZzApplicationBuilderPrivate::isFrozen() const noexcept
{
    return frozen_;
}

} // namespace ZzPureTools
