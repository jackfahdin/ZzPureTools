#include "ZzExampleSettingsWindow.h"

#include <exception>
#include <utility>

#include <QtCore/QEvent>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationWindow.h>

#include "ZzExampleSettingsWindowPrivate.h"

namespace ZzExample {

ZzCore::ZzResult<ZzExampleSettingsWindow *>
ZzExampleSettingsWindow::create(
    ZzPureTools::ZzApplicationWindow *parentWindow,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell)
{
    try {
        auto window = std::unique_ptr<ZzExampleSettingsWindow>(
            new ZzExampleSettingsWindow(parentWindow));
        auto initialized = window->initialize(
            std::move(context), application, shell);
        if (!initialized) {
            return ZzCore::ZzResult<ZzExampleSettingsWindow *>::failure(
                initialized.error());
        }
        return ZzCore::ZzResult<ZzExampleSettingsWindow *>::success(
            window.release());
    } catch (const std::exception &exception) {
        return ZzCore::ZzResult<ZzExampleSettingsWindow *>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("failed to create example settings window"),
                QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return ZzCore::ZzResult<ZzExampleSettingsWindow *>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Unknown,
                QStringLiteral("failed to create example settings window")));
    }
}

ZzExampleSettingsWindow::ZzExampleSettingsWindow(
    ZzPureTools::ZzApplicationWindow *parentWindow)
    : QMainWindow(parentWindow, Qt::Window)
    , d_ptr(std::make_unique<ZzExampleSettingsWindowPrivate>(this))
{
    setObjectName(QStringLiteral("zzExampleSettingsWindow"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
}

ZzExampleSettingsWindow::~ZzExampleSettingsWindow() = default;

ZzCore::ZzResult<void> ZzExampleSettingsWindow::initialize(
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell)
{
    return d_ptr->initialize(
        std::move(context), application, shell);
}

void ZzExampleSettingsWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    if (event->type() == QEvent::LanguageChange) {
        d_ptr->refreshTranslations();
    } else if (event->type() == QEvent::WindowStateChange) {
        d_ptr->syncWindowState();
    }
}

} // namespace ZzExample
