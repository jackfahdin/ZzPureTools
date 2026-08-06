#include <memory>
#include <utility>

#include <QtCore/QMetaObject>
#include <QtCore/QStringList>
#include <QtTest/QTest>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

namespace {

/** @brief 记录应用关闭顺序并可预设启动失败的测试模块。 */
class ZzBuilderTestModule final : public ZzPureTools::ZzApplicationModule
{
public:
    ZzBuilderTestModule(
        QString id,
        QStringList *events,
        bool failStart = false)
        : descriptor_{
              ZzPureTools::ZzModuleId(std::move(id)),
              QStringLiteral("1.0.0"),
              {}}
        , events_(events)
        , failStart_(failStart)
    {
    }

    [[nodiscard]] ZzPureTools::ZzModuleDescriptor descriptor()
        const override
    {
        return descriptor_;
    }

    [[nodiscard]] ZzCore::ZzResult<void> start() override
    {
        if (events_ != nullptr) {
            events_->append(QStringLiteral("start"));
        }
        if (failStart_) {
            return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                ZzCore::ZzErrorCode::Backend,
                QStringLiteral("test module start failed")));
        }
        return ZzCore::ZzResult<void>::success();
    }

    void requestStop() noexcept override
    {
        if (events_ != nullptr) {
            events_->append(QStringLiteral("request"));
        }
    }

    void stop() noexcept override
    {
        if (events_ != nullptr) {
            events_->append(QStringLiteral("stop"));
        }
    }

private:
    ZzPureTools::ZzModuleDescriptor descriptor_;
    QStringList *events_;
    bool failStart_;
};

/** @brief 在销毁时记录页面位于模块 requestStop 和 stop 之间。 */
class ZzBuilderProbeWidget final : public QWidget
{
public:
    ZzBuilderProbeWidget(QWidget *parent, QStringList *events)
        : QWidget(parent)
        , events_(events)
    {
    }

    ~ZzBuilderProbeWidget() override
    {
        if (events_ != nullptr) {
            events_->append(QStringLiteral("view"));
        }
    }

private:
    QStringList *events_;
};

[[nodiscard]] ZzPureTools::ZzPureApplication &zzApplication()
{
    auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
    Q_ASSERT(application != nullptr);
    return *application;
}

[[nodiscard]] ZzPureTools::ZzNavigationNode zzNode(QString route)
{
    return {
        ZzPureTools::ZzRouteId(std::move(route)),
        QStringLiteral("ZzApplicationBuilderTest"),
        QStringLiteral("Test page"),
        {}};
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzPage(
    QString route,
    int *factoryCalls = nullptr,
    bool failFactory = false,
    QStringList *destructionEvents = nullptr)
{
    ZzPureTools::ZzPageRegistration page;
    page.routeId = ZzPureTools::ZzRouteId(std::move(route));
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    page.factory =
        [factoryCalls, failFactory, destructionEvents](QWidget *pageParent)
        -> ZzCore::ZzResult<std::unique_ptr<
            ZzPureTools::ZzPageInstance>> {
            if (factoryCalls != nullptr) {
                ++(*factoryCalls);
            }
            if (failFactory) {
                return ZzCore::ZzResult<std::unique_ptr<
                    ZzPureTools::ZzPageInstance>>::failure(ZzCore::ZzError(
                        ZzCore::ZzErrorCode::Backend,
                        QStringLiteral("test page factory failed")));
            }
            QWidget *view = destructionEvents == nullptr
                ? new QWidget(pageParent)
                : new ZzBuilderProbeWidget(pageParent, destructionEvents);
            return ZzPureTools::ZzPageInstance::create(
                pageParent,
                view,
                std::make_unique<QObject>(),
                std::make_unique<QObject>());
        };
    return page;
}

void zzConfigureSinglePage(
    ZzPureTools::ZzApplicationBuilder &builder,
    int *factoryCalls = nullptr,
    bool failFactory = false,
    QStringList *destructionEvents = nullptr)
{
    QVERIFY(builder.addPage(zzPage(
        QStringLiteral("home"),
        factoryCalls,
        failFactory,
        destructionEvents)));
    QVERIFY(builder.addNavigationNode(zzNode(QStringLiteral("home"))));
    QVERIFY(builder.setInitialRoute(
        ZzPureTools::ZzRouteId(QStringLiteral("home"))));
}

} // namespace

