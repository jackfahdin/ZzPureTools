#include "ZzExampleSettingsWindowPrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzAuxiliaryWindowChrome.h>
#include <ZzPureTools/ZzPureApplication.h>


#include "ZzExampleApplicationContext.h"
#include "ZzExampleSettingsWindow.h"
#include "ZzExampleSystemPage.h"
#include "ZzExampleSystemPageKind.h"
#include "ZzExampleSystemPresenter.h"
#include "ZzExampleSystemViewModel.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

[[nodiscard]] ZzCore::ZzResult<void> zzSettingsWindowFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<void>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

} // namespace

ZzExampleSettingsWindowPrivate::ZzExampleSettingsWindowPrivate(
    ZzExampleSettingsWindow *window)
    : q_ptr(window)
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }
}

ZzExampleSettingsWindowPrivate::~ZzExampleSettingsWindowPrivate()
{
    presenter.reset();
    viewModel.reset();
    chrome.reset();
}

ZzCore::ZzResult<void> ZzExampleSettingsWindowPrivate::initialize(
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell)
{
    auto *const applicationWindow = qobject_cast<
        ZzPureTools::ZzApplicationWindow *>(q_ptr->parentWidget());
    if (initialized) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("example settings window is already initialized"));
    }
    if (applicationWindow == nullptr || !context || application == nullptr
        || shell == nullptr || application->themeController() == nullptr) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("example settings window requires parent, context, application and shell"));
    }
    if (ZzExampleWindowShell::attachedTo(*applicationWindow) != shell) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("example settings window shell must belong to its parent window"));
    }
    if (QThread::currentThread() != q_ptr->thread()
        || applicationWindow->thread() != q_ptr->thread()
        || application->thread() != q_ptr->thread()
        || shell->thread() != q_ptr->thread()) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("example settings window inputs must use the GUI thread"));
    }

    parentWindow = applicationWindow;
    viewModel = std::make_unique<ZzExampleSystemViewModel>();
    page = new ZzExampleSystemPage(
        ZzExampleSystemPageKind::Settings,
        QCoreApplication::translate("ZzPureToolsExample", "设置"),
        viewModel.get(),
        q_ptr);
    q_ptr->setCentralWidget(page);
    presenter = std::make_unique<ZzExampleSystemPresenter>(
        ZzExampleSystemPageKind::Settings,
        page,
        viewModel.get(),
        std::move(context),
        application,
        applicationWindow,
        shell);
    q_ptr->setMinimumSize(640, 520);
    q_ptr->resize(760, 620);

    auto chromeResult = ZzPureTools::ZzAuxiliaryWindowChrome::create(
        q_ptr, application->themeController());
    if (!chromeResult) {
        return ZzCore::ZzResult<void>::failure(chromeResult.error());
    }
    chrome = std::move(chromeResult).value();

    refreshTranslations();
    initialized = true;
    return ZzCore::ZzResult<void>::success();
}

void ZzExampleSettingsWindowPrivate::refreshTranslations()
{
    const QString title = QCoreApplication::translate(
        "ZzPureToolsExample", "设置");
    q_ptr->setWindowTitle(title);
}

} // namespace ZzExample
