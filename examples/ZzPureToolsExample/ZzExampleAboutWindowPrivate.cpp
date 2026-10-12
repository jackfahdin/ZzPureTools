#include "ZzExampleAboutWindowPrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzAuxiliaryWindowChrome.h>
#include <ZzPureTools/ZzPureApplication.h>


#include "ZzExampleAboutWindow.h"
#include "ZzExampleApplicationContext.h"
#include "ZzExampleSystemPage.h"
#include "ZzExampleSystemPageKind.h"
#include "ZzExampleSystemPresenter.h"
#include "ZzExampleSystemViewModel.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

[[nodiscard]] ZzCore::ZzResult<void> zzAboutWindowFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<void>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

} // namespace

ZzExampleAboutWindowPrivate::ZzExampleAboutWindowPrivate(
    ZzExampleAboutWindow *window)
    : q_ptr(window)
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }
}

ZzExampleAboutWindowPrivate::~ZzExampleAboutWindowPrivate()
{
    presenter.reset();
    viewModel.reset();
    chrome.reset();
}

ZzCore::ZzResult<void> ZzExampleAboutWindowPrivate::initialize(
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell)
{
    auto *const applicationWindow = qobject_cast<
        ZzPureTools::ZzApplicationWindow *>(q_ptr->parentWidget());
    if (initialized) {
        return zzAboutWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("example about window is already initialized"));
    }
    if (applicationWindow == nullptr || !context || application == nullptr
        || shell == nullptr) {
        return zzAboutWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("example about window requires parent, context, application and shell"));
    }
    if (ZzExampleWindowShell::attachedTo(*applicationWindow) != shell) {
        return zzAboutWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("example about window shell must belong to its parent window"));
    }
    if (QThread::currentThread() != q_ptr->thread()
        || applicationWindow->thread() != q_ptr->thread()
        || application->thread() != q_ptr->thread()
        || shell->thread() != q_ptr->thread()) {
        return zzAboutWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("example about window inputs must use the GUI thread"));
    }

    parentWindow = applicationWindow;
    q_ptr->setWindowIcon(applicationWindow->windowIcon());
    viewModel = std::make_unique<ZzExampleSystemViewModel>();
    page = new ZzExampleSystemPage(
        ZzExampleSystemPageKind::About,
        QCoreApplication::translate("ZzPureToolsExample", "关于"),
        viewModel.get(),
        q_ptr);
    q_ptr->setCentralWidget(page);
    presenter = std::make_unique<ZzExampleSystemPresenter>(
        ZzExampleSystemPageKind::About,
        page,
        viewModel.get(),
        std::move(context),
        application,
        applicationWindow,
        shell);
    q_ptr->setMinimumSize(680, 480);
    q_ptr->resize(820, 560);

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

void ZzExampleAboutWindowPrivate::refreshTranslations()
{
    const QString title = QCoreApplication::translate(
        "ZzPureToolsExample", "关于 ZzPureTools");
    q_ptr->setWindowTitle(title);
}

} // namespace ZzExample
