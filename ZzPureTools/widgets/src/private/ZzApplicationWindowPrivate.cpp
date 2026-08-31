#include "ZzApplicationWindowPrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QModelIndex>
#include <QtCore/QThread>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzThemeController.h>

#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowCapability.h>
#include <ZzWindowKit/ZzWindowChromeConfiguration.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzPageHost.h>

namespace ZzPureTools {

namespace {

[[nodiscard]] ZzCore::ZzResult<void> zzWindowFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        code, std::move(message)));
}

void zzLogNavigationError(const ZzCore::ZzError &error)
{
    qWarning().noquote()
        << "application window navigation failed:"
        << error.technicalMessage()
        << error.context();
}

} // namespace

ZzApplicationWindowPrivate::ZzApplicationWindowPrivate(
    ZzApplicationWindow *window)
    : q_ptr(window)
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }
}

ZzApplicationWindowPrivate::~ZzApplicationWindowPrivate()
{
    controller.reset();
    model.reset();
    agent.reset();
}

ZzCore::ZzResult<void> ZzApplicationWindowPrivate::initialize(
    const QList<ZzPageRegistration> &registrations,
    const QList<ZzNavigationNode> &nodes,
    const ZzRouteId &initialRoute,
    ZzFluentUI::ZzThemeController *themeController,
    const ZzWindowSetupCallback &windowSetupCallback)
{
    if (initialized) {
        return zzWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application window is already initialized"));
    }
    if (QThread::currentThread() != q_ptr->thread()) {
        return zzWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("application window called from a non-owner thread"));
    }
    if (themeController == nullptr
        || themeController->thread() != q_ptr->thread()) {
        return zzWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("theme controller must exist in the window thread"));
    }
    if (!initialRoute.isValid()) {
        return zzWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("initial page route must not be empty"));
    }

    theme = themeController;
    titleBar = new ZzFluentUI::ZzFluentTitleBar(q_ptr);
    titleBar->setThemeMode(themeController->mode());
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::themeModeRequested,
        themeController,
        &ZzFluentUI::ZzThemeController::setMode);
    QObject::connect(
        themeController,
        &ZzFluentUI::ZzThemeController::snapshotChanged,
        q_ptr,
        [this] {
            if (titleBar != nullptr && theme != nullptr) {
                titleBar->setThemeMode(theme->mode());
            }
        });
    syncWindowIcon();
    QObject::connect(
        q_ptr,
        &QWidget::windowIconChanged,
        q_ptr,
        [this](const QIcon &) { syncWindowIcon(); });
    q_ptr->setMenuWidget(titleBar);
    body = new QWidget(q_ptr);
    bodyIdentity = body;
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    navigationPane = new ZzFluentUI::ZzNavigationPane(body);
    host = new ZzPageHost(body);
    bodyLayout->addWidget(navigationPane);
    bodyLayout->addWidget(host, 1);
    q_ptr->setCentralWidget(body);
    q_ptr->resize(1100, 720);

    model = std::make_unique<ZzNavigationModel>();
    auto modelResult = model->setNodes(nodes);
    if (!modelResult) {
        return modelResult;
    }
    navigationPane->setModel(model.get());

    controller = std::make_unique<ZzNavigationController>(
        model.get(), host);
    auto registrationsResult = controller->setRegistrations(registrations);
    if (!registrationsResult) {
        return registrationsResult;
    }

    QObject::connect(
        navigationPane,
        &ZzFluentUI::ZzNavigationPane::navigationRequested,
        q_ptr,
        [this](const QModelIndex &index) {
            auto nodeResult = model->nodeAt(index.row());
            if (!nodeResult) {
                zzLogNavigationError(nodeResult.error());
                return;
            }
            const ZzRouteId routeId =
                std::move(nodeResult).value().routeId;
            auto navigationResult = controller->navigate(routeId);
            if (!navigationResult) {
                zzLogNavigationError(navigationResult.error());
            }
        });
    QObject::connect(
        controller.get(),
        &ZzNavigationController::currentRouteChanged,
        q_ptr,
        [this](const ZzRouteId &) { syncNavigationSelection(); });
    QObject::connect(
        model.get(),
        &QAbstractItemModel::modelReset,
        q_ptr,
        [this] { syncNavigationSelection(); });

    if (windowSetupCallback) {
        try {
            auto setupResult = windowSetupCallback(*q_ptr);
            if (!setupResult) {
                return setupResult;
            }
        } catch (const std::exception &exception) {
            return zzWindowFailure(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("window setup callback threw an exception: %1")
                    .arg(QString::fromLocal8Bit(exception.what())));
        } catch (...) {
            return zzWindowFailure(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("window setup callback threw an unknown exception"));
        }
    }

    agent = std::make_unique<ZzWindowKit::ZzWindowAgent>();
    auto attachResult = agent->attach(q_ptr);
    if (!attachResult) {
        return attachResult;
    }

    const auto capabilities = agent->capabilities();
    const bool nativeSystemButtons = capabilities.testFlag(
        ZzWindowKit::ZzWindowCapability::NativeSystemButtons);
    titleBar->setSystemButtonsVisible(!nativeSystemButtons);
    ZzWindowKit::ZzWindowChromeConfiguration chrome;
    chrome.titleBar = titleBar;
    chrome.windowIcon = titleBar->windowIconWidget();
    chrome.interactiveWidgets = titleBar->hitTestVisibleWidgets();
    if (!nativeSystemButtons) {
        chrome.minimizeButton = titleBar->minimizeButton();
        chrome.maximizeButton = titleBar->maximizeButton();
        chrome.closeButton = titleBar->closeButton();
    }
    auto chromeResult = agent->configureChrome(chrome);
    if (!chromeResult) {
        return chromeResult;
    }

    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::minimizeRequested,
        q_ptr,
        &QWidget::showMinimized);
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::maximizeRestoreRequested,
        q_ptr,
        [this] {
            if (q_ptr->isMaximized()) {
                q_ptr->showNormal();
            } else {
                q_ptr->showMaximized();
            }
        });
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::closeRequested,
        q_ptr,
        &QWidget::close);

    refreshTranslations();
    syncWindowState();
    auto navigationResult = controller->navigate(initialRoute);
    if (!navigationResult) {
        return navigationResult;
    }

    initialized = true;
    return ZzCore::ZzResult<void>::success();
}

void ZzApplicationWindowPrivate::refreshTranslations()
{
    const QString title = QCoreApplication::translate(
        "ZzApplicationWindow", "ZzPureTools");
    q_ptr->setWindowTitle(title);
    if (titleBar != nullptr) {
        titleBar->setTitle(title);
    }
    if (model) {
        model->refreshTranslations();
    }
}

void ZzApplicationWindowPrivate::syncWindowState()
{
    if (titleBar != nullptr) {
        titleBar->setMaximized(q_ptr->isMaximized());
    }
}

void ZzApplicationWindowPrivate::syncWindowIcon()
{
    if (titleBar != nullptr) {
        titleBar->setWindowIcon(q_ptr->windowIcon());
    }
}

void ZzApplicationWindowPrivate::syncNavigationSelection()
{
    if (navigationPane == nullptr || !model || !controller) {
        return;
    }
    auto indexResult = model->indexForRoute(controller->currentRoute());
    navigationPane->setCurrentSourceIndex(
        indexResult ? indexResult.value() : QModelIndex());
}

} // namespace ZzPureTools
