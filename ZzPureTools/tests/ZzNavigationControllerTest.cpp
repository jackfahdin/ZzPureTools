#include <memory>
#include <utility>
#include <vector>

#include <QtCore/QParallelAnimationGroup>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzIconDescriptor.h>

#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzRouteId.h>

namespace {

/** @brief 保存页面 factory 的调用次数和可切换失败状态。 */
struct ZzPageFactoryState final
{
    int calls = 0;
    bool fail = false;
};

[[nodiscard]] ZzPureTools::ZzNavigationNode zzNode(
    QString route,
    QString title,
    QString icon = {})
{
    return {
        ZzPureTools::ZzRouteId(std::move(route)),
        QStringLiteral("ZzNavigationControllerTest"),
        std::move(title),
        {std::move(icon), false}};
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzRegistration(
    QString route,
    ZzPageFactoryState *state = nullptr)
{
    const QString routeValue = route;
    ZzPureTools::ZzPageRegistration registration;
    registration.routeId = ZzPureTools::ZzRouteId(std::move(route));
    registration.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    registration.factory =
        [state, routeValue](QWidget *pageParent)
        -> ZzCore::ZzResult<std::unique_ptr<
            ZzPureTools::ZzPageInstance>> {
            if (state != nullptr) {
                ++state->calls;
                if (state->fail) {
                    return ZzCore::ZzResult<std::unique_ptr<
                        ZzPureTools::ZzPageInstance>>::failure(
                            ZzCore::ZzError(
                                ZzCore::ZzErrorCode::Backend,
                                QStringLiteral("test page creation failed"),
                                QStringLiteral("routeId=%1")
                                    .arg(routeValue)));
                }
            }
            return ZzPureTools::ZzPageInstance::create(
                pageParent,
                new QWidget(pageParent),
                std::make_unique<QObject>(),
                std::make_unique<QObject>());
        };
    return registration;
}

} // namespace

/** @brief 验证强类型导航模型、窗口级历史和页面失败恢复。 */
class ZzNavigationControllerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void modelExposesDisplayAndRouteRoles()
    {
        ZzPureTools::ZzNavigationModel model;
        const ZzFluentUI::ZzIconDescriptor expectedIcon{
            QStringLiteral(":/icons/settings.svg"), true};
        QVERIFY(model.setNodes({
            {ZzPureTools::ZzRouteId(QStringLiteral("settings")),
             QStringLiteral("ZzNavigationControllerTest"),
             QStringLiteral("Settings"),
             expectedIcon}}));

        QCOMPARE(model.rowCount(), 1);
        const QModelIndex index = model.index(0, 0);
        QCOMPARE(
            model.data(index, Qt::DisplayRole).toString(),
            QStringLiteral("Settings"));
        QCOMPARE(
            model.data(
                     index,
                     static_cast<int>(
                         ZzPureTools::ZzNavigationRole::Route))
                .value<ZzPureTools::ZzRouteId>(),
            ZzPureTools::ZzRouteId(QStringLiteral("settings")));
        const auto icon = model.data(
                                   index,
                                   static_cast<int>(
                                       ZzPureTools::ZzNavigationRole::Icon))
                              .value<ZzFluentUI::ZzIconDescriptor>();
        QCOMPARE(icon.resourceId, expectedIcon.resourceId);
        QCOMPARE(icon.mirroredInRightToLeft, true);
        QCOMPARE(
            model.roleNames().value(Qt::DisplayRole),
            QByteArrayLiteral("display"));
        QCOMPARE(
            model.roleNames().value(static_cast<int>(
                ZzPureTools::ZzNavigationRole::Route)),
            QByteArrayLiteral("route"));

        auto nodeResult = model.nodeAt(0);
        QVERIFY(nodeResult);
        QCOMPARE(
            nodeResult.value().titleSourceText,
            QStringLiteral("Settings"));
    }

