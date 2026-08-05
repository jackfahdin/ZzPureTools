#include <concepts>
#include <cstdlib>
#include <memory>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <ZzCore/ZzApplicationPaths.h>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzResult.h>
#include <ZzCore/ZzStopSource.h>
#include <ZzCore/ZzStopToken.h>
#include <ZzCore/ZzTaskExecutor.h>
#include <ZzCore/ZzTaskHandle.h>
#include <ZzCore/ZzTaskStatus.h>

/**
 * @brief 对 ZzCore 公开值类型和任务接口执行运行时冒烟验证。
 */
class ZzCorePublicHeadersTest final
{
public:
    /**
     * @brief 运行公开 API 冒烟验证。
     * @param argc 进程参数数量。
     * @param argv 进程参数数组。
     * @return 全部契约成立时返回 EXIT_SUCCESS，否则返回 EXIT_FAILURE。
     */
    [[nodiscard]] static int run(int argc, char *argv[])
    {
        QCoreApplication application(argc, argv);

        const ZzCore::ZzError originalError(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("invalid argument"),
            QStringLiteral("public header smoke"));
        // 此处有意复制，用于验证公开值类型的复制语义。
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        const auto copiedError = originalError;
        if (copiedError.code() != ZzCore::ZzErrorCode::InvalidArgument
            || copiedError.technicalMessage()
                != QStringLiteral("invalid argument")) {
            return EXIT_FAILURE;
        }

        if (!ZzCore::ZzResult<void>::success()) {
            return EXIT_FAILURE;
        }
        auto moveOnlyResult =
            ZzCore::ZzResult<std::unique_ptr<int>>::success(
                std::make_unique<int>(7));
        auto moveOnlyValue = std::move(moveOnlyResult).value();
        if (moveOnlyValue == nullptr || *moveOnlyValue != 7) {
            return EXIT_FAILURE;
        }

        const ZzCore::ZzApplicationPaths paths(
            QStringLiteral("ZzProject"),
            QStringLiteral("ZzCoreHeaderSmoke"));
        if (paths.configDirectory().isEmpty()
            || paths.dataDirectory().isEmpty()
            || paths.cacheDirectory().isEmpty()
            || paths.logDirectory().isEmpty()) {
            return EXIT_FAILURE;
        }

        ZzCore::ZzTaskExecutor executor(1);
        ZzCore::ZzStopSource stopSource;
        const ZzCore::ZzStopToken stopToken = stopSource.token();
        if (!stopToken.stopPossible() || stopToken.stopRequested()
            || !stopSource.requestStop() || stopSource.requestStop()
            || !stopToken.stopRequested()) {
            return EXIT_FAILURE;
        }

        auto handle = executor.submit<int>([](ZzCore::ZzStopToken) {
            return ZzCore::ZzResult<int>::success(9);
        });
        static_assert(std::same_as<
                      decltype(handle),
                      ZzCore::ZzTaskHandle<int>>);
        auto future = handle.future();
        future.waitForFinished();
        if (handle.status() != ZzCore::ZzTaskStatus::Finished
            || future.result().value() != 9) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
};

int main(int argc, char *argv[])
{
    return ZzCorePublicHeadersTest::run(argc, argv);
}
