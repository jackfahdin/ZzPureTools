#pragma once

#include <memory>

#include <QtCore/QString>

#include <ZzCore/ZzResult.h>

namespace ZzCore {
class ZzApplicationPaths;
class ZzSettingsStore;
class ZzTaskExecutor;
}

namespace ZzExample {

class ZzExampleActivityModel;
class ZzExampleApplicationContextPrivate;

/**
 * @brief 保存跨窗口共享且不依赖 QWidget 的应用服务与只读能力。
 *
 * 对象及其设置存储必须在创建线程访问；任务提交遵循 ZzTaskExecutor 的并发契约。
 */
class ZzExampleApplicationContext final
{
public:
    /**
     * @brief 创建应用目录、设置存储和独立任务执行器。
     * @return 可共享上下文，或目录准备及对象创建错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<std::shared_ptr<
        ZzExampleApplicationContext>> create();

    /** @brief 在创建线程释放设置存储、任务执行器和应用路径。 */
    ~ZzExampleApplicationContext();

    /** @brief 禁止复制独占的设置与任务服务。 */
    ZzExampleApplicationContext(const ZzExampleApplicationContext &) = delete;

    /** @brief 禁止复制赋值独占的设置与任务服务。 */
    ZzExampleApplicationContext &operator=(
        const ZzExampleApplicationContext &) = delete;

    /** @brief 禁止移动已绑定创建线程的应用服务。 */
    ZzExampleApplicationContext(ZzExampleApplicationContext &&) = delete;

    /** @brief 禁止移动赋值已绑定创建线程的应用服务。 */
    ZzExampleApplicationContext &operator=(
        ZzExampleApplicationContext &&) = delete;

    /** @brief 返回应用级设置接口，不向展示层暴露具体 Qt 后端。 */
    [[nodiscard]] ZzCore::ZzSettingsStore &settingsStore() noexcept;

    /** @brief 返回共享任务执行器。 */
    [[nodiscard]] ZzCore::ZzTaskExecutor &taskExecutor() noexcept;

    /** @brief 返回跨窗口共享的有界活动展示模型。 */
    [[nodiscard]] ZzExampleActivityModel &activityModel() noexcept;

    /** @brief 返回跨平台应用目录值。 */
    [[nodiscard]] const ZzCore::ZzApplicationPaths &paths() const noexcept;

    /** @brief 返回用于状态展示的稳定平台名称。 */
    [[nodiscard]] const QString &platformName() const noexcept;

private:
    explicit ZzExampleApplicationContext(
        const ZzCore::ZzApplicationPaths &paths);

    std::unique_ptr<ZzExampleApplicationContextPrivate> d_ptr;
};

} // namespace ZzExample
