#include <memory>
#include <stdexcept>
#include <utility>

#include <QtCore/QStringList>
#include <QtTest/QTest>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzApplicationRuntime.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleGraphBuilder.h>
#include <ZzPureTools/ZzModuleId.h>

namespace {

/** @brief 描述测试模块启动时采用的结果策略。 */
enum class ZzStartBehavior : unsigned char
{
    Success,
    Failure,
    ThrowException
};

/** @brief 记录运行时生命周期顺序并模拟启动失败的测试模块。 */
class ZzRecordingModule final : public ZzPureTools::ZzApplicationModule
{
public:
    ZzRecordingModule(
        QString id,
        QList<ZzPureTools::ZzModuleId> dependencies,
        QStringList *events,
        ZzStartBehavior startBehavior = ZzStartBehavior::Success)
        : descriptor_{
              ZzPureTools::ZzModuleId(std::move(id)),
              QStringLiteral("1.0.0"),
              std::move(dependencies)}
        , events_(events)
        , startBehavior_(startBehavior)
    {
    }

    [[nodiscard]] ZzPureTools::ZzModuleDescriptor descriptor()
        const override
    {
        return descriptor_;
    }

    [[nodiscard]] ZzCore::ZzResult<void> start() override
    {
        events_->append(QStringLiteral("start:%1")
                            .arg(descriptor_.id.value()));
        if (startBehavior_ == ZzStartBehavior::ThrowException) {
            throw std::runtime_error("start failure");
        }
        if (startBehavior_ == ZzStartBehavior::Failure) {
            return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
                ZzCore::ZzErrorCode::Backend,
                QStringLiteral("test module rejected start"),
                QStringLiteral("source=test")));
        }
        return ZzCore::ZzResult<void>::success();
    }

    void requestStop() noexcept override
    {
        events_->append(QStringLiteral("request:%1")
                            .arg(descriptor_.id.value()));
    }

    void stop() noexcept override
    {
        events_->append(QStringLiteral("stop:%1")
                            .arg(descriptor_.id.value()));
    }

private:
    ZzPureTools::ZzModuleDescriptor descriptor_;
    QStringList *events_;
    ZzStartBehavior startBehavior_;
};

} // namespace

