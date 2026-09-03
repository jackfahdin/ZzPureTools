#pragma once

#include <memory>

#include <QtCore/QObject>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPureToolsExport.h>
#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>
#include <ZzPureTools/ZzWorkspaceWindowHandle.h>

namespace ZzPureTools {

class ZzApplicationWindow;
class ZzPureApplicationPrivate;
class ZzWorkspaceWindowCoordinatorPrivate;

/** @brief 保存同一应用中工作区窗口与 Shell 的登记快照。 */
class ZZ_PURE_TOOLS_EXPORT ZzWorkspaceWindowCoordinator final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzWorkspaceWindowCoordinator)

public:
    /** @brief 断开登记对象的观察连接并释放配置快照。 */
    ~ZzWorkspaceWindowCoordinator() override;

    /**
     * @brief 登记一个应用拥有的窗口、其工作区 Shell 和配置快照。
     * @param handle 同线程且互相匹配的窗口与 Shell 观察值。
     * @param configuration 在成功时按值保存的窗口配置。
     * @param primary 是否将该窗口登记为唯一主窗口。
     * @return 参数、线程、宿主或唯一性约束不满足时返回错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> registerWindow(
        const ZzWorkspaceWindowHandle &handle,
        const ZzWorkspaceWindowConfiguration &configuration,
        bool primary = false);

    /**
     * @brief 仅移除窗口的协调器登记状态，不销毁窗口或 Shell。
     * @param window 已登记窗口的非拥有观察指针。
     * @return 未登记窗口返回 NotFound。
     */
    [[nodiscard]] ZzCore::ZzResult<void> unregisterWindow(
        ZzApplicationWindow *window);

    /**
     * @brief 返回窗口登记时保存的配置值副本。
     * @param window 已登记窗口的非拥有观察指针。
     * @return 未登记窗口返回 NotFound。
     */
    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceWindowConfiguration>
    configuration(ZzApplicationWindow *window) const;

protected:
    /** @brief 保留已登记对象的销毁事件过滤安装点。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class ZzPureApplicationPrivate;

    /** @brief 仅允许应用私有状态创建每应用唯一协调器。 */
    explicit ZzWorkspaceWindowCoordinator(QObject *applicationObject);

    /** @brief 阻止后续登记并在窗口销毁前断开全部观察连接。 */
    void beginShutdown() noexcept;

    std::unique_ptr<ZzWorkspaceWindowCoordinatorPrivate> d_ptr;
};

} // namespace ZzPureTools
