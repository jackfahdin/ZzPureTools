#include "ZzExampleSmokeControllerPrivate.h"

#include <cstdlib>
#include <string_view>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListView>
#include <QtWidgets/QMessageBox>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include "ZzExampleActivityModel.h"
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

/** @brief 将路由表中的 UTF-8 常量转换为 Qt 字符串。 */
[[nodiscard]] QString zzFromUtf8(std::string_view text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

/** @brief 返回关闭场景对应的 QMessageBox 按钮角色。 */
[[nodiscard]] QMessageBox::ButtonRole zzCloseButtonRole(
    ZzExampleSmokeScenario scenario)
{
    switch (scenario) {
    case ZzExampleSmokeScenario::CloseCancel:
        return QMessageBox::RejectRole;
    case ZzExampleSmokeScenario::CloseMinimize:
        return QMessageBox::ActionRole;
    case ZzExampleSmokeScenario::CloseConfirm:
        return QMessageBox::AcceptRole;
    default:
        return QMessageBox::InvalidRole;
    }
}

} // namespace

ZzExampleSmokeControllerPrivate::ZzExampleSmokeControllerPrivate(
    bool enabled,
    ZzPureTools::ZzPureApplication *pureApplication,
    std::shared_ptr<ZzExampleApplicationContext> applicationContext)
    : application(pureApplication)
    , context(std::move(applicationContext))
    , scenario(readScenario(enabled))
{
    Q_ASSERT(application != nullptr);
    Q_ASSERT(context != nullptr);
    if (scenario == ZzExampleSmokeScenario::MultiWindow
        || scenario == ZzExampleSmokeScenario::CloseConfirm) {
        application->setQuitOnLastWindowClosed(false);
    }
}

bool ZzExampleSmokeControllerPrivate::closeGuardEnabled() const noexcept
{
    return scenario == ZzExampleSmokeScenario::Disabled
        || scenario == ZzExampleSmokeScenario::CloseCancel
        || scenario == ZzExampleSmokeScenario::CloseMinimize
        || scenario == ZzExampleSmokeScenario::CloseConfirm;
}

void ZzExampleSmokeControllerPrivate::windowAttached(
    ZzPureTools::ZzApplicationWindow &window)
{
    if (scheduled || scenario == ZzExampleSmokeScenario::Disabled) {
        return;
    }
    scheduled = true;
    switch (scenario) {
    case ZzExampleSmokeScenario::Routes:
        scheduleRouteSmoke(window);
        break;
    case ZzExampleSmokeScenario::MultiWindow:
        scheduleMultiWindowSmoke(window);
        break;
    case ZzExampleSmokeScenario::CloseCancel:
    case ZzExampleSmokeScenario::CloseMinimize:
    case ZzExampleSmokeScenario::CloseConfirm:
        scheduleCloseGuardSmoke(window);
        break;
    case ZzExampleSmokeScenario::Invalid:
        QTimer::singleShot(0, application, [this] {
            fail("unsupported smoke scenario");
        });
        break;
    case ZzExampleSmokeScenario::Disabled:
        break;
    }
}

ZzExampleSmokeScenario ZzExampleSmokeControllerPrivate::readScenario(
    bool enabled)
{
    if (!enabled) {
        return ZzExampleSmokeScenario::Disabled;
    }
    const QString value = qEnvironmentVariable(
        "ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO").trimmed();
    if (value.isEmpty() || value == QStringLiteral("routes")) {
        return ZzExampleSmokeScenario::Routes;
    }
    if (value == QStringLiteral("multi-window")) {
        return ZzExampleSmokeScenario::MultiWindow;
    }
    if (value == QStringLiteral("close-cancel")) {
        return ZzExampleSmokeScenario::CloseCancel;
    }
    if (value == QStringLiteral("close-minimize")) {
        return ZzExampleSmokeScenario::CloseMinimize;
    }
    if (value == QStringLiteral("close-confirm")) {
        return ZzExampleSmokeScenario::CloseConfirm;
    }
    return ZzExampleSmokeScenario::Invalid;
}

void ZzExampleSmokeControllerPrivate::scheduleRouteSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        auto *controller = window.navigationController();
        if (controller == nullptr) {
            fail("route smoke has no navigation controller");
            return;
        }
        for (const auto &route : ZzExampleRouteCatalog::routes()) {
            auto result = controller->navigate(
                ZzPureTools::ZzRouteId(zzFromUtf8(route.routeId)));
            if (!result) {
                fail("route smoke navigation failed");
                return;
            }
        }
    });
}

