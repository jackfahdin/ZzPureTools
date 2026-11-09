#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QPainter>
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
