#include "ZzStarterSetup.h"

#include <cstdlib>
#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzPureApplication.h>

namespace {

/** @brief 不依赖 QtTest，给安装消费者提供有定位信息的非零退出码。 */
bool zzRequire(bool condition, const char *message)
{
    if (!condition) {
        qCritical() << "starter smoke:" << message;
    }
    return condition;
}

} // namespace

int zzRunStarterSmoke(ZzPureTools::ZzPureApplication &application, QSettings &settings)
{
    QPointer<ZzPureTools::ZzApplicationWindow> first;
    for (auto *widget : application.topLevelWidgets()) {
        if (auto *window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(widget)) {
            first = window;
        }
    }
    if (!zzRequire(first && application.windowCount() == 1, "one initial window")
        || !zzRequire(first->titleBar()->themeMode() == ZzFluentUI::ZzThemeMode::HighContrast, "restored theme")
        || !zzRequire(first->titleBar()->title() == QStringLiteral("My Application"), "setup title")
        || !zzRequire(!first->windowIcon().isNull(), "application icon")) {
        return EXIT_FAILURE;
    }
    auto *icon = qobject_cast<QLabel *>(first->titleBar()->windowIconWidget());
    if (!zzRequire(icon && !icon->pixmap().isNull(), "rendered titlebar icon")) {
        return EXIT_FAILURE;
    }
    first->setWindowTitle(QStringLiteral("Document A"));
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(first, &languageChange);
    if (!zzRequire(first->windowTitle() == QStringLiteral("Document A")
            && first->titleBar()->title() == QStringLiteral("Document A"), "custom title after language change")) {
        return EXIT_FAILURE;
    }
    auto *newWindow = first->findChild<QPushButton *>(QStringLiteral("newWindowButton"));
    auto *aboutButton = first->findChild<QPushButton *>(QStringLiteral("aboutButton"));
    if (!zzRequire(newWindow && aboutButton, "starter page commands")) {
        return EXIT_FAILURE;
    }
    newWindow->click();
    QPointer<ZzPureTools::ZzApplicationWindow> second;
    for (auto *widget : application.topLevelWidgets()) {
        auto *window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(widget);
        if (window && window != first) {
            second = window;
        }
    }
    if (!zzRequire(second && application.windowCount() == 2, "new window command")
        || !zzRequire(second->titleBar()->title() == QStringLiteral("My Application"), "reused setup callback")) {
        return EXIT_FAILURE;
    }
    application.themeController()->setMode(ZzFluentUI::ZzThemeMode::Light);
    first->toggleTheme();
    settings.sync();
    QSettings saved(settings.fileName(), settings.format());
    if (!zzRequire(application.themeController()->mode() == ZzFluentUI::ZzThemeMode::Dark
            && second->titleBar()->themeMode() == ZzFluentUI::ZzThemeMode::Dark, "shared theme")
        || !zzRequire(saved.value(QStringLiteral("themeMode")).toInt()
            == static_cast<int>(ZzFluentUI::ZzThemeMode::Dark), "persisted theme")) {
        return EXIT_FAILURE;
    }
    aboutButton->click();
    QPointer<QMainWindow> auxiliary = first->findChild<QMainWindow *>(QStringLiteral("starterAboutWindow"));
    if (!zzRequire(auxiliary && auxiliary->isVisible(), "about command")) {
        return EXIT_FAILURE;
    }
    auto *bar = qobject_cast<ZzFluentUI::ZzFluentTitleBar *>(auxiliary->menuWidget());
    if (!zzRequire(bar && bar->title() == QStringLiteral("About My Application"), "auxiliary title")) {
        return EXIT_FAILURE;
    }
    QMetaObject::invokeMethod(bar, "closeRequested", Qt::DirectConnection);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (!zzRequire(auxiliary.isNull() && application.windowCount() == 2, "auxiliary closes independently")) {
        return EXIT_FAILURE;
    }
    second->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    if (!zzRequire(second.isNull() && application.windowCount() == 1, "secondary window close")) {
        return EXIT_FAILURE;
    }
    aboutButton->click();
    auxiliary = first->findChild<QMainWindow *>(QStringLiteral("starterAboutWindow"));
    if (!zzRequire(auxiliary && auxiliary->isVisible(), "reopened auxiliary before shutdown")) {
        return EXIT_FAILURE;
    }
    application.beginShutdown();
    if (!zzRequire(first.isNull() && auxiliary.isNull() && application.windowCount() == 0, "shutdown ownership")) {
        return EXIT_FAILURE;
    }
    qInfo() << "PASS starter: title, icon, persistence, multi-window, auxiliary, shutdown";
    return EXIT_SUCCESS;
}
