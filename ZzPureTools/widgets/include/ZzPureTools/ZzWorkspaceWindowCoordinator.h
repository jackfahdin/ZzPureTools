#pragma once

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPureToolsExport.h>
#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>
#include <ZzFluentUI/ZzWorkspacePageId.h>
#include <ZzPureTools/ZzWorkspaceWindowHandle.h>
#include <ZzPureTools/ZzWorkspaceWindowCreateOptions.h>
#include <ZzPureTools/ZzWorkspaceWindowFactory.h>

class QWidget;

namespace ZzFluentUI {
class ZzSplitWorkspace;
class ZzTabGroupId;
}

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

    /** @brief 设置创建后续工作区窗口的工厂；空工厂会禁止创建。 */
    void setWindowFactory(ZzWorkspaceWindowFactory factory);

    /** @brief 将补丁合并到已登记窗口配置并同步其窗口和 Shell 表面。 */
    [[nodiscard]] ZzCore::ZzResult<void> applyConfiguration(
        ZzApplicationWindow *window,
        const ZzWorkspaceWindowConfigurationPatch &patch);

    /** @brief 通过窗口工厂创建、登记并按来源配置初始化一个独立窗口。 */
    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceWindowHandle> createWindow(
        const ZzWorkspaceWindowCreateOptions &options = {});

    /** @brief 事务创建暂存窗口、迁移来源标签，成功后才显示新窗口。 */
    [[nodiscard]] ZzCore::ZzResult<void> tearOff(
        ZzFluentUI::ZzSplitWorkspace *sourceWorkspace,
        const ZzFluentUI::ZzTabGroupId &sourceGroup,
        int sourceIndex,
        const ZzWorkspaceWindowCreateOptions &options = {});

    /** @brief 按已登记窗口的关闭策略回收页面并请求关闭窗口。 */
    [[nodiscard]] ZzCore::ZzResult<void> closeWindow(
        ZzApplicationWindow *window);

    /** @brief 仅消费一次当前 Delegate 策略产生的待批准关闭请求。 */
    [[nodiscard]] ZzCore::ZzResult<void> approveDelegatedClose(
        ZzApplicationWindow *window);

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

    /**
     * @brief 按登记顺序保存全部窗口、工作区页面和来源栈拓扑。
     * @return 成功时返回带 SHA-256 校验的 `ZZWT` 字节；页面观察不完整时返回错误。
     */
    [[nodiscard]] ZzCore::ZzResult<QByteArray> saveTopology() const;

    /**
     * @brief 在空业务拓扑中事务恢复窗口、页面和跨屏几何。
     * @param state `ZZWT` v2 字节，或兼容的 `ZZSW` v1 单窗口字节。
     * @param pageResolver 按持久布局键创建无父对象页面的解析器。
     * @return 全部窗口和页面提交后成功；任一步失败时保持原拓扑不变。
     */
    [[nodiscard]] ZzCore::ZzResult<void> restoreTopology(
        const QByteArray &state,
        const ZzWorkspacePageResolver &pageResolver);

protected:
    /** @brief 保留已登记对象的销毁事件过滤安装点。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    /** @brief 新工作区窗口完成登记和初始配置提交后发出。 */
    void windowCreated(ZzApplicationWindow *window);

    /** @brief 已登记窗口的独立配置完整提交后发出。 */
    void windowConfigurationChanged(
        ZzApplicationWindow *window,
        const ZzWorkspaceWindowConfiguration &configuration);

    /** @brief 页面已锁定且窗口即将回收并关闭时发出；接收期间禁止公开迁移。 */
    void windowAboutToClose(
        ZzApplicationWindow *window,
        const QList<QWidget *> &pages);

    /** @brief Delegate 策略首次进入待批准关闭状态时发出。 */
    void windowCloseApprovalRequested(ZzApplicationWindow *window);

    /** @brief 找不到任何回收目标时按稳定顺序报告保留的页面。 */
    void orphanedPages(const QList<QWidget *> &pages);

    /** @brief 转发登记工作区活动页面并附加窗口身份。 */
    void activePageChanged(
        ZzApplicationWindow *window, QWidget *page,
        const ZzFluentUI::ZzWorkspacePageId &id);

    /** @brief 转发登记工作区页面修改和注意状态并附加窗口身份。 */
    void pageActivityChanged(
        ZzApplicationWindow *window, QWidget *page,
        const ZzFluentUI::ZzWorkspacePageId &id,
        bool modified, bool attention);

private:
    friend class ZzPureApplicationPrivate;

    /** @brief 仅允许应用私有状态创建每应用唯一协调器。 */
    explicit ZzWorkspaceWindowCoordinator(QObject *applicationObject);

    /** @brief 阻止后续登记并在窗口销毁前断开全部观察连接。 */
    void beginShutdown() noexcept;

    std::unique_ptr<ZzWorkspaceWindowCoordinatorPrivate> d_ptr;
};

} // namespace ZzPureTools
