#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtGui/QPainter>
#include <QtGui/QAccessible>
#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
#include "../widgets/src/private/ZzPageHostTransitionPrivate.h"
#include <functional>
#include <thread>

namespace {
using Host = ZzPureTools::ZzPageHost;
using Policy = ZzPureTools::ZzPageLifetimePolicy;
using Route = ZzPureTools::ZzRouteId;

/** @brief 真实显隐事件可重入的页面。 */
class ZzTransitionView final : public QWidget
{
public:
    explicit ZzTransitionView(QWidget *parent) : QWidget(parent) {}
    std::function<void(QEvent::Type)> callback;
    QColor color = Qt::red;
    int paints = 0;
    std::function<void()> painted;
protected:
    void paintEvent(QPaintEvent *) override
    {
        ++paints;
        QPainter painter(this);
        painter.fillRect(rect(), color);
        const auto handler = painted;
        if (handler) handler();
    }
    bool event(QEvent *event) override
    {
        const QPointer<QWidget> guard(this);
        const auto handler = callback;
        if (handler) handler(event->type());
        if (!guard) return true;
        return QWidget::event(event);
    }
};

/** @brief 保存 factory 观察值，均不拥有页面。 */
struct ZzPageFixture final
{
    QPointer<ZzTransitionView> view;
    QPointer<QWidget> parent;
    QPointer<QObject> presenter;
    QPointer<QObject> viewModel;
    QStringList events;
    QColor color = Qt::red;
    int calls = 0;
    std::function<void()> created;
    std::function<void()> canceled;
    ZzPureTools::ZzPageRegistration registration(const QString &id, Policy policy = Policy::Persistent)
    {
        ZzPureTools::ZzPageRegistration result;
        result.routeId = Route(id);
        result.lifetime = policy;
        result.factory = [this](QWidget *pageParent) {
            ++calls;
            parent = pageParent;
            view = new ZzTransitionView(pageParent);
            view->color = color;
            auto vm = std::make_unique<QObject>();
            auto pr = std::make_unique<QObject>();
            viewModel = vm.get();
            presenter = pr.get();
            QObject::connect(view, &QObject::destroyed, pageParent, [this] { events.append(QStringLiteral("view")); });
            QObject::connect(presenter, &QObject::destroyed, pageParent, [this] { events.append(QStringLiteral("presenter")); });
            QObject::connect(viewModel, &QObject::destroyed, pageParent, [this] { events.append(QStringLiteral("view-model")); });
            auto instance = ZzPureTools::ZzPageInstance::create(pageParent, view,
                std::move(vm), std::move(pr));
            if (instance) instance.value()->addCancellation([this] { events.append(QStringLiteral("cancel")); });
            if (instance && canceled) instance.value()->addCancellation(canceled);
            if (created) created();
            return instance;
        };
        return result;
    }
};
}

