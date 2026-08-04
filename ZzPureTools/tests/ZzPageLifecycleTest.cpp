#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include <QtCore/QMetaMethod>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtTest/QTest>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzRouteId.h>

namespace {

/** @brief 在析构或断开信号时记录精确顺序的测试展示对象。 */
class ZzPageProbeObject final : public QObject
{
    Q_OBJECT

public:
    ZzPageProbeObject(
        QStringList *events,
        QString destructionEvent,
        bool recordDisconnect = false)
        : events_(events)
        , destructionEvent_(std::move(destructionEvent))
        , recordDisconnect_(recordDisconnect)
    {
    }

    ~ZzPageProbeObject() override
    {
        if (events_ != nullptr && !destructionEvent_.isEmpty()) {
            events_->append(destructionEvent_);
        }
    }

Q_SIGNALS:
    void presentationChanged();

protected:
    void disconnectNotify(const QMetaMethod &signal) override
    {
        if (recordDisconnect_ && !disconnectRecorded_
            && events_ != nullptr) {
            disconnectRecorded_ = true;
            events_->append(QStringLiteral("disconnect"));
        }
        QObject::disconnectNotify(signal);
    }

private:
    QStringList *events_;
    QString destructionEvent_;
    bool recordDisconnect_;
    bool disconnectRecorded_ = false;
};

/** @brief 在析构时记录事件并维护活动 View 数量的测试页面。 */
class ZzPageProbeWidget final : public QWidget
{
public:
    ZzPageProbeWidget(
        QWidget *parent,
        QStringList *events = nullptr,
        QString destructionEvent = {},
        int *liveViews = nullptr)
        : QWidget(parent)
        , events_(events)
        , destructionEvent_(std::move(destructionEvent))
        , liveViews_(liveViews)
    {
        if (liveViews_ != nullptr) {
            ++(*liveViews_);
        }
    }

    ~ZzPageProbeWidget() override
    {
        if (events_ != nullptr && !destructionEvent_.isEmpty()) {
            events_->append(destructionEvent_);
        }
        if (liveViews_ != nullptr) {
            --(*liveViews_);
        }
    }

private:
    QStringList *events_;
    QString destructionEvent_;
    int *liveViews_;
};

[[nodiscard]] ZzCore::ZzResult<std::unique_ptr<
    ZzPureTools::ZzPageInstance>> zzCreateOrderedPage(
        QWidget *pageParent,
        QStringList *events)
{
    auto *view = new ZzPageProbeWidget(
        pageParent, events, QStringLiteral("view"));
    auto viewModel = std::make_unique<ZzPageProbeObject>(
        events, QStringLiteral("view-model"));
    auto presenter = std::make_unique<ZzPageProbeObject>(
        events, QStringLiteral("presenter"), true);
    QObject::connect(
        presenter.get(),
        &ZzPageProbeObject::presentationChanged,
        view,
        [] {});
    return ZzPureTools::ZzPageInstance::create(
        pageParent,
        view,
        std::move(viewModel),
        std::move(presenter));
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzCountingRegistration(
    QString route,
    ZzPureTools::ZzPageLifetimePolicy policy,
    int *factoryCalls,
    int *liveViews,
    QWidget **lastView = nullptr)
{
    ZzPureTools::ZzPageRegistration registration;
    registration.routeId = ZzPureTools::ZzRouteId(std::move(route));
    registration.lifetime = policy;
    registration.factory =
        [factoryCalls, liveViews, lastView](QWidget *pageParent) {
            ++(*factoryCalls);
            auto *view = new ZzPageProbeWidget(
                pageParent, nullptr, {}, liveViews);
            if (lastView != nullptr) {
                *lastView = view;
            }
            return ZzPureTools::ZzPageInstance::create(
                pageParent,
                view,
                std::make_unique<QObject>(),
                std::make_unique<QObject>());
        };
    return registration;
}

[[nodiscard]] ZzCore::ZzResult<std::unique_ptr<
    ZzPureTools::ZzPageInstance>> zzFactoryFailure()
{
    return ZzCore::ZzResult<std::unique_ptr<
        ZzPureTools::ZzPageInstance>>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("test page factory failed")));
}

} // namespace