void ZzExampleSmokeControllerPrivate::scheduleMultiWindowSmoke(
    ZzPureTools::ZzApplicationWindow &firstWindow)
{
    QTimer::singleShot(0, &firstWindow, [this, &firstWindow] {
        if (application->windowCount() != 1) {
            fail("multi-window smoke did not start with one window");
            return;
        }
        auto secondResult = application->createWindow();
        if (!secondResult) {
            fail("multi-window smoke could not create a second window");
            return;
        }
        auto *secondWindow = secondResult.value();
        auto *firstNavigation = firstWindow.navigationController();
        auto *secondNavigation = secondWindow->navigationController();
        auto *firstShell = ZzExampleWindowShell::attachedTo(firstWindow);
        auto *secondShell = ZzExampleWindowShell::attachedTo(*secondWindow);
        if (application->windowCount() != 2
            || firstNavigation == nullptr || secondNavigation == nullptr
            || firstNavigation == secondNavigation
            || firstWindow.windowAgent() == nullptr
            || secondWindow->windowAgent() == nullptr
            || firstWindow.windowAgent() == secondWindow->windowAgent()
            || firstShell == nullptr || secondShell == nullptr
            || firstShell == secondShell) {
            fail("multi-window smoke found shared window-owned state");
            return;
        }

        auto *firstActivity = firstWindow.findChild<QListView *>(
            QStringLiteral("zzExampleActivityList"));
        auto *secondActivity = secondWindow->findChild<QListView *>(
            QStringLiteral("zzExampleActivityList"));
        if (firstActivity == nullptr || secondActivity == nullptr
            || firstActivity->model() != &context->activityModel()
            || secondActivity->model() != &context->activityModel()) {
            fail("multi-window smoke did not share the activity model");
            return;
        }

        const int activityRows = context->activityModel().rowCount();
        auto firstRoute = firstNavigation->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("controls")));
        auto secondRoute = secondNavigation->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("settings")));
        const bool secondDockWasVisible =
            secondShell->isActivityDockVisible();
        firstShell->setActivityDockVisible(!secondDockWasVisible);
        if (!firstRoute || !secondRoute
            || firstNavigation->currentRoute().value()
                != QStringLiteral("controls")
            || secondNavigation->currentRoute().value()
                != QStringLiteral("settings")
            || secondShell->isActivityDockVisible()
                != secondDockWasVisible
            || context->activityModel().rowCount() < activityRows + 2) {
            fail("multi-window smoke isolation assertions failed");
            return;
        }

        QPointer<ZzPureTools::ZzApplicationWindow> secondObserver(
            secondWindow);
        if (!secondWindow->close()) {
            fail("multi-window smoke could not close the second window");
            return;
        }
        QTimer::singleShot(0, application,
            [this, secondObserver] {
                if (!secondObserver.isNull()
                    || application->windowCount() != 1) {
                    fail("multi-window smoke did not erase the closed window");
                    return;
                }
                QCoreApplication::quit();
            });
    });
}

void ZzExampleSmokeControllerPrivate::scheduleCloseGuardSmoke(
    ZzPureTools::ZzApplicationWindow &window)
{
    QTimer::singleShot(0, &window, [this, &window] {
        const int activityRows = context->activityModel().rowCount();
        QTimer::singleShot(0, application,
            [this] { chooseCloseDialogButton(); });
        QPointer<ZzPureTools::ZzApplicationWindow> observer(&window);
        const bool closed = window.close();
        if (scenario == ZzExampleSmokeScenario::CloseConfirm) {
            if (!closed) {
                fail("close-confirm smoke did not accept the close event");
                return;
            }
            QTimer::singleShot(0, application,
                [this, observer, activityRows] {
                    if (!observer.isNull()
                        || application->windowCount() != 0
                        || context->activityModel().rowCount()
                            != activityRows + 1) {
                        fail("close-confirm smoke state mismatch");
                        return;
                    }
                    QCoreApplication::quit();
                });
            return;
        }

        const bool expectsMinimized =
            scenario == ZzExampleSmokeScenario::CloseMinimize;
        if (closed || application->windowCount() != 1
            || observer.isNull()
            || window.isMinimized() != expectsMinimized
            || context->activityModel().rowCount() != activityRows + 1) {
            fail("close guard smoke state mismatch");
            return;
        }
        QTimer::singleShot(0, application, [this] {
            application->beginShutdown();
            QCoreApplication::exit(EXIT_SUCCESS);
        });
    });
}

void ZzExampleSmokeControllerPrivate::chooseCloseDialogButton()
{
    auto *dialog = qobject_cast<QMessageBox *>(
        QApplication::activeModalWidget());
    if (dialog == nullptr) {
        fail("close guard smoke did not open a message box");
        return;
    }
    const QMessageBox::ButtonRole expectedRole =
        zzCloseButtonRole(scenario);
    for (QAbstractButton *button : dialog->buttons()) {
        if (dialog->buttonRole(button) == expectedRole) {
            button->click();
            return;
        }
    }
    dialog->reject();
    fail("close guard smoke could not find the expected button");
}

void ZzExampleSmokeControllerPrivate::fail(const char *reason) const
{
    qCritical().noquote() << "ZzPureToolsExample smoke failed:" << reason;
    QCoreApplication::exit(EXIT_FAILURE);
}

} // namespace ZzExample