/** @brief 验证应用两阶段构建、冻结和关闭顺序。 */
class ZzApplicationBuilderTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void buildFreezesModulesAndPages()
    {
        auto &application = zzApplication();
        ZzPureTools::ZzApplicationBuilder builder;
        zzConfigureSinglePage(builder);
        QVERIFY(builder.build(application));
        QVERIFY(builder.isFrozen());

        auto pageResult = builder.addPage(zzPage(QStringLiteral("late")));
        auto moduleResult = builder.addModule(
            std::make_unique<ZzBuilderTestModule>(
                QStringLiteral("late"), nullptr));
        auto secondBuild = builder.build(application);

        QVERIFY(!pageResult);
        QVERIFY(!moduleResult);
        QVERIFY(!secondBuild);
        QCOMPARE(
            secondBuild.error().code(),
            ZzCore::ZzErrorCode::InvalidState);
        application.beginShutdown();
    }

    void buildRejectsDuplicateRoutesBeforeStartingModules()
    {
        auto &application = zzApplication();
        QStringList events;
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzBuilderTestModule>(
            QStringLiteral("module"), &events)));
        QVERIFY(builder.addPage(zzPage(QStringLiteral("home"))));
        QVERIFY(builder.addPage(zzPage(QStringLiteral("home"))));
        QVERIFY(builder.setInitialRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("home"))));

        auto result = builder.build(application);

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(events.isEmpty());
        QCOMPARE(application.windowCount(), 0);
        application.beginShutdown();
    }

    void buildRejectsNavigationNodeWithoutPage()
    {
        auto &application = zzApplication();
        QStringList events;
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzBuilderTestModule>(
            QStringLiteral("module"), &events)));
        QVERIFY(builder.addPage(zzPage(QStringLiteral("home"))));
        QVERIFY(builder.addNavigationNode(zzNode(QStringLiteral("missing"))));
        QVERIFY(builder.setInitialRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("home"))));

        auto result = builder.build(application);

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(events.isEmpty());
        application.beginShutdown();
    }

    void buildRejectsInvalidNavigationPresentationBeforeStartingModules()
    {
        auto &application = zzApplication();
        QStringList events;

        {
            ZzPureTools::ZzApplicationBuilder builder;
            QVERIFY(builder.addModule(
                std::make_unique<ZzBuilderTestModule>(
                    QStringLiteral("section-module"), &events)));
            QVERIFY(builder.addPage(zzPage(QStringLiteral("home"))));
            auto node = zzNode(QStringLiteral("home"));
            node.sectionTranslationContext =
                QStringLiteral("ZzApplicationBuilderTest");
            QVERIFY(builder.addNavigationNode(std::move(node)));
            QVERIFY(builder.setInitialRoute(
                ZzPureTools::ZzRouteId(QStringLiteral("home"))));
            const auto result = builder.build(application);
            QVERIFY(!result);
            QCOMPARE(
                result.error().code(),
                ZzCore::ZzErrorCode::InvalidArgument);
            QVERIFY(events.isEmpty());
        }

        {
            ZzPureTools::ZzApplicationBuilder builder;
            QVERIFY(builder.addModule(
                std::make_unique<ZzBuilderTestModule>(
                    QStringLiteral("badge-module"), &events)));
            QVERIFY(builder.addPage(zzPage(QStringLiteral("home"))));
            auto node = zzNode(QStringLiteral("home"));
            node.badgeText = QStringLiteral("bad\nline");
            QVERIFY(builder.addNavigationNode(std::move(node)));
            QVERIFY(builder.setInitialRoute(
                ZzPureTools::ZzRouteId(QStringLiteral("home"))));
            const auto result = builder.build(application);
            QVERIFY(!result);
            QCOMPARE(
                result.error().code(),
                ZzCore::ZzErrorCode::InvalidArgument);
            QVERIFY(events.isEmpty());
        }

        {
            ZzPureTools::ZzApplicationBuilder builder;
            QVERIFY(builder.addModule(
                std::make_unique<ZzBuilderTestModule>(
                    QStringLiteral("footer-module"), &events)));
            for (int index = 0; index < 7; ++index) {
                const QString route =
                    QStringLiteral("footer-%1").arg(index);
                QVERIFY(builder.addPage(zzPage(route)));
                auto node = zzNode(route);
                node.placement =
                    ZzFluentUI::ZzNavigationPlacement::Footer;
                QVERIFY(builder.addNavigationNode(std::move(node)));
            }
            QVERIFY(builder.setInitialRoute(
                ZzPureTools::ZzRouteId(QStringLiteral("footer-0"))));
            const auto result = builder.build(application);
            QVERIFY(!result);
            QCOMPARE(
                result.error().code(),
                ZzCore::ZzErrorCode::InvalidArgument);
            QVERIFY(events.isEmpty());
        }

        QCOMPARE(application.windowCount(), 0);
        application.beginShutdown();
    }

    void buildRequiresRegisteredInitialRoute()
    {
        auto &application = zzApplication();
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addPage(zzPage(QStringLiteral("home"))));
        QVERIFY(builder.setInitialRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("missing"))));

        auto result = builder.build(application);

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(application.windowCount(), 0);
        application.beginShutdown();
    }

    void moduleStartFailureDoesNotCreateWindow()
    {
        auto &application = zzApplication();
        QStringList events;
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzBuilderTestModule>(
            QStringLiteral("failing"), &events, true)));
        zzConfigureSinglePage(builder);

        auto result = builder.build(application);

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Backend);
        QCOMPARE(events, QStringList({QStringLiteral("start")}));
        QCOMPARE(application.windowCount(), 0);
        application.beginShutdown();
    }

    void successfulBuildCreatesOneWindowWithoutCreatingUnvisitedPages()
    {
        auto &application = zzApplication();
        int homeCalls = 0;
        int detailsCalls = 0;
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addPage(zzPage(
            QStringLiteral("home"), &homeCalls)));
        QVERIFY(builder.addPage(zzPage(
            QStringLiteral("details"), &detailsCalls)));
        QVERIFY(builder.addNavigationNode(zzNode(QStringLiteral("home"))));
        QVERIFY(builder.addNavigationNode(zzNode(QStringLiteral("details"))));
        QVERIFY(builder.setInitialRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("home"))));

        QVERIFY(builder.build(application));

        QCOMPARE(application.windowCount(), 1);
        QCOMPARE(homeCalls, 1);
        QCOMPARE(detailsCalls, 0);
        application.beginShutdown();
    }

    void windowFailureLeavesApplicationUnbuilt()
    {
        auto &application = zzApplication();
        QStringList events;
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzBuilderTestModule>(
            QStringLiteral("module"), &events)));
        zzConfigureSinglePage(builder, nullptr, true);

        auto result = builder.build(application);

        QVERIFY(!result);
        QCOMPARE(application.windowCount(), 0);
        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start"),
                QStringLiteral("request"),
                QStringLiteral("stop")}));
        QVERIFY(!application.createWindow());
        application.beginShutdown();
    }

    void failedBuildCanRetryWithFreshBuilder()
    {
        auto &application = zzApplication();
        ZzPureTools::ZzApplicationBuilder failing;
        QVERIFY(failing.addModule(std::make_unique<ZzBuilderTestModule>(
            QStringLiteral("failing"), nullptr, true)));
        zzConfigureSinglePage(failing);
        QVERIFY(!failing.build(application));

        ZzPureTools::ZzApplicationBuilder retry;
        zzConfigureSinglePage(retry);
        QVERIFY(retry.build(application));

        QCOMPARE(application.windowCount(), 1);
        application.beginShutdown();
    }

    void successfulApplicationRejectsSecondBuilder()
    {
        auto &application = zzApplication();
        ZzPureTools::ZzApplicationBuilder first;
        zzConfigureSinglePage(first);
        QVERIFY(first.build(application));

        ZzPureTools::ZzApplicationBuilder second;
        zzConfigureSinglePage(second);
        auto secondResult = second.build(application);
        QVERIFY(!secondResult);
        QCOMPARE(
            secondResult.error().code(),
            ZzCore::ZzErrorCode::InvalidState);

        application.beginShutdown();
        ZzPureTools::ZzApplicationBuilder afterShutdown;
        zzConfigureSinglePage(afterShutdown);
        auto afterResult = afterShutdown.build(application);
        QVERIFY(!afterResult);
        QCOMPARE(
            afterResult.error().code(),
            ZzCore::ZzErrorCode::InvalidState);
    }

    void applicationInstallsFluentStyleBeforeBuilding()
    {
        auto &application = zzApplication();
        QVERIFY(application.themeController() != nullptr);
        QVERIFY(qobject_cast<ZzFluentUI::ZzFluentStyle *>(
                    application.style())
                != nullptr);
        QCOMPARE(application.windowCount(), 0);
        application.beginShutdown();
    }

    void aboutToQuitDestroysWindowsBeforeStoppingModules()
    {
        auto &application = zzApplication();
        QStringList events;
        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzBuilderTestModule>(
            QStringLiteral("module"), &events)));
        zzConfigureSinglePage(builder, nullptr, false, &events);
        QVERIFY(builder.build(application));

        QVERIFY(QMetaObject::invokeMethod(
            &application,
            "aboutToQuit",
            Qt::DirectConnection));

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start"),
                QStringLiteral("request"),
                QStringLiteral("view"),
                QStringLiteral("stop")}));
        QCOMPARE(application.windowCount(), 0);
    }
};

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return 1;
    }
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzApplicationBuilderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzApplicationBuilderTest.moc"
