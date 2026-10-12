#include "ZzStarterSetup.h"

#include <memory>
#include <utility>
#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <ZzFluentUI/ZzSegoeIconFont.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzAuxiliaryWindowChrome.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPureApplication.h>

namespace {

/** @brief 用普通 Qt 窗口承载内容；chrome 在 QMainWindow 子树之前销毁。 */
class ZzStarterAboutWindow final : public QMainWindow
{
public:
    explicit ZzStarterAboutWindow(QWidget *owner) : QMainWindow(owner, Qt::Window) {}
    std::unique_ptr<ZzPureTools::ZzAuxiliaryWindowChrome> chrome;
};

/** @brief 创建由主窗口拥有、关闭时自动释放的辅助窗口。 */
ZzCore::ZzResult<QMainWindow *> zzCreateAboutWindow(
    QWidget *owner, ZzFluentUI::ZzThemeController *theme)
{
    auto window = std::make_unique<ZzStarterAboutWindow>(owner);
    window->setObjectName(QStringLiteral("starterAboutWindow"));
    window->setWindowTitle(QStringLiteral("About My Application"));
    window->setWindowIcon(owner->windowIcon());
    window->setWindowModality(Qt::WindowModal);
    window->setAttribute(Qt::WA_DeleteOnClose);
    auto chrome = ZzPureTools::ZzAuxiliaryWindowChrome::create(window.get(), theme);
    if (!chrome) {
        return ZzCore::ZzResult<QMainWindow *>::failure(chrome.error());
    }
    window->chrome = std::move(chrome).value();
    auto *label = new QLabel(QStringLiteral("My Application\nBuilt with ZzPureToolsFrame"), window.get());
    label->setAlignment(Qt::AlignCenter);
    window->setCentralWidget(label);
    window->resize(480, 280);
    return ZzCore::ZzResult<QMainWindow *>::success(window.release());
}

} // namespace

ZzCore::ZzResult<void> zzBuildStarter(
    ZzPureTools::ZzPureApplication &application, QSettings &settings)
{
    bool validMode = false;
    const int savedMode = settings.value(QStringLiteral("themeMode"), 0).toInt(&validMode);
    const auto mode = validMode && savedMode >= 0
            && savedMode <= static_cast<int>(ZzFluentUI::ZzThemeMode::HighContrast)
        ? static_cast<ZzFluentUI::ZzThemeMode>(savedMode)
        : ZzFluentUI::ZzThemeMode::System;
    application.themeController()->setMode(mode);

    ZzPureTools::ZzApplicationBuilder builder;
    auto configured = builder.setWindowSetupCallback(
        [&settings](ZzPureTools::ZzApplicationWindow &window) {
            window.setWindowTitle(QStringLiteral("My Application"));
            window.setWindowIcon(ZzFluentUI::ZzSegoeIconFont::icon(ZzFluentUI::ZzSegoeIcon::Home));
            QObject::connect(&window, &ZzPureTools::ZzApplicationWindow::themeToggled,
                &window, [&settings](ZzFluentUI::ZzThemeMode selected) {
                    settings.setValue(QStringLiteral("themeMode"), static_cast<int>(selected));
                    settings.sync();
                    if (settings.status() != QSettings::NoError) {
                        qWarning() << "Could not save theme settings";
                    }
                });
            return ZzCore::ZzResult<void>::success();
        });
    if (!configured) {
        return configured;
    }
    const ZzPureTools::ZzRouteId home(QStringLiteral("home"));
    ZzPureTools::ZzPageRegistration page;
    page.routeId = home;
    page.factory = [&application](QWidget *parent) {
        auto *view = new QWidget(parent);
        auto *layout = new QVBoxLayout(view);
        layout->setContentsMargins(32, 32, 32, 32);
        layout->setSpacing(16);
        auto *welcome = new QLabel(QStringLiteral("Welcome to My Application"), view);
        auto *newWindow = new QPushButton(QStringLiteral("New window"), view);
        newWindow->setObjectName(QStringLiteral("newWindowButton"));
        auto *about = new QPushButton(QStringLiteral("About"), view);
        about->setObjectName(QStringLiteral("aboutButton"));
        layout->addWidget(welcome);
        layout->addWidget(newWindow);
        layout->addWidget(about);
        layout->addStretch();
        QObject::connect(newWindow, &QPushButton::clicked, view, [&application] {
            const auto result = application.createWindow();
            if (!result) {
                qWarning().noquote() << result.error().technicalMessage();
            }
        });
        QObject::connect(about, &QPushButton::clicked, view,
            [&application, view, auxiliary = QPointer<QMainWindow>{}]() mutable {
                if (!auxiliary) {
                    const auto result = zzCreateAboutWindow(view->window(), application.themeController());
                    if (!result) {
                        qWarning().noquote() << result.error().technicalMessage();
                        return;
                    }
                    auxiliary = result.value();
                }
                auxiliary->show();
                auxiliary->raise();
                auxiliary->activateWindow();
            });
        return ZzPureTools::ZzPageInstance::create(parent, view,
            std::make_unique<QObject>(), std::make_unique<QObject>());
    };
    configured = builder.addPage(std::move(page));
    if (!configured) {
        return configured;
    }
    configured = builder.addNavigationNode({home,
        QStringLiteral("ZzApplicationStarter"), QStringLiteral("Home"), {}});
    if (!configured) {
        return configured;
    }
    configured = builder.setInitialRoute(home);
    return configured ? builder.build(application) : configured;
}
