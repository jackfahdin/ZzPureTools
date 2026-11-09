#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <ZzCore/ZzErrorCode.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
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
protected:
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
            auto instance = ZzPureTools::ZzPageInstance::create(pageParent, view,
                std::make_unique<QObject>(), std::make_unique<QObject>());
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
    void configurationDefaultsAndValidation()
    {
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
        ZzPageFixture page;
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
