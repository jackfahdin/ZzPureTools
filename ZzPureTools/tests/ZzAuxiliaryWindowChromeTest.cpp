#include <QtCore/QPointer>
#include <QtGui/QPlatformSurfaceEvent>
#include <QtGui/QPixmap>
#include <QtGui/QWindow>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenuBar>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzPureTools/ZzAuxiliaryWindowChrome.h>
#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowKitBootstrap.h>

namespace {

class ZzNativeSurfaceTeardownObserver final : public QObject
{
public:
    explicit ZzNativeSurfaceTeardownObserver(
        const ZzPureTools::ZzAuxiliaryWindowChrome *chrome)
        : chrome_(chrome)
    {
    }

    bool sawSurfaceDestruction = false;
    bool agentReleasedBeforeSurface = false;

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface
            && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()
                == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            sawSurfaceDestruction = true;
            agentReleasedBeforeSurface = chrome_->windowAgent() == nullptr;
        }
        return false;
    }

private:
    const ZzPureTools::ZzAuxiliaryWindowChrome *chrome_;
};

} // namespace

/** @brief 验证辅助窗口装配的标题、按钮、主题和生命周期契约。 */
class ZzAuxiliaryWindowChromeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void synchronizesHostAndTheme()
    {
        QMainWindow host;
        host.setWindowTitle(QStringLiteral("Settings"));
        ZzFluentUI::ZzThemeController theme;
        auto result = ZzPureTools::ZzAuxiliaryWindowChrome::create(&host, &theme);
        QVERIFY(result);
        auto chrome = std::move(result).value();
        auto *bar = chrome->titleBar();
        QCOMPARE(bar->title(), QStringLiteral("Settings"));
        host.setWindowTitle(QStringLiteral("Preferences"));
        QCOMPARE(bar->title(), QStringLiteral("Preferences"));
        QPixmap pixels(16, 16);
        pixels.fill(Qt::blue);
        host.setWindowIcon(QIcon(pixels));
        auto *iconLabel = qobject_cast<QLabel *>(bar->windowIconWidget());
        QVERIFY(iconLabel != nullptr);
        const QImage renderedIcon = iconLabel->pixmap().toImage();
        QVERIFY(!renderedIcon.isNull());
        QCOMPARE(renderedIcon.pixelColor(renderedIcon.width() / 2,
            renderedIcon.height() / 2), QColor(Qt::blue));
        theme.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(bar->themeMode(), ZzFluentUI::ZzThemeMode::Dark);
        QVERIFY(bar->minimizeButton()->isHidden());
        QVERIFY(bar->maximizeButton()->isHidden());
        using Button = ZzFluentUI::ZzFluentTitleBar::Button;
        QVERIFY(!bar->isButtonVisible(Button::Theme));
        QVERIFY(!bar->isButtonVisible(Button::AlwaysOnTop));
        const bool native = chrome->windowAgent()->capabilities().testFlag(
            ZzWindowKit::ZzWindowCapability::NativeSystemButtons);
        QCOMPARE(bar->closeButton()->isHidden(), native);
        host.show();
        QVERIFY(host.isVisible());
        // Exercise the shared close command even when the platform draws native buttons.
        QVERIFY(QMetaObject::invokeMethod(bar, "closeRequested", Qt::DirectConnection));
        QVERIFY(!host.isVisible());
    }

    void rejectsOccupiedHostsWithoutReplacingContent()
    {
        QMainWindow host;
        ZzFluentUI::ZzThemeController theme;
        auto *menu = new QMenuBar(&host);
        host.setMenuWidget(menu);
        QVERIFY(!ZzPureTools::ZzAuxiliaryWindowChrome::create(&host, &theme));
        QCOMPARE(host.menuWidget(), menu);
        QVERIFY(!ZzPureTools::ZzAuxiliaryWindowChrome::create(nullptr, &theme));
        QMainWindow empty;
        QVERIFY(!ZzPureTools::ZzAuxiliaryWindowChrome::create(&empty, nullptr));
        auto result = ZzPureTools::ZzAuxiliaryWindowChrome::create(&empty, &theme);
        QVERIFY(result);
        QVERIFY(!ZzPureTools::ZzAuxiliaryWindowChrome::create(&empty, &theme));
        QCOMPARE(empty.menuWidget(), result.value()->titleBar());
    }

    void releasesChromeBeforeOrAfterHost()
    {
        ZzFluentUI::ZzThemeController theme;
        auto host = std::make_unique<QMainWindow>();
        auto result = ZzPureTools::ZzAuxiliaryWindowChrome::create(host.get(), &theme);
        QVERIFY(result);
        auto chrome = std::move(result).value();
        QPointer<ZzFluentUI::ZzFluentTitleBar> bar = chrome->titleBar();
        chrome.reset();
        QVERIFY(bar.isNull());
        QVERIFY(host->menuWidget() == nullptr);
        result = ZzPureTools::ZzAuxiliaryWindowChrome::create(host.get(), &theme);
        QVERIFY(result);
        chrome = std::move(result).value();
        host.reset();
        QVERIFY(chrome->titleBar() == nullptr);
        theme.setMode(ZzFluentUI::ZzThemeMode::Light);
        chrome.reset();
    }

    void releasesAgentBeforeHiddenNativeSurface()
    {
        ZzFluentUI::ZzThemeController theme;
        auto host = std::make_unique<QMainWindow>();
        auto result = ZzPureTools::ZzAuxiliaryWindowChrome::create(host.get(), &theme);
        QVERIFY(result);
        auto chrome = std::move(result).value();
        // Cocoa creates a native host while constructing the title bar's menu.
        // Materialize the same lifetime boundary on other Qt platforms too.
        (void)host->winId();
        auto *nativeWindow = host->windowHandle();
        QVERIFY(nativeWindow != nullptr);
        nativeWindow->create();
        QVERIFY(nativeWindow->handle() != nullptr);
        QVERIFY(!host->isVisible());
        ZzNativeSurfaceTeardownObserver observer(chrome.get());
        nativeWindow->installEventFilter(&observer);

        host.reset();

        QVERIFY(observer.sawSurfaceDestruction);
        QVERIFY(observer.agentReleasedBeforeSurface);
        QVERIFY(chrome->windowAgent() == nullptr);
        QVERIFY(chrome->titleBar() == nullptr);
        theme.setMode(ZzFluentUI::ZzThemeMode::Dark);
    }
};

int main(int argc, char *argv[])
{
    const auto prepared = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!prepared) {
        return EXIT_FAILURE;
    }
    QApplication application(argc, argv);
    ZzAuxiliaryWindowChromeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzAuxiliaryWindowChromeTest.moc"
