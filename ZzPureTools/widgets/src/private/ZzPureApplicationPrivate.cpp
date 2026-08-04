#include "ZzPureApplicationPrivate.h"

#include <algorithm>
#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QTranslator>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzThemeController.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzPureApplication.h>

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzApplicationFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    QString context = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), std::move(context)));
}

} // namespace

ZzPureApplicationPrivate::ZzPureApplicationPrivate(
    ZzPureApplication *application)
    : q_ptr(application)
    , theme(std::make_unique<ZzFluentUI::ZzThemeController>())
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }
    aboutToQuitConnection = QObject::connect(
        q_ptr,
        &QCoreApplication::aboutToQuit,
        q_ptr,
        [this] {
            beginShutdown();
        },
        Qt::DirectConnection);
}

ZzPureApplicationPrivate::~ZzPureApplicationPrivate()
{
    QObject::disconnect(aboutToQuitConnection);
    beginShutdown();
}

ZzCore::ZzResult<ZzApplicationWindow *>
ZzPureApplicationPrivate::createWindow()
{
    if (QThread::currentThread() != q_ptr->thread()) {
        return zzApplicationFailure<ZzApplicationWindow *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application called from a non-owner thread"));
    }
    if (!built || shuttingDown) {
        return zzApplicationFailure<ZzApplicationWindow *>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application is not accepting new windows"));
    }

    auto windowResult = ZzApplicationWindow::create(
        registrations, navigationNodes, initialRoute, theme.get());
    if (!windowResult) {
        return ZzCore::ZzResult<ZzApplicationWindow *>::failure(
            windowResult.error());
    }
    return adoptWindow(std::move(windowResult).value());
}

ZzCore::ZzResult<ZzApplicationWindow *>
ZzPureApplicationPrivate::adoptWindow(
    std::unique_ptr<ZzApplicationWindow> window)
{
    if (!window) {
        return zzApplicationFailure<ZzApplicationWindow *>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("application window must not be null"));
    }

    try {
        windows.reserve(windows.size() + 1);
    } catch (const std::exception &exception) {
        return zzApplicationFailure<ZzApplicationWindow *>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to reserve application window storage"),
            QString::fromLocal8Bit(exception.what()));
    } catch (...) {
        return zzApplicationFailure<ZzApplicationWindow *>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to reserve application window storage"));
    }

    const auto connection = connectWindowCloseProtocol(window.get());
    if (!connection) {
        return zzApplicationFailure<ZzApplicationWindow *>(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to connect application window close protocol"));
    }

    auto *const observer = window.get();
    windows.push_back(std::move(window));
    observer->show();
    return ZzCore::ZzResult<ZzApplicationWindow *>::success(observer);
}

QMetaObject::Connection
ZzPureApplicationPrivate::connectWindowCloseProtocol(
    ZzApplicationWindow *window)
{
    if (window == nullptr) {
        return {};
    }
    const QPointer<ZzApplicationWindow> observer(window);
    return QObject::connect(
        window,
        &ZzApplicationWindow::closeAccepted,
        q_ptr,
        [this, observer] {
            if (shuttingDown || observer.isNull()
                || !observer->consumeAcceptedClose()) {
                return;
            }
            const auto iterator = std::find_if(
                windows.begin(),
                windows.end(),
                [&observer](const auto &ownedWindow) {
                    return ownedWindow.get() == observer.data();
                });
            if (iterator != windows.end()) {
                windows.erase(iterator);
            }
        },
        Qt::QueuedConnection);
}

void ZzPureApplicationPrivate::commitBuild(
    std::unique_ptr<ZzApplicationRuntime> stagedRuntime,
    QList<ZzPageRegistration> stagedRegistrations,
    QList<ZzNavigationNode> stagedNodes,
    ZzRouteId stagedInitialRoute,
    std::vector<std::unique_ptr<QTranslator>> stagedTranslators,
    std::vector<std::unique_ptr<ZzApplicationWindow>> stagedWindows)
    noexcept
{
    runtime.swap(stagedRuntime);
    registrations.swap(stagedRegistrations);
    navigationNodes.swap(stagedNodes);
    initialRoute = std::move(stagedInitialRoute);
    translators.swap(stagedTranslators);
    windows.swap(stagedWindows);
    built = true;
    hasEverBuilt = true;
}

void ZzPureApplicationPrivate::showInitialWindow()
{
    Q_ASSERT(built && !windows.empty());
    if (built && !windows.empty()) {
        windows.front()->show();
    }
}

void ZzPureApplicationPrivate::beginShutdown() noexcept
{
    if (shuttingDown) {
        return;
    }
    shuttingDown = true;

    if (runtime) {
        runtime->requestStop();
    }
    windows.clear();
    if (runtime) {
        runtime->stop();
    }
    for (auto iterator = translators.rbegin();
         iterator != translators.rend(); ++iterator) {
        static_cast<void>(q_ptr->removeTranslator(iterator->get()));
    }
    translators.clear();
    runtime.reset();
    registrations.clear();
    navigationNodes.clear();
    initialRoute = {};
    built = false;
}

} // namespace ZzPureTools
