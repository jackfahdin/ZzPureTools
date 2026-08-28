#include <memory>
#include <utility>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>

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
        ZzPureTools::ZzNavigationNode node{
            ZzPureTools::ZzRouteId(QStringLiteral("settings")),
            QStringLiteral("ZzNavigationControllerTest"),
            QStringLiteral("Settings"),
            expectedIcon};
        node.sectionTranslationContext =
            QStringLiteral("ZzNavigationControllerTest");
        node.sectionSourceText = QStringLiteral("System");
        node.badgeText = QStringLiteral("7");
        QVERIFY(model.setNodes({node}));

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
        const QVariant iconValue = model.data(
            index,
            static_cast<int>(ZzPureTools::ZzNavigationRole::Icon));
        const auto icon =
            iconValue.value<ZzFluentUI::ZzIconDescriptor>();
        QCOMPARE(icon.resourceId, expectedIcon.resourceId);
        QCOMPARE(icon.mirroredInRightToLeft, true);
        QCOMPARE(
            model.data(
                index,
                static_cast<int>(ZzPureTools::ZzNavigationRole::Section)),
            QVariant(QStringLiteral("System")));
        QCOMPARE(
            model.data(
                     index,
                     static_cast<int>(
                         ZzPureTools::ZzNavigationRole::Placement))
                .value<ZzFluentUI::ZzNavigationPlacement>(),
            ZzFluentUI::ZzNavigationPlacement::Primary);
        QCOMPARE(
            model.data(
                index,
                static_cast<int>(ZzPureTools::ZzNavigationRole::Badge)),
            QVariant(QStringLiteral("7")));
        QCOMPARE(
            model.data(index, Qt::ToolTipRole),
            QVariant(QStringLiteral("Settings (7)")));
        QCOMPARE(
            model.data(index, Qt::AccessibleDescriptionRole),
            QVariant(QStringLiteral("7")));
        QCOMPARE(
            model.roleNames().value(Qt::DisplayRole),
            QByteArrayLiteral("display"));
        QCOMPARE(
            model.roleNames().value(static_cast<int>(
                ZzPureTools::ZzNavigationRole::Route)),
            QByteArrayLiteral("route"));
        QCOMPARE(
            model.roleNames().value(static_cast<int>(
                ZzPureTools::ZzNavigationRole::Icon)),
            QByteArrayLiteral("icon"));
        QCOMPARE(
            model.roleNames().value(static_cast<int>(
                ZzPureTools::ZzNavigationRole::Section)),
            QByteArrayLiteral("section"));
        QCOMPARE(
            model.roleNames().value(static_cast<int>(
                ZzPureTools::ZzNavigationRole::Placement)),
            QByteArrayLiteral("placement"));
        QCOMPARE(
            model.roleNames().value(static_cast<int>(
                ZzPureTools::ZzNavigationRole::Badge)),
            QByteArrayLiteral("badge"));
        QCOMPARE(
            static_cast<int>(ZzPureTools::ZzNavigationRole::Icon),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Icon));

        auto nodeResult = model.nodeAt(0);
        QVERIFY(nodeResult);
        QCOMPARE(
            nodeResult.value().titleSourceText,
            QStringLiteral("Settings"));
    }

    void modelRejectsInvalidPresentationAtomically()
    {
        ZzPureTools::ZzNavigationModel model;
        const auto stable =
            zzNode(QStringLiteral("stable"), QStringLiteral("Stable"));
        QVERIFY(model.setNodes({stable}));
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        const auto rejected = [&model](
                                  QList<ZzPureTools::ZzNavigationNode> nodes) {
            const auto result = model.setNodes(std::move(nodes));
            return !result
                && result.error().code()
                    == ZzCore::ZzErrorCode::InvalidArgument;
        };

        auto unpairedSection =
            zzNode(QStringLiteral("section"), QStringLiteral("Section"));
        unpairedSection.sectionTranslationContext =
            QStringLiteral("ZzNavigationControllerTest");
        QVERIFY(rejected({unpairedSection}));

        auto footerSection =
            zzNode(QStringLiteral("footer"), QStringLiteral("Footer"));
        footerSection.sectionTranslationContext =
            QStringLiteral("ZzNavigationControllerTest");
        footerSection.sectionSourceText = QStringLiteral("Footer section");
        footerSection.placement =
            ZzFluentUI::ZzNavigationPlacement::Footer;
        QVERIFY(rejected({footerSection}));

        auto invalidBadge =
            zzNode(QStringLiteral("badge"), QStringLiteral("Badge"));
        invalidBadge.badgeText = QStringLiteral(" bad");
        QVERIFY(rejected({invalidBadge}));
        invalidBadge.badgeText = QStringLiteral("bad\nline");
        QVERIFY(rejected({invalidBadge}));
        invalidBadge.badgeText = QStringLiteral("123456789");
        QVERIFY(rejected({invalidBadge}));

        auto invalidPlacement =
            zzNode(QStringLiteral("placement"), QStringLiteral("Placement"));
        using Placement = ZzFluentUI::ZzNavigationPlacement;
        // 故意模拟反序列化产生的越界枚举，验证公开模型的输入防线。
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        invalidPlacement.placement = static_cast<Placement>(0xff);
        QVERIFY(rejected({invalidPlacement}));

        QList<ZzPureTools::ZzNavigationNode> footerNodes;
        for (int index = 0; index < 7; ++index) {
            auto footer = zzNode(
                QStringLiteral("footer-%1").arg(index),
                QStringLiteral("Footer %1").arg(index));
            footer.placement = ZzFluentUI::ZzNavigationPlacement::Footer;
            footerNodes.append(std::move(footer));
        }
        QVERIFY(rejected(std::move(footerNodes)));

        QCOMPARE(resetSpy.size(), 0);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.nodeAt(0).value().routeId, stable.routeId);
    }

    void routeLookupAndBadgeUpdateAreLocal()
    {
        ZzPureTools::ZzNavigationModel model;
        QVERIFY(model.setNodes({
            zzNode(QStringLiteral("second"), QStringLiteral("Second")),
            zzNode(QStringLiteral("first"), QStringLiteral("First"))}));

        auto firstIndex = model.indexForRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("first")));
        QVERIFY(firstIndex);
        QCOMPARE(firstIndex.value().row(), 1);
        auto emptyRoute = model.indexForRoute({});
        QVERIFY(!emptyRoute);
        QCOMPARE(
            emptyRoute.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        auto missingRoute = model.indexForRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("missing")));
        QVERIFY(!missingRoute);
        QCOMPARE(
            missingRoute.error().code(), ZzCore::ZzErrorCode::NotFound);

        QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
        QVERIFY(model.setBadge(
            ZzPureTools::ZzRouteId(QStringLiteral("first")),
            QStringLiteral("12")));
        QCOMPARE(changedSpy.size(), 1);
        const QList<QVariant> arguments = changedSpy.takeFirst();
        QCOMPARE(arguments.at(0).value<QModelIndex>(), firstIndex.value());
        QCOMPARE(arguments.at(1).value<QModelIndex>(), firstIndex.value());
        const QList<int> roles = arguments.at(2).value<QList<int>>();
        QCOMPARE(
            roles,
            QList<int>({
                static_cast<int>(ZzPureTools::ZzNavigationRole::Badge),
                Qt::ToolTipRole,
                Qt::AccessibleDescriptionRole}));
        QCOMPARE(
            model.data(
                firstIndex.value(),
                static_cast<int>(ZzPureTools::ZzNavigationRole::Badge)),
            QVariant(QStringLiteral("12")));
        QCOMPARE(
            model.data(firstIndex.value(), Qt::ToolTipRole),
            QVariant(QStringLiteral("First (12)")));

        QVERIFY(model.setBadge(
            ZzPureTools::ZzRouteId(QStringLiteral("first")),
            QStringLiteral("12")));
        QCOMPARE(changedSpy.size(), 0);
        QVERIFY(!model.setBadge(
            ZzPureTools::ZzRouteId(QStringLiteral("first")),
            QStringLiteral("123456789")));
        QVERIFY(!model.setBadge(
            ZzPureTools::ZzRouteId(QStringLiteral("missing")),
            QStringLiteral("1")));
        QCOMPARE(changedSpy.size(), 0);

        QVERIFY(model.setNodes({
            zzNode(QStringLiteral("first"), QStringLiteral("First")),
            zzNode(QStringLiteral("second"), QStringLiteral("Second"))}));
        firstIndex = model.indexForRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("first")));
        QVERIFY(firstIndex);
        QCOMPARE(firstIndex.value().row(), 0);
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

    void backAndForwardMaintainIndependentBoundedStacks()
    {
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("A")),
            zzRegistration(QStringLiteral("B")),
            zzRegistration(QStringLiteral("C"))}));
        QSignalSpy historySpy(
            &controller,
            &ZzPureTools::ZzNavigationController::historyStateChanged);

        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("A"))));
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("B"))));
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("C"))));
        QVERIFY(controller.canGoBack());
        QVERIFY(!controller.canGoForward());

        QVERIFY(controller.goBack());
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("B")));
        QVERIFY(controller.canGoBack());
        QVERIFY(controller.canGoForward());
        QVERIFY(controller.goBack());
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("A")));
        QVERIFY(!controller.canGoBack());
        QVERIFY(controller.canGoForward());

        QVERIFY(controller.goForward());
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("B")));
        QVERIFY(controller.canGoBack());
        QVERIFY(controller.canGoForward());

        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("C"))));
        QVERIFY(controller.canGoBack());
        QVERIFY(!controller.canGoForward());
        QVERIFY(historySpy.size() >= 4);
    }

    void failedForwardKeepsCurrentRouteAndHistory()
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
        QVERIFY(controller.goBack());
        secondState.fail = true;

        auto failedForward = controller.goForward();

        QVERIFY(!failedForward);
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("A")));
        QVERIFY(controller.canGoForward());
        secondState.fail = false;
        QVERIFY(controller.goForward());
        QCOMPARE(
            controller.currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("B")));
        QVERIFY(!controller.canGoForward());
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

    void navigationDoesNotCreateTransitionAnimations()
    {
        ZzPureTools::ZzNavigationModel model;
        ZzPureTools::ZzPageHost host;
        ZzPureTools::ZzNavigationController controller(&model, &host);
        QVERIFY(controller.setRegistrations({
            zzRegistration(QStringLiteral("A")),
            zzRegistration(QStringLiteral("B"))}));

        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("A"))));
        QVERIFY(controller.navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("B"))));

        // 过渡动画因无任何视觉消费者而被移除；此处守卫其不被重新引入。
        QCOMPARE(
            controller.findChildren<QAbstractAnimation *>().size(),
            0);
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
        QVERIFY(!controller.canGoForward());
        auto result = controller.goBack();
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::NotFound);
        auto forwardResult = controller.goForward();
        QVERIFY(!forwardResult);
        QCOMPARE(
            forwardResult.error().code(), ZzCore::ZzErrorCode::NotFound);
    }
};

QTEST_MAIN(ZzNavigationControllerTest)

#include "ZzNavigationControllerTest.moc"