    void navigatesByRouteIdInsteadOfRow()
    {
        ZzPageFactoryState firstState;
        ZzPageFactoryState secondState;
        ZzPureTools::ZzNavigationModel model;
        QVERIFY(model.setNodes({
            zzNode(QStringLiteral("second"), QStringLiteral("Second")),
            zzNode(QStringLiteral("first"), QStringLiteral("First"))}));
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("first"), &firstState),
            zzRegistration(QStringLiteral("second"), &secondState)}));

        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("first"))));

        QCOMPARE(firstState.calls, 1);
        QCOMPARE(secondState.calls, 0);
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("first")));
    }

    void backUsesWindowLocalHistory()
    {
        ZzPureTools::ZzNavigationModel firstModel;
        ZzPureTools::ZzNavigationModel secondModel;
        ZzPureTools::ZzPageHost firstHost;
        ZzPureTools::ZzPageHost secondHost;
        ZzPureTools::ZzNavigationController first(
            &firstModel, &firstHost);
        ZzPureTools::ZzNavigationController second(
            &secondModel, &secondHost);
        const QList<ZzPureTools::ZzPageRegistration> registrations{
            zzRegistration(QStringLiteral("A")),
            zzRegistration(QStringLiteral("B")),
            zzRegistration(QStringLiteral("C")),
            zzRegistration(QStringLiteral("D"))};
        QVERIFY(first.setRegistrations(registrations));
        QVERIFY(second.setRegistrations(registrations));

        QVERIFY(first.navigate(ZzPureTools::ZzRouteId(QStringLiteral("A"))));
        QVERIFY(first.navigate(ZzPureTools::ZzRouteId(QStringLiteral("B"))));
        QVERIFY(second.navigate(ZzPureTools::ZzRouteId(QStringLiteral("C"))));
        QVERIFY(second.navigate(ZzPureTools::ZzRouteId(QStringLiteral("D"))));
        QVERIFY(first.goBack());

        QCOMPARE(
            first.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("A")));
        QCOMPARE(
            second.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("D")));
        QVERIFY(second.canGoBack());
    }

    void historyIsCappedAtOneHundredEntries()
    {
        constexpr int routeCount = 105;
        std::vector<ZzPageFactoryState> states;
        states.resize(routeCount);
        QList<ZzPureTools::ZzPageRegistration> registrations;
        registrations.reserve(routeCount);
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        for (int index = 0; index < routeCount; ++index) {
            registrations.append(zzRegistration(
                QStringLiteral("page-%1").arg(index),
                &states[static_cast<std::size_t>(index)]));
        }
        QVERIFY(controller.setRegistrations(std::move(registrations)));
        for (int index = 0; index < routeCount; ++index) {
            QVERIFY(controller.navigate(ZzPureTools::ZzRouteId(
                QStringLiteral("page-%1").arg(index))));
        }

        for (int index = 0; index < 100; ++index) {
            QVERIFY(controller.goBack());
        }

        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("page-4")));
        QVERIFY(!controller.canGoBack());
    }

    void duplicateRouteRegistrationFails()
    {
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);

        auto result = controller.setRegistrations({
            zzRegistration(QStringLiteral("duplicate")),
            zzRegistration(QStringLiteral("duplicate"))});

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
    }

    void failedPageCreationShowsFrameworkErrorPage()
    {
        ZzPageFactoryState goodState;
        ZzPageFactoryState failedState{0, true};
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("good"), &goodState),
            zzRegistration(QStringLiteral("failed"), &failedState)}));
        QSignalSpy failedSpy(
            &controller,
            &ZzPureTools::ZzNavigationController::navigationFailed);
        QSignalSpy routeSpy(
            &controller,
            &ZzPureTools::ZzNavigationController::currentRouteChanged);
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("good"))));

        auto result = controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("failed")));

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Backend);
        QCOMPARE(failedSpy.size(), 1);
        QCOMPARE(routeSpy.size(), 2);
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("failed")));
        const auto labels = host.findChildren<QLabel *>();
        QVERIFY(!labels.isEmpty());
        QVERIFY(!labels.constFirst()->text().contains(
            QStringLiteral("test page creation failed")));
    }

    void retrySameRouteAfterFailureAttemptsFactoryAgain()
    {
        ZzPageFactoryState failedState{0, true};
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("failed"), &failedState)}));

        QVERIFY(!controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("failed"))));
        QVERIFY(!controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("failed"))));

        QCOMPARE(failedState.calls, 2);
    }

    void returningFromFrameworkErrorDoesNotCreateSelfHistory()
    {
        ZzPageFactoryState goodState;
        ZzPageFactoryState failedState{0, true};
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("good"), &goodState),
            zzRegistration(QStringLiteral("failed"), &failedState)}));

        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("good"))));
        QVERIFY(!controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("failed"))));
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("good"))));

        QVERIFY(!controller.canGoBack());
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("good")));
    }

    void secondNavigationCancelsFirstTransition()
    {
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("A")),
            zzRegistration(QStringLiteral("B"))}));
        const auto groups =
            controller.findChildren<QParallelAnimationGroup *>();
        QCOMPARE(groups.size(), 1);
        auto *const transition = groups.constFirst();

        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("A"))));
        QCOMPARE(transition->state(), QAbstractAnimation::Running);
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("B"))));

        QCOMPARE(
            controller.findChildren<QParallelAnimationGroup *>().size(),
            1);
        QCOMPARE(
            controller.findChildren<QParallelAnimationGroup *>()
                .constFirst(),
            transition);
        QCOMPARE(transition->state(), QAbstractAnimation::Running);
    }

    void failedNavigationKeepsHistoryConsistent()
    {
        ZzPageFactoryState firstState;
        ZzPageFactoryState secondState;
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("A"), &firstState),
            zzRegistration(QStringLiteral("B"), &secondState)}));
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("A"))));
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("B"))));
        firstState.fail = true;

        auto failedBack = controller.goBack();

        QVERIFY(!failedBack);
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("B")));
        QVERIFY(controller.canGoBack());
        firstState.fail = false;
        QVERIFY(controller.goBack());
        QVERIFY(!controller.canGoBack());
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("A")));
    }

    void zeroHistoryCapacityDisablesBackNavigation()
    {
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("A")),
            zzRegistration(QStringLiteral("B"))}));
        QVERIFY(controller.setHistoryCapacity(0));

        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("A"))));
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("B"))));

        QVERIFY(!controller.canGoBack());
        auto result = controller.goBack();
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::NotFound);
    }
};

QTEST_MAIN(ZzNavigationControllerTest)

#include "ZzNavigationControllerTest.moc"