/** @brief 验证同步页面事务、快照及生命周期互不延迟。 */
class ZzPageTransitionTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void transitionBlocksInputAndPreservesDisabledControls()
    {
        ZzPageFixture first, second;
        Host host;
        host.resize(200, 100);
        host.show();
        host.activateWindow();
        QCoreApplication::processEvents();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.setTransitionDuration(1000));
        QVERIFY(host.activate(first.registration(QStringLiteral("a"))));
        auto *input = new QLineEdit(first.view);
        input->show();
        input->setFocus();
        QPointer<QPushButton> button;
        QPointer<QLineEdit> disabled;
        int clicks = 0;
        second.created = [&] {
            button = new QPushButton(QStringLiteral("Target"), second.view);
            disabled = new QLineEdit(second.view);
            disabled->setEnabled(false);
            QObject::connect(button, &QPushButton::clicked, &host, [&] { ++clicks; });
        };
        QVERIFY(host.activate(second.registration(QStringLiteral("b"))));
        auto *overlay = host.findChild<QWidget *>(QStringLiteral("zzAnimatedStackOverlay"));
        QVERIFY(overlay && overlay->isVisible());
        QTest::mouseClick(overlay, Qt::LeftButton);
        QTest::keyClick(&host, Qt::Key_Tab);
        QTest::keyClick(&host, Qt::Key_Space);
        QCOMPARE(clicks, 0);
        QVERIFY(host.hasFocus());
        QVERIFY(!button->isVisible());
        host.findChild<QVariantAnimation *>()->setCurrentTime(1000);
        QVERIFY(button->isVisible());
        QVERIFY(!disabled->isEnabled());
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(clicks, 1);
    }

    void targetMovedDuringSourceCaptureFailsSafely()
    {
        ZzPageFixture first, second;
        QWidget elsewhere;
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        const auto a = first.registration(QStringLiteral("a"));
        QVERIFY(host.activate(a));
        first.view->painted = [&] { if (second.view) second.view->setParent(&elsewhere); };
        QVERIFY(!host.activate(second.registration(QStringLiteral("b"))));
        QCOMPARE(host.currentRoute(), a.routeId);
        QVERIFY(!second.view);
        QVERIFY(first.view->isVisible());
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
    }

    void externalSourceDestructionDuringTargetCaptureFallsBack()
    {
        ZzPageFixture first, second;
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.activate(first.registration(QStringLiteral("a"))));
        second.created = [&] { second.view->painted = [&] { delete first.view.data(); }; };
        const auto b = second.registration(QStringLiteral("b"));
        QVERIFY(host.activate(b));
        QCOMPARE(host.currentRoute(), b.routeId);
        QVERIFY(!host.isTransitionRunning());
        QVERIFY(second.view->isVisible());
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
        QCoreApplication::processEvents();
        QVERIFY(!first.presenter);
    }

    void independentHostsKeepTheirOwnFrames()
    {
        ZzPageFixture first, second, third, fourth;
        Host one, two;
        one.resize(200, 100);
        two.resize(200, 100);
        one.show();
        two.show();
        QCoreApplication::processEvents();
        QVERIFY(one.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(two.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(one.activate(first.registration(QStringLiteral("a"))));
        QVERIFY(one.activate(second.registration(QStringLiteral("b"))));
        const auto bytes = ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&one);
        QVERIFY(bytes > 0);
        QVERIFY(two.activate(third.registration(QStringLiteral("c"))));
        QVERIFY(two.activate(fourth.registration(QStringLiteral("d"))));
        two.deactivateCurrent();
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&one), bytes);
        QVERIFY(one.isTransitionRunning());
        QCOMPARE(one.currentRoute(), Route(QStringLiteral("b")));
    }

    void thousandTransitionsKeepBoundedResources()
    {
        ZzPageFixture pages[4];
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setRecreatableCapacity(1));
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.setTransitionDuration(1000));
        for (int i = 0; i < 1000; ++i) {
            const int index = i % 4;
            const auto page = pages[index].registration(QString::number(index), Policy::Recreatable);
            QVERIFY(host.activate(page));
            QCOMPARE(host.currentRoute(), page.routeId);
            int live = 0;
            for (const auto &fixture : pages) if (fixture.view) ++live;
            QCOMPARE(live, std::min(i + 1, 2));
            const auto bytes = ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host);
            QVERIFY(bytes <= 64 * 1024 * 1024);
            QCOMPARE(bytes > 0, i > 0);
        }
        QCOMPARE(host.findChildren<QVariantAnimation *>().size(), 1);
        host.findChild<QVariantAnimation *>()->setCurrentTime(1000);
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
    }

    void disabledRouteAndNativeViewSkipCapture_data()
    {
        QTest::addColumn<bool>("native");
        QTest::newRow("route") << false;
        QTest::newRow("native") << true;
    }
    void disabledRouteAndNativeViewSkipCapture()
    {
        QFETCH(bool, native);
        ZzPageFixture first, second;
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.activate(first.registration(QStringLiteral("a"))));
        const auto b = second.registration(QStringLiteral("b"));
        if (native) second.created = [&] { second.view->setAttribute(Qt::WA_NativeWindow); };
        else QVERIFY(host.setRouteTransitionEnabled(b.routeId, false));
        QVERIFY(host.activate(b));
        QVERIFY(!host.isTransitionRunning());
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
        QVERIFY(second.view->isVisible());
    }

    void viewDestructionCanDestroyHost()
    {
        ZzPageFixture page;
        auto *host = new Host;
        QPointer<Host> guard(host);
        QVERIFY(host->activate(page.registration(QStringLiteral("a"))));
        QObject::connect(page.view, &QObject::destroyed, [&] { delete host; });
        delete page.view.data();
        QVERIFY(!guard);
        QVERIFY(!page.view);
        QVERIFY(!page.presenter);
        QVERIFY(!page.viewModel);
    }

    void environmentSettlesCommittedRoute_data()
    {
        QTest::addColumn<int>("change");
        for (int i = 0; i < 17; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
    }
    void environmentSettlesCommittedRoute()
    {
        QFETCH(int, change);
        ZzPageFixture first, second;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        Host host;
        host.setStyle(&style);
        host.resize(200, 100);
        host.show();
        QCoreApplication::processEvents();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.setTransitionDuration(1000));
        const auto a = first.registration(QStringLiteral("a"));
        const auto b = second.registration(QStringLiteral("b"));
        QVERIFY(host.activate(a));
        QVERIFY(host.activate(b));
        QVERIFY(host.isTransitionRunning());
        switch (change) {
        case 0: host.resize(220, 120); break;
        case 1: host.hide(); break;
        case 2: host.setEnabled(false); break;
        case 3: host.setLayoutDirection(Qt::RightToLeft); break;
        case 4: QVERIFY(host.setTransitionDuration(0)); break;
        case 5: QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::None)); break;
        case 6: { QEvent e(QEvent::WindowDeactivate); QApplication::sendEvent(&host, &e); break; }
        case 7: { QEvent e(QEvent::DevicePixelRatioChange); QApplication::sendEvent(&host, &e); break; }
        case 8: { QEvent e(QEvent::PaletteChange); QApplication::sendEvent(&host, &e); break; }
        case 9: { QEvent e(QEvent::StyleChange); QApplication::sendEvent(&host, &e); break; }
        case 10: { QEvent e(QEvent::FontChange); QApplication::sendEvent(&host, &e); break; }
        case 11: { QEvent e(QEvent::LanguageChange); QApplication::sendEvent(&host, &e); break; }
        case 12: controller.setReducedMotion(true); QCoreApplication::processEvents(); break;
        case 13: host.setContentsMargins(5, 7, 9, 11); break;
        case 14: QVERIFY(host.setRouteTransitionEnabled(a.routeId, false)); break;
        case 15: QVERIFY(host.setRouteTransitionEnabled(b.routeId, false)); break;
        case 16:
            QVERIFY(host.setRouteTransitionEnabled(Route(QStringLiteral("unrelated")), false));
            QVERIFY(host.isTransitionRunning());
            host.findChild<QVariantAnimation *>()->setCurrentTime(1000);
            break;
        }
        QCOMPARE(host.currentRoute(), b.routeId);
        QVERIFY(!host.isTransitionRunning());
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
        QVERIFY(first.view->isHidden());
        QVERIFY(!second.view->isHidden());
    }

    void rendersFrames_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("backward");
        QTest::addColumn<bool>("rtl");
        for (int mode = 1; mode <= 3; ++mode) for (bool backward : {false, true}) for (bool rtl : {false, true}) {
            const auto name = QStringLiteral("%1-%2-%3").arg(mode).arg(backward).arg(rtl).toLatin1();
            QTest::newRow(name.constData()) << mode << backward << rtl;
        }
    }
    void rendersFrames()
    {
        QFETCH(int, mode);
        QFETCH(bool, backward);
        QFETCH(bool, rtl);
        ZzPageFixture first, second;
        second.color = Qt::blue;
        Host host;
        host.setContentsMargins(5, 7, 9, 11);
        host.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        host.resize(214, 118);
        host.show();
        QVERIFY(host.setTransitionMode(static_cast<Host::ZzTransitionMode>(mode)));
        QVERIFY(host.setTransitionDuration(200));
        QVERIFY(host.activate(first.registration(QStringLiteral("a"), Policy::WhileActive)));
        QVERIFY(host.activate(second.registration(QStringLiteral("b")), backward
            ? Host::ZzTransitionDirection::Backward : Host::ZzTransitionDirection::Forward));
        QVERIFY(host.isTransitionRunning());
        auto *animation = host.findChild<QVariantAnimation *>();
        animation->pause();
        animation->setCurrentTime(100);
        const auto frame = host.grab().toImage();
        const auto colorAt = [&](QPoint point) {
            point += host.contentsRect().topLeft();
            return frame.pixelColor(qRound(point.x() * frame.devicePixelRatio()), qRound(point.y() * frame.devicePixelRatio()));
        };
        if (mode == 3) {
            const auto color = colorAt(QPoint(100, 50));
            QVERIFY(qAbs(color.red() - 32) <= 1);
            QVERIFY(qAbs(color.blue() - 223) <= 1);
        } else {
            const bool reverse = backward != (mode == 1 && rtl);
            const QPoint near = mode == 1 ? QPoint(5, 50) : QPoint(100, 5);
            const QPoint far = mode == 1 ? QPoint(195, 50) : QPoint(100, 95);
            QCOMPARE(colorAt(reverse ? far : near), QColor(Qt::red));
            QCOMPARE(colorAt(reverse ? near : far), QColor(Qt::blue));
        }
        animation->setCurrentTime(200);
        QCOMPARE(second.view->geometry(), QRect(0, 0, 200, 100));
    }

    void interruptedCaptureCommitsWithoutFrames()
    {
        ZzPageFixture first, second;
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.activate(first.registration(QStringLiteral("a"))));
        second.created = [&] { second.view->painted = [&] { host.resize(220, 120); }; };
        const auto b = second.registration(QStringLiteral("b"));
        QVERIFY(host.activate(b));
        QCOMPARE(host.currentRoute(), b.routeId);
        QVERIFY(!host.isTransitionRunning());
        QVERIFY(second.view->isVisible());
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
    }

    void focusedTargetAndAccessibilityRecover()
    {
        ZzPageFixture first, second;
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *host = new Host(&window);
        layout->addWidget(host);
        window.resize(300, 200);
        window.show();
        window.activateWindow();
        QCoreApplication::processEvents();
        QVERIFY(host->setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host->setTransitionDuration(1000));
        QVERIFY(host->activate(first.registration(QStringLiteral("a"))));
        auto *input = new QLineEdit(first.view);
        input->show();
        input->setFocus();
        QPointer<QLineEdit> targetInput;
        second.created = [&] {
            second.view->setAccessibleName(QStringLiteral("target"));
            targetInput = new QLineEdit(second.view);
        };
        QVERIFY(host->activate(second.registration(QStringLiteral("b"))));
        QVERIFY(host->hasFocus());
        auto *accessible = QAccessible::queryAccessibleInterface(second.view);
        QVERIFY(accessible);
        QVERIFY(accessible->state().invisible);
        const int paints = second.view->paints;
        auto *animation = host->findChild<QVariantAnimation *>();
        animation->pause();
        for (int t : {100, 200, 500, 800}) animation->setCurrentTime(t);
        QCOMPARE(second.view->paints, paints);
        animation->setCurrentTime(1000);
        QVERIFY(!accessible->state().invisible);
        QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("target"));
        QCOMPARE(QApplication::focusWidget(), targetInput.data());
    }

    void nestedRunningStackFallsBack()
    {
        ZzPageFixture first, second;
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.activate(first.registration(QStringLiteral("a"))));
        auto *nested = new ZzFluentUI::ZzAnimatedStackedWidget(first.view);
        nested->addWidget(new QWidget);
        nested->addWidget(new QWidget);
        nested->resize(150, 80);
        nested->show();
        nested->setDuration(1000);
        QVERIFY(nested->setCurrentIndex(1));
        QVERIFY(nested->isTransitionRunning());
        QVERIFY(host.activate(second.registration(QStringLiteral("b"))));
        QVERIFY(!host.isTransitionRunning());
        QVERIFY(!nested->isTransitionRunning());
    }

    void capturePreservesExternalFocus_data()
    {
        QTest::addColumn<bool>("source");
        QTest::newRow("source") << true;
        QTest::newRow("target") << false;
    }
    void capturePreservesExternalFocus()
    {
        QFETCH(bool, source);
        ZzPageFixture first, second;
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *host = new Host(&window);
        auto *external = new QLineEdit(&window);
        layout->addWidget(host);
        layout->addWidget(external);
        window.resize(300, 250);
        window.show();
        window.activateWindow();
        QCoreApplication::processEvents();
        QVERIFY(host->setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host->setTransitionDuration(1000));
        QVERIFY(host->activate(first.registration(QStringLiteral("a"))));
        auto *input = new QLineEdit(first.view);
        input->show();
        input->setFocus();
        QCOMPARE(QApplication::focusWidget(), input);
        if (source) first.view->painted = [&] { external->setFocus(); };
        else second.created = [&] { second.view->painted = [&] { external->setFocus(); }; };
        QVERIFY(host->activate(second.registration(QStringLiteral("b"))));
        QCOMPARE(QApplication::focusWidget(), external);
        auto *animation = host->findChild<QVariantAnimation *>();
        animation->setCurrentTime(animation->duration());
        QCOMPARE(QApplication::focusWidget(), external);
    }

    void externalDestructionRejectsReentry_data()
    {
        QTest::addColumn<bool>("early");
        QTest::newRow("factory-connection") << true;
        QTest::newRow("later-connection") << false;
    }
    void externalDestructionRejectsReentry()
    {
        QFETCH(bool, early);
        ZzPageFixture first, second;
        Host host;
        const auto b = second.registration(QStringLiteral("b"));
        bool rejected = false;
        const auto observe = [&] {
            QObject::connect(first.view, &QObject::destroyed, &host, [&] {
                auto result = host.activate(b);
                rejected = !result && result.error().code() == ZzCore::ZzErrorCode::InvalidState;
            });
        };
        if (early) first.created = observe;
        QVERIFY(host.activate(first.registration(QStringLiteral("a"), Policy::WhileActive)));
        if (!early) observe();
        delete first.view.data();
        QVERIFY(rejected);
        QVERIFY(host.activate(b));
    }

    void explicitPageMinimumSizeIsPreserved()
    {
        ZzPageFixture page;
        Host host;
        QVERIFY(host.activate(page.registration(QStringLiteral("a"))));
        page.view->setMinimumSize(400, 300);
        QVERIFY(host.minimumSizeHint().width() >= 400);
        QVERIFY(host.minimumSizeHint().height() >= 300);
    }

    void lifecycleCommitsBeforePlayback_data()
    {
        QTest::addColumn<int>("policy");
        QTest::addColumn<int>("capacity");
        QTest::addColumn<int>("mode");
        for (int policy = 0; policy < 3; ++policy) {
            for (int capacity : {0, 1, 3}) {
                for (int mode = 0; mode < 4; ++mode) {
                    const auto name = QStringLiteral("%1-%2-%3").arg(policy).arg(capacity).arg(mode).toLatin1();
                    QTest::newRow(name.constData()) << policy << capacity << mode;
                }
            }
        }
    }
    void lifecycleCommitsBeforePlayback()
    {
        QFETCH(int, policy);
        QFETCH(int, capacity);
        QFETCH(int, mode);
        ZzPageFixture first, second;
        second.color = Qt::blue;
        Host host;
        host.resize(200, 100);
        host.show();
        QCoreApplication::processEvents();
        QVERIFY(host.setRecreatableCapacity(capacity));
        QVERIFY(host.setTransitionMode(static_cast<Host::ZzTransitionMode>(mode)));
        QVERIFY(host.setTransitionDuration(200));
        const auto lifetime = static_cast<Policy>(policy);
        QVERIFY(host.activate(first.registration(QStringLiteral("a"), lifetime)));
        const auto b = second.registration(QStringLiteral("b"));
        QVERIFY(host.activate(b));
        QCOMPARE(host.currentRoute(), b.routeId);
        const bool destroyed = lifetime == Policy::WhileActive || (lifetime == Policy::Recreatable && capacity == 0);
        QCOMPARE(first.view.isNull(), destroyed);
        QCOMPARE(first.presenter.isNull(), destroyed);
        QCOMPARE(first.viewModel.isNull(), destroyed);
        if (destroyed) QCOMPARE(first.events, QStringList({QStringLiteral("cancel"), QStringLiteral("view"),
            QStringLiteral("presenter"), QStringLiteral("view-model")}));
        QCOMPARE(host.isTransitionRunning(), mode != 0);
        const auto bytes = ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host);
        QCOMPARE(bytes > 0, mode != 0);
        QVERIFY(bytes <= 64 * 1024 * 1024);
        if (mode != 0) {
            QVERIFY(!second.view->isVisible());
            const auto animations = host.findChildren<QVariantAnimation *>();
            QCOMPARE(animations.size(), 1);
            animations.first()->pause();
            animations.first()->setCurrentTime(100);
            const QImage frame = host.grab().toImage();
            const QColor pixel = frame.pixelColor(qRound(5 * frame.devicePixelRatio()), qRound(5 * frame.devicePixelRatio()));
            if (mode != static_cast<int>(Host::ZzTransitionMode::Fade)) QCOMPARE(pixel, QColor(Qt::red));
            else QVERIFY(pixel.red() > 0 && pixel.blue() > 0);
            animations.first()->setCurrentTime(200);
        }
        QVERIFY(!host.isTransitionRunning());
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
        QVERIFY(second.view->isVisible());
    }

    void failedFactoryKeepsRunningSegment()
    {
        ZzPageFixture first, second;
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.setTransitionDuration(1000));
        QVERIFY(host.activate(first.registration(QStringLiteral("a"))));
        const auto b = second.registration(QStringLiteral("b"));
        QVERIFY(host.activate(b));
        auto *animation = host.findChild<QVariantAnimation *>();
        animation->pause();
        animation->setCurrentTime(100);
        ZzPureTools::ZzPageRegistration failed;
        failed.routeId = Route(QStringLiteral("failed"));
        failed.factory = [](QWidget *) {
            return ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>>::failure(
                ZzCore::ZzError(ZzCore::ZzErrorCode::Backend, QStringLiteral("expected failure")));
        };
        QVERIFY(!host.activate(failed));
        QCOMPARE(host.currentRoute(), b.routeId);
        QVERIFY(host.isTransitionRunning());
        QCOMPARE(animation->currentTime(), 100);
        QVERIFY(host.activate(b));
        QCOMPARE(animation->currentTime(), 100);
        QVERIFY(host.showFrameworkError(failed.routeId));
        QVERIFY(!host.isTransitionRunning());
        QCOMPARE(ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host), 0);
    }

    void rapidActivationIsSynchronous()
    {
        ZzPageFixture pages[4];
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.setTransitionDuration(1000));
        for (int i = 0; i < 4; ++i) {
            const auto registration = pages[i].registration(QString::number(i), Policy::WhileActive);
            QVERIFY(host.activate(registration));
            QCOMPARE(host.currentRoute(), registration.routeId);
            if (i) {
                QVERIFY(!pages[i - 1].view);
                QVERIFY(host.isTransitionRunning());
            }
        }
        auto *animation = host.findChild<QVariantAnimation *>();
        QVERIFY(animation);
        animation->setCurrentTime(1000);
        QVERIFY(pages[3].view->isVisible());
    }

    void configurationDefaultsAndValidation()
    {
        ZzPageFixture page;
        Host host;
        QCOMPARE(host.transitionMode(), Host::ZzTransitionMode::None);
        QCOMPARE(host.focusPolicy(), Qt::NoFocus);
        QCOMPARE(host.transitionDuration(), -1);
        QVERIFY(!host.isTransitionRunning());
        QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
        QVERIFY(host.setTransitionDuration(200));
        QVERIFY(!host.setTransitionMode(static_cast<Host::ZzTransitionMode>(99)));
        QVERIFY(!host.setTransitionDuration(-2));
        QVERIFY(!host.setTransitionDuration(10001));
        QCOMPARE(host.transitionMode(), Host::ZzTransitionMode::Fade);
        QCOMPARE(host.transitionDuration(), 200);
        QVERIFY(!host.setRouteTransitionEnabled({}, false));
        QVERIFY(!host.isRouteTransitionEnabled({}));
        const Route route(QStringLiteral("a"));
        QVERIFY(host.isRouteTransitionEnabled(route));
        QVERIFY(host.setRouteTransitionEnabled(route, false));
        const auto a = page.registration(route.value(), Policy::WhileActive);
        QVERIFY(host.activate(a));
        host.deactivateCurrent();
        QVERIFY(host.activate(a));
        QVERIFY(!host.isRouteTransitionEnabled(route));
        QVERIFY(host.setRouteTransitionEnabled(route, true));
        QVERIFY(host.isRouteTransitionEnabled(route));
    }

    void factoryCanDestroyHost()
    {
        ZzPageFixture page;
        auto *host = new Host;
        QPointer<Host> guard(host);
        page.created = [&] { delete host; };
        auto result = host->activate(page.registration(QStringLiteral("a")));
        QVERIFY(!result);
        QVERIFY(!guard);
        QVERIFY(!page.view);
    }

    void cancellationCanDestroyHost()
    {
        ZzPageFixture first, second;
        auto *host = new Host;
        QPointer<Host> guard(host);
        first.canceled = [&] { delete host; };
        QVERIFY(host->activate(first.registration(QStringLiteral("a"), Policy::WhileActive)));
        auto result = host->activate(second.registration(QStringLiteral("b")));
        QVERIFY(!result);
        QVERIFY(!guard);
        QVERIFY(!first.view);
        QVERIFY(!second.view);
    }

    void crossThreadConfigurationIsRejected()
    {
        Host host;
        bool rejected = false;
        std::thread worker([&] {
            auto result = host.setTransitionDuration(200);
            rejected = !result && result.error().code() == ZzCore::ZzErrorCode::InvalidState;
            rejected = rejected && host.transitionMode() == Host::ZzTransitionMode::None
                && host.transitionDuration() == -1 && !host.currentRoute().isValid();
        });
        worker.join();
        QVERIFY(rejected);
    }

    void hideCanDestroyTarget()
    {
        ZzPageFixture first, second;
        Host host;
        host.resize(200, 100);
        host.show();
        QVERIFY(host.activate(first.registration(QStringLiteral("a"))));
        first.view->callback = [&](QEvent::Type type) {
            if (type == QEvent::Hide) delete second.view.data();
        };
        auto result = host.activate(second.registration(QStringLiteral("b")));
        QVERIFY(!result);
        QVERIFY(!second.view);
        QVERIFY(host.currentRoute() != Route(QStringLiteral("b")));
        QVERIFY(host.activate(second.registration(QStringLiteral("b"))));
    }

    void factoryRejectsReentry()
    {
        ZzPageFixture first, second;
        Host host;
        const auto a = first.registration(QStringLiteral("a"));
        const auto b = second.registration(QStringLiteral("b"));
        bool rejected = false;
        first.created = [&] {
            auto nested = host.activate(b);
            rejected = !nested && nested.error().code() == ZzCore::ZzErrorCode::InvalidState;
            rejected = rejected && !host.setTransitionDuration(100)
                && !host.setRecreatableCapacity(0) && !host.showFrameworkError(b.routeId)
                && !host.setRouteTransitionEnabled(a.routeId, false);
        };
        QVERIFY(host.activate(a));
        QVERIFY(rejected);
        QCOMPARE(second.calls, 0);
        QCOMPARE(host.currentRoute(), a.routeId);
    }

    void hideRejectsReentry()
    {
        ZzPageFixture first, second, third;
        Host host;
        host.resize(200, 100);
        host.show();
        const auto a = first.registration(QStringLiteral("a"));
        const auto b = second.registration(QStringLiteral("b"));
        const auto c = third.registration(QStringLiteral("c"));
        QVERIFY(host.activate(a));
        bool rejected = false;
        first.view->callback = [&](QEvent::Type type) {
            if (type != QEvent::Hide) return;
            auto nested = host.activate(c);
            rejected = !nested && nested.error().code() == ZzCore::ZzErrorCode::InvalidState;
            host.deactivateCurrent();
        };
        QVERIFY(host.activate(b));
        QVERIFY(rejected);
        QCOMPARE(third.calls, 0);
        QCOMPARE(host.currentRoute(), b.routeId);
        QVERIFY(second.view->isVisible());
    }
};

QTEST_MAIN(ZzPageTransitionTest)
#include "ZzPageTransitionTest.moc"
