#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzApplicationRuntime.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleGraphBuilder.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzRouteId.h>

namespace {

/** @brief 记录生命周期事件并可模拟 descriptor 异常的测试模块。 */
class ZzRecordingModule final : public ZzPureTools::ZzApplicationModule
{
public:
    ZzRecordingModule(
        ZzPureTools::ZzModuleDescriptor descriptor,
        QStringList *events = nullptr,
        bool throwDescriptor = false)
        : descriptor_(std::move(descriptor))
        , events_(events)
        , throwDescriptor_(throwDescriptor)
    {
    }

    [[nodiscard]] ZzPureTools::ZzModuleDescriptor descriptor()
        const override
    {
        if (throwDescriptor_) {
            throw std::runtime_error("descriptor failure");
        }
        return descriptor_;
    }

    [[nodiscard]] ZzCore::ZzResult<void> start() override
    {
        if (events_ != nullptr) {
            events_->append(QStringLiteral("start:%1")
                                .arg(descriptor_.id.value()));
        }
        return ZzCore::ZzResult<void>::success();
    }

    void requestStop() noexcept override
    {
        if (events_ != nullptr) {
            events_->append(QStringLiteral("request:%1")
                                .arg(descriptor_.id.value()));
        }
    }

    void stop() noexcept override
    {
        if (events_ != nullptr) {
            events_->append(QStringLiteral("stop:%1")
                                .arg(descriptor_.id.value()));
        }
    }

private:
    ZzPureTools::ZzModuleDescriptor descriptor_;
    QStringList *events_;
    bool throwDescriptor_;
};

[[nodiscard]] ZzPureTools::ZzModuleDescriptor zzDescriptor(
    QString id,
    QList<ZzPureTools::ZzModuleId> dependencies = {})
{
    return {
        ZzPureTools::ZzModuleId(std::move(id)),
        QStringLiteral("1.0.0"),
        std::move(dependencies)};
}

} // namespace

/** @brief 验证强类型标识、模块图校验和稳定拓扑排序。 */
class ZzModuleGraphTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void idsAreOwningAndStronglyTyped()
    {
        QString source = QStringLiteral("  settings  ");
        const ZzPureTools::ZzModuleId module(source);
        const ZzPureTools::ZzRouteId route(source);
        source.clear();

        QCOMPARE(module.value(), QStringLiteral("settings"));
        QCOMPARE(route.value(), QStringLiteral("settings"));
        QVERIFY(module.isValid());
        QVERIFY(route.isValid());
        static_assert(!std::is_same_v<
            ZzPureTools::ZzModuleId,
            ZzPureTools::ZzRouteId>);
        static_assert(!std::is_convertible_v<
            ZzPureTools::ZzModuleId,
            ZzPureTools::ZzRouteId>);
        static_assert(std::is_abstract_v<
            ZzPureTools::ZzApplicationModule>);

        QHash<ZzPureTools::ZzModuleId, int> modules;
        modules.insert(module, 1);
        QCOMPARE(
            modules.value(ZzPureTools::ZzModuleId(
                QStringLiteral("settings"))),
            1);
    }

    void emptyIdsAreInvalid()
    {
        QVERIFY(!ZzPureTools::ZzModuleId().isValid());
        QVERIFY(!ZzPureTools::ZzModuleId(QStringLiteral(" \t ")).isValid());
        QVERIFY(!ZzPureTools::ZzRouteId(QStringLiteral("   ")).isValid());
    }

    void rejectsDuplicateIds()
    {
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(QStringLiteral("duplicate")))));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(QStringLiteral("duplicate")))));

        auto result = builder.build();

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(result.error().context().contains(
            QStringLiteral("moduleId=duplicate")));
        QVERIFY(builder.isFrozen());
    }

    void rejectsMissingDependency()
    {
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(
                QStringLiteral("consumer"),
                {ZzPureTools::ZzModuleId(QStringLiteral("missing"))}))));

        auto result = builder.build();

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(result.error().context().contains(
            QStringLiteral("moduleId=consumer")));
        QVERIFY(result.error().context().contains(
            QStringLiteral("dependencyId=missing")));
    }

    void rejectsCycle()
    {
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(
                QStringLiteral("A"),
                {ZzPureTools::ZzModuleId(QStringLiteral("B"))}))));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(
                QStringLiteral("B"),
                {ZzPureTools::ZzModuleId(QStringLiteral("A"))}))));

        auto result = builder.build();

        QVERIFY(!result);
        QCOMPARE(
            result.error().code(),
            ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(
            result.error().context(),
            QStringLiteral("moduleIds=A,B"));
    }

    void ordersIndependentModulesByRegistrationOrder()
    {
        QStringList events;
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(
                QStringLiteral("C"),
                {ZzPureTools::ZzModuleId(QStringLiteral("A"))}),
            &events)));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(QStringLiteral("B")), &events)));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(QStringLiteral("A")), &events)));

        auto buildResult = builder.build();
        QVERIFY(buildResult);
        auto runtime = std::move(buildResult).value();
        QCOMPARE(runtime->moduleCount(), 3);
        QVERIFY(runtime->start());

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start:B"),
                QStringLiteral("start:A"),
                QStringLiteral("start:C")}));
    }

    void convertsThrownDescriptorToErrorWithoutInventingModuleId()
    {
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(QStringLiteral("A")))));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            zzDescriptor(QStringLiteral("unavailable")), nullptr, true)));

        auto result = builder.build();

        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::Unknown);
        QVERIFY(result.error().context().contains(
            QStringLiteral("registrationIndex=1")));
        QVERIFY(result.error().context().contains(
            QString::fromLatin1(typeid(ZzRecordingModule).name())));
        QVERIFY(!result.error().context().contains(
            QStringLiteral("moduleId=")));
        QVERIFY(!result.error().context().contains(
            QStringLiteral("unavailable")));
    }
};

QTEST_GUILESS_MAIN(ZzModuleGraphTest)

#include "ZzModuleGraphTest.moc"