/** @brief 验证应用运行时的启动、失败回滚和逆序停止契约。 */
class ZzApplicationRuntimeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rollsBackOnlyStartedModules()
    {
        QStringList events;
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("A"),
            QList<ZzPureTools::ZzModuleId>{},
            &events)));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("B"),
            QList<ZzPureTools::ZzModuleId>{
                ZzPureTools::ZzModuleId(QStringLiteral("A"))},
            &events,
            ZzStartBehavior::Failure)));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("C"),
            QList<ZzPureTools::ZzModuleId>{
                ZzPureTools::ZzModuleId(QStringLiteral("B"))},
            &events)));
        auto buildResult = builder.build();
        QVERIFY(buildResult);
        auto runtime = std::move(buildResult).value();

        auto startResult = runtime->start();

        QVERIFY(!startResult);
        QCOMPARE(
            startResult.error().code(),
            ZzCore::ZzErrorCode::Backend);
        QVERIFY(startResult.error().context().contains(
            QStringLiteral("moduleId=B")));
        QVERIFY(startResult.error().context().contains(
            QStringLiteral("source=test")));
        QVERIFY(!runtime->isRunning());
        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start:A"),
                QStringLiteral("start:B"),
                QStringLiteral("request:A"),
                QStringLiteral("stop:A")}));
    }

    void convertsThrownStartToErrorAndRollsBack()
    {
        QStringList events;
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("A"),
            QList<ZzPureTools::ZzModuleId>{},
            &events)));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("B"),
            QList<ZzPureTools::ZzModuleId>{
                ZzPureTools::ZzModuleId(QStringLiteral("A"))},
            &events,
            ZzStartBehavior::ThrowException)));
        auto buildResult = builder.build();
        QVERIFY(buildResult);
        auto runtime = std::move(buildResult).value();

        auto startResult = runtime->start();

        QVERIFY(!startResult);
        QCOMPARE(startResult.error().code(), ZzCore::ZzErrorCode::Unknown);
        QVERIFY(startResult.error().context().contains(
            QStringLiteral("moduleId=B")));
        QVERIFY(startResult.error().context().contains(
            QStringLiteral("start failure")));
        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start:A"),
                QStringLiteral("start:B"),
                QStringLiteral("request:A"),
                QStringLiteral("stop:A")}));
    }

    void requestsAndStopsInReverseOrder()
    {
        QStringList events;
        ZzPureTools::ZzModuleGraphBuilder builder;
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("A"),
            QList<ZzPureTools::ZzModuleId>{},
            &events)));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("B"),
            QList<ZzPureTools::ZzModuleId>{
                ZzPureTools::ZzModuleId(QStringLiteral("A"))},
            &events)));
        QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
            QStringLiteral("C"),
            QList<ZzPureTools::ZzModuleId>{
                ZzPureTools::ZzModuleId(QStringLiteral("B"))},
            &events)));
        auto buildResult = builder.build();
        QVERIFY(buildResult);
        auto runtime = std::move(buildResult).value();
        QCOMPARE(runtime->moduleCount(), 3);
        QVERIFY(runtime->start());
        QVERIFY(runtime->isRunning());

        runtime->requestStop();
        runtime->requestStop();
        QVERIFY(!runtime->isRunning());
        runtime->stop();
        runtime->stop();

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start:A"),
                QStringLiteral("start:B"),
                QStringLiteral("start:C"),
                QStringLiteral("request:C"),
                QStringLiteral("request:B"),
                QStringLiteral("request:A"),
                QStringLiteral("stop:C"),
                QStringLiteral("stop:B"),
                QStringLiteral("stop:A")}));
    }

    void destructorStopsRunningRuntime()
    {
        QStringList events;
        {
            ZzPureTools::ZzModuleGraphBuilder builder;
            QVERIFY(builder.addModule(std::make_unique<ZzRecordingModule>(
                QStringLiteral("owned"),
                QList<ZzPureTools::ZzModuleId>{},
                &events)));
            auto buildResult = builder.build();
            QVERIFY(buildResult);
            auto runtime = std::move(buildResult).value();
            QVERIFY(runtime->start());
        }

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start:owned"),
                QStringLiteral("request:owned"),
                QStringLiteral("stop:owned")}));
    }

    void moveAssignmentStopsCurrentRuntime()
    {
        QStringList events;
        ZzPureTools::ZzModuleGraphBuilder currentBuilder;
        QVERIFY(currentBuilder.addModule(
            std::make_unique<ZzRecordingModule>(
                QStringLiteral("current"),
                QList<ZzPureTools::ZzModuleId>{},
                &events)));
        auto currentResult = currentBuilder.build();
        QVERIFY(currentResult);
        auto current = std::move(currentResult).value();

        ZzPureTools::ZzModuleGraphBuilder incomingBuilder;
        QVERIFY(incomingBuilder.addModule(
            std::make_unique<ZzRecordingModule>(
                QStringLiteral("incoming"),
                QList<ZzPureTools::ZzModuleId>{},
                &events)));
        auto incomingResult = incomingBuilder.build();
        QVERIFY(incomingResult);
        auto incoming = std::move(incomingResult).value();

        QVERIFY(current->start());
        QVERIFY(incoming->start());
        *current = std::move(*incoming);
        current->requestStop();
        current->stop();

        QCOMPARE(
            events,
            QStringList({
                QStringLiteral("start:current"),
                QStringLiteral("start:incoming"),
                QStringLiteral("request:current"),
                QStringLiteral("stop:current"),
                QStringLiteral("request:incoming"),
                QStringLiteral("stop:incoming")}));
    }
};

QTEST_GUILESS_MAIN(ZzApplicationRuntimeTest)

#include "ZzApplicationRuntimeTest.moc"
