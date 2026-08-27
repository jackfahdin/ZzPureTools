#include <ZzPureTools/ZzApplicationWindow.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QCloseEvent>

#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzPureTools/ZzPageHost.h>

#include "private/ZzApplicationWindowPrivate.h"

namespace ZzPureTools {

ZzApplicationWindow::ZzApplicationWindow()
    : QMainWindow(nullptr)
    , d_ptr(std::make_unique<ZzApplicationWindowPrivate>(this))
{
    setAttribute(Qt::WA_DeleteOnClose, false);
}

ZzApplicationWindow::~ZzApplicationWindow() = default;

ZzCore::ZzResult<std::unique_ptr<ZzApplicationWindow>>
ZzApplicationWindow::create(
    const QList<ZzPageRegistration> &registrations,
    const QList<ZzNavigationNode> &nodes,
    const ZzRouteId &initialRoute,
    ZzFluentUI::ZzThemeController *themeController,
    const ZzWindowSetupCallback &windowSetupCallback)
{
    auto window = std::unique_ptr<ZzApplicationWindow>(
        new ZzApplicationWindow());
    auto initialized = window->initialize(
        registrations,
        nodes,
        initialRoute,
        themeController,
        windowSetupCallback);
    if (!initialized) {
        return ZzCore::ZzResult<std::unique_ptr<
            ZzApplicationWindow>>::failure(initialized.error());
    }
    return ZzCore::ZzResult<std::unique_ptr<
        ZzApplicationWindow>>::success(std::move(window));
}

ZzCore::ZzResult<void> ZzApplicationWindow::initialize(
    const QList<ZzPageRegistration> &registrations,
    const QList<ZzNavigationNode> &nodes,
    const ZzRouteId &initialRoute,
    ZzFluentUI::ZzThemeController *themeController,
    const ZzWindowSetupCallback &windowSetupCallback)
{
    return d_ptr->initialize(
        registrations,
        nodes,
        initialRoute,
        themeController,
        windowSetupCallback);
}

ZzNavigationController *ZzApplicationWindow::navigationController()
    const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? d_ptr->controller.get() : nullptr;
}

ZzNavigationModel *ZzApplicationWindow::navigationModel() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? d_ptr->model.get() : nullptr;
}

ZzPageHost *ZzApplicationWindow::pageHost() const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? d_ptr->host : nullptr;
}

ZzFluentUI::ZzFluentTitleBar *ZzApplicationWindow::titleBar()
    const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? d_ptr->titleBar : nullptr;
}

ZzFluentUI::ZzNavigationPane *ZzApplicationWindow::navigationPane()
    const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? d_ptr->navigationPane : nullptr;
}

ZzWindowKit::ZzWindowAgent *ZzApplicationWindow::windowAgent()
    const noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    return QThread::currentThread() == thread()
        ? d_ptr->agent.get() : nullptr;
}

void ZzApplicationWindow::changeEvent(QEvent *event)
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

void ZzApplicationWindow::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);
    if (event != nullptr && event->isAccepted()
        && !d_ptr->acceptedClosePending) {
        d_ptr->acceptedClosePending = true;
        Q_EMIT closeAccepted();
    }
}

bool ZzApplicationWindow::consumeAcceptedClose() noexcept
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (QThread::currentThread() != thread()
        || !d_ptr->acceptedClosePending) {
        return false;
    }
    d_ptr->acceptedClosePending = false;
    return true;
}

} // namespace ZzPureTools