/** @brief 验证页面单一所有权、销毁顺序和有界缓存策略。 */
class ZzPageLifecycleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // 被测工厂在失败路径销毁页面树；静态分析器不了解该所有权契约。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void rejectsViewWithDifferentPageParent()
    {
        QWidget expectedParent;
        QWidget differentParent;
        QPointer<QWidget> view = new ZzPageProbeWidget(&differentParent);
        auto viewModel = std::make_unique<ZzPageProbeObject>(nullptr, QString{});
        auto presenter = std::make_unique<ZzPageProbeObject>(nullptr, QString{});
        QPointer<QObject> viewModelPointer = viewModel.get();
        QPointer<QObject> presenterPointer = presenter.get();

        auto result = ZzPureTools::ZzPageInstance::create(
            &expectedParent,
            view.data(),
            std::move(viewModel),
            std::move(presenter));

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(view.isNull());
        QVERIFY(viewModelPointer.isNull());
        QVERIFY(presenterPointer.isNull());
    }

    void rejectsParentedPresentationObjectsWithoutDoubleDelete()
    {
        QWidget pageParent;
        QPointer<QWidget> view = new ZzPageProbeWidget(&pageParent);
        auto viewModel = std::make_unique<ZzPageProbeObject>(nullptr, QString{});
        auto presenter = std::make_unique<ZzPageProbeObject>(nullptr, QString{});
        viewModel->setParent(view.data());
        presenter->setParent(viewModel.get());
        QPointer<QObject> viewModelPointer = viewModel.get();
        QPointer<QObject> presenterPointer = presenter.get();

        auto result = ZzPureTools::ZzPageInstance::create(
            &pageParent,
            view.data(),
            std::move(viewModel),
            std::move(presenter));

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(view.isNull());
        QVERIFY(viewModelPointer.isNull());
        QVERIFY(presenterPointer.isNull());
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void destroysViewBeforePresenterAndViewModel()
    {
        QStringList events;
        QWidget pageParent;
        auto result = zzCreateOrderedPage(&pageParent, &events);
        QVERIFY(result);
        auto instance = std::move(result).value();

        instance.reset();

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("disconnect"),
                QStringLiteral("view"),
                QStringLiteral("presenter"),
                QStringLiteral("view-model")}));
    }

    void cancelsTasksBeforeDestroyingPresentationObjects()
    {
        QStringList events;
        QWidget pageParent;
        auto result = zzCreateOrderedPage(&pageParent, &events);
        QVERIFY(result);
        auto instance = std::move(result).value();
        instance->addCancellation([&events] {
            events.append(QStringLiteral("cancel"));
        });

        instance->prepareForDestruction();
        instance->prepareForDestruction();
        instance->addCancellation([&events] {
            events.append(QStringLiteral("late-cancel"));
        });

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("cancel"),
                QStringLiteral("disconnect"),
                QStringLiteral("view"),
                QStringLiteral("presenter"),
                QStringLiteral("view-model"),
                QStringLiteral("late-cancel")}));
    }

    void cancellationExceptionDoesNotSkipDestruction()
    {
        QStringList events;
        QWidget pageParent;
        auto result = zzCreateOrderedPage(&pageParent, &events);
        QVERIFY(result);
        auto instance = std::move(result).value();
        instance->addCancellation([&events] {
            events.append(QStringLiteral("cancel"));
            throw std::runtime_error("cancellation failure");
        });
        instance->addCancellation([&events] {
            events.append(QStringLiteral("cancel-next"));
        });
        QTest::ignoreMessage(
            QtWarningMsg,
            "page cancellation callback threw an exception: cancellation failure");

        instance->prepareForDestruction();

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("cancel"),
                QStringLiteral("cancel-next"),
                QStringLiteral("disconnect"),
                QStringLiteral("view"),
                QStringLiteral("presenter"),
                QStringLiteral("view-model")}));
    }

    void persistentPageIsReused()
    {
        int persistentCalls = 0;
        int otherCalls = 0;
        int liveViews = 0;
        QWidget *firstView = nullptr;
        ZzPureTools::ZzPageHost host;
        const auto persistent = zzCountingRegistration(
            QStringLiteral("persistent"),
            ZzPureTools::ZzPageLifetimePolicy::Persistent,
            &persistentCalls,
            &liveViews,
            &firstView);
        const auto other = zzCountingRegistration(
            QStringLiteral("other"),
            ZzPureTools::ZzPageLifetimePolicy::WhileActive,
            &otherCalls,
            &liveViews);

        QVERIFY(host.activate(persistent));
        const QPointer<QWidget> originalView = firstView;
        QVERIFY(host.activate(other));
        QVERIFY(host.activate(persistent));

        QCOMPARE(persistentCalls, 1);
        QCOMPARE(otherCalls, 1);
        QCOMPARE(firstView, originalView.data());
        QCOMPARE(host.currentRoute(), persistent.routeId);
    }

    void whileActivePageIsDestroyedOnLeave()
    {
        int activeCalls = 0;
        int otherCalls = 0;
        int liveViews = 0;
        QWidget *activeView = nullptr;
        ZzPureTools::ZzPageHost host;
        const auto active = zzCountingRegistration(
            QStringLiteral("active"),
            ZzPureTools::ZzPageLifetimePolicy::WhileActive,
            &activeCalls,
            &liveViews,
            &activeView);
        const auto other = zzCountingRegistration(
            QStringLiteral("other"),
            ZzPureTools::ZzPageLifetimePolicy::Persistent,
            &otherCalls,
            &liveViews);

        QVERIFY(host.activate(active));
        const QPointer<QWidget> firstView = activeView;
        QVERIFY(host.activate(other));
        QVERIFY(firstView.isNull());
        QVERIFY(host.activate(active));

        QCOMPARE(activeCalls, 2);
        QCOMPARE(otherCalls, 1);
        QVERIFY(activeView != nullptr);
    }

    void recreatableCacheNeverExceedsCapacity()
    {
        int liveViews = 0;
        int factoryCalls[5]{};
        ZzPureTools::ZzPageHost host;
        QVERIFY(host.setRecreatableCapacity(3));

        for (int index = 0; index < 5; ++index) {
            const auto registration = zzCountingRegistration(
                QStringLiteral("page-%1").arg(index),
                ZzPureTools::ZzPageLifetimePolicy::Recreatable,
                &factoryCalls[index],
                &liveViews);
            QVERIFY(host.activate(registration));
            QCOMPARE(liveViews, std::min(index + 1, 4));
        }

        QCOMPARE(liveViews, 4);
        QVERIFY(host.setRecreatableCapacity(1));
        QCOMPARE(liveViews, 2);
        QVERIFY(host.setRecreatableCapacity(0));
        QCOMPARE(liveViews, 1);
        host.deactivateCurrent();
        QCOMPARE(liveViews, 0);
    }

    void failedFactoryLeavesNoHalfInitializedWidget()
    {
        ZzPureTools::ZzPageHost host;
        auto *stack = host.findChild<QStackedWidget *>();
        QVERIFY(stack != nullptr);
        const qsizetype childCount = stack->children().size();
        ZzPureTools::ZzPageRegistration failed;
        failed.routeId = ZzPureTools::ZzRouteId(QStringLiteral("failed"));
        failed.factory = [](QWidget *pageParent) {
            static_cast<void>(new QWidget(pageParent));
            return zzFactoryFailure();
        };

        auto result = host.activate(failed);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Backend);
        QCOMPARE(stack->children().size(), childCount);
        QVERIFY(!host.currentRoute().isValid());
    }

    void thrownFactoryBecomesUnknownAndPreservesCurrentPage()
    {
        int currentCalls = 0;
        int liveViews = 0;
        ZzPureTools::ZzPageHost host;
        const auto current = zzCountingRegistration(
            QStringLiteral("current"),
            ZzPureTools::ZzPageLifetimePolicy::Persistent,
            &currentCalls,
            &liveViews);
        QVERIFY(host.activate(current));
        auto *stack = host.findChild<QStackedWidget *>();
        QVERIFY(stack != nullptr);
        QWidget *const currentWidget = stack->currentWidget();
        const qsizetype childCount = stack->children().size();
        ZzPureTools::ZzPageRegistration throwing;
        throwing.routeId = ZzPureTools::ZzRouteId(QStringLiteral("throwing"));
        throwing.factory = [](QWidget *pageParent)
            -> ZzCore::ZzResult<std::unique_ptr<
                ZzPureTools::ZzPageInstance>> {
            static_cast<void>(new QWidget(pageParent));
            throw std::runtime_error("factory failure");
        };

        auto result = host.activate(throwing);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Unknown);
        QVERIFY(result.error().context().contains(
            QStringLiteral("factory failure")));
        QCOMPARE(host.currentRoute(), current.routeId);
        QCOMPARE(stack->currentWidget(), currentWidget);
        QCOMPARE(stack->children().size(), childCount);
        QCOMPARE(liveViews, 1);
    }

    void successfulNullFactoryBecomesInvalidStateAndPreservesCurrentPage()
    {
        int currentCalls = 0;
        int liveViews = 0;
        ZzPureTools::ZzPageHost host;
        const auto current = zzCountingRegistration(
            QStringLiteral("current"),
            ZzPureTools::ZzPageLifetimePolicy::Persistent,
            &currentCalls,
            &liveViews);
        QVERIFY(host.activate(current));
        auto *stack = host.findChild<QStackedWidget *>();
        QVERIFY(stack != nullptr);
        QWidget *const currentWidget = stack->currentWidget();
        const qsizetype childCount = stack->children().size();
        ZzPureTools::ZzPageRegistration nullPage;
        nullPage.routeId = ZzPureTools::ZzRouteId(QStringLiteral("null"));
        nullPage.factory = [](QWidget *pageParent) {
            static_cast<void>(new QWidget(pageParent));
            return ZzCore::ZzResult<std::unique_ptr<
                ZzPureTools::ZzPageInstance>>::success(nullptr);
        };

        auto result = host.activate(nullPage);

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(host.currentRoute(), current.routeId);
        QCOMPARE(stack->currentWidget(), currentWidget);
        QCOMPARE(stack->children().size(), childCount);
        QCOMPARE(liveViews, 1);
    }
};

QTEST_MAIN(ZzPageLifecycleTest)

#include "ZzPageLifecycleTest.moc"
