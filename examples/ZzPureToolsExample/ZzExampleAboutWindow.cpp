#include "ZzExampleAboutWindow.h"

#include <exception>
#include <utility>

#include <QtCore/QEvent>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationWindow.h>

#include "ZzExampleAboutWindowPrivate.h"

namespace ZzExample {

ZzCore::ZzResult<ZzExampleAboutWindow *> ZzExampleAboutWindow::create(
    ZzPureTools::ZzApplicationWindow *parentWindow,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell)
{
    try {
        auto window = std::unique_ptr<ZzExampleAboutWindow>(
            new ZzExampleAboutWindow(parentWindow));
        auto initialized = window->initialize(
            std::move(context), application, shell);
        if (!initialized) {
            return ZzCore::ZzResult<ZzExampleAboutWindow *>::failure(
                initialized.error());
        }
        return ZzCore::ZzResult<ZzExampleAboutWindow *>::success(
            window.release());
    } catch (const std::exception &exception) {
        return ZzCore::ZzResult<ZzExampleAboutWindow *>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("failed to create example about window"),
                QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return ZzCore::ZzResult<ZzExampleAboutWindow *>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("failed to create example about window")));
    }
}

ZzExampleAboutWindow::ZzExampleAboutWindow(
    ZzPureTools::ZzApplicationWindow *parentWindow)
    : QMainWindow(parentWindow, Qt::Window)
    , d_ptr(std::make_unique<ZzExampleAboutWindowPrivate>(this))
{
    setObjectName(QStringLiteral("zzExampleAboutWindow"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
}

ZzExampleAboutWindow::~ZzExampleAboutWindow() = default;

ZzCore::ZzResult<void> ZzExampleAboutWindow::initialize(
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell)
{
    return d_ptr->initialize(
        std::move(context), application, shell);
}

void ZzExampleAboutWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        d_ptr->refreshTranslations();
    }
}

} // namespace ZzExample
