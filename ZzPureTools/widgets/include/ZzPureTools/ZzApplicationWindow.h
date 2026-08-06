#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtWidgets/QMainWindow>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureToolsExport.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWindowSetupCallback.h>

class QCloseEvent;
class QEvent;

namespace ZzFluentUI {
class ZzFluentTitleBar;
class ZzNavigationPane;
class ZzThemeController;
}

namespace ZzWindowKit {
class ZzWindowAgent;
}

namespace ZzPureTools {

class ZzApplicationBuilderPrivate;
class ZzApplicationWindowPrivate;
class ZzNavigationController;
class ZzNavigationModel;
class ZzPageHost;
class ZzPureApplicationPrivate;

/**
 * @brief 组合无边框窗口、Fluent 标题栏、导航模型和页面宿主。
 *
 * 顶层窗口只能由 ZzPureApplication 的 unique_ptr 拥有，始终关闭
 * Qt::WA_DeleteOnClose。全部观察 getter 只允许在窗口所属 GUI 线程调用。
 */
class ZZ_PURE_TOOLS_EXPORT ZzApplicationWindow final : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzApplicationWindow)

public:
    /** @brief 按展示层、WindowKit、Model 和 Controller 的安全顺序销毁窗口。 */
    ~ZzApplicationWindow() override;

    /** @brief 返回当前窗口独占导航控制器的非拥有观察指针。 */
    [[nodiscard]] ZzNavigationController *navigationController()
        const noexcept;

    /** @brief 返回当前窗口独占导航模型的非拥有观察指针。 */
    [[nodiscard]] ZzNavigationModel *navigationModel() const noexcept;

    /** @brief 返回当前窗口 Qt 子树拥有的页面宿主观察指针。 */
    [[nodiscard]] ZzPageHost *pageHost() const noexcept;

    /** @brief 返回当前窗口 Qt 子树拥有的 Fluent 标题栏观察指针。 */
    [[nodiscard]] ZzFluentUI::ZzFluentTitleBar *titleBar()
        const noexcept;

    /** @brief 返回当前窗口 Qt 子树拥有的 Fluent 导航面板观察指针。 */
    [[nodiscard]] ZzFluentUI::ZzNavigationPane *navigationPane()
        const noexcept;

    /** @brief 返回当前窗口独占 WindowKit 代理的非拥有观察指针。 */
    [[nodiscard]] ZzWindowKit::ZzWindowAgent *windowAgent()
        const noexcept;

protected:
    /** @brief 刷新语言和窗口最大化状态，其他事件交给 QMainWindow。 */
    void changeEvent(QEvent *event) override;

    /** @brief 仅在基类最终接受关闭后发出一次队列删除意图。 */
    void closeEvent(QCloseEvent *event) override;

private:
    /** @brief 通知应用在 closeEvent 返回后尝试消费真实关闭令牌。 */
    Q_SIGNAL void closeAccepted();

    friend class ZzApplicationBuilderPrivate;
    friend class ZzPureApplicationPrivate;

    /** @brief 创建尚未装配、禁止自动删除的顶层窗口。 */
    ZzApplicationWindow();

    /**
     * @brief 创建并完整初始化一个不可见顶层窗口。
     * @param registrations 窗口共享配置的页面注册值。
     * @param nodes 窗口独立模型使用的导航节点值。
     * @param initialRoute 首次创建的页面路由。
     * @param themeController 非空应用级主题控制器观察值。
     * @return 完整窗口所有权，或任一装配错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<
        std::unique_ptr<ZzApplicationWindow>> create(
        const QList<ZzPageRegistration> &registrations,
        const QList<ZzNavigationNode> &nodes,
        const ZzRouteId &initialRoute,
        ZzFluentUI::ZzThemeController *themeController,
        const ZzWindowSetupCallback &windowSetupCallback);

    /** @brief 按固定顺序装配当前不可见窗口。 */
    [[nodiscard]] ZzCore::ZzResult<void> initialize(
        const QList<ZzPageRegistration> &registrations,
        const QList<ZzNavigationNode> &nodes,
        const ZzRouteId &initialRoute,
        ZzFluentUI::ZzThemeController *themeController,
        const ZzWindowSetupCallback &windowSetupCallback);

    /** @brief 原子式消费一次由真实 accepted closeEvent 设置的令牌。 */
    [[nodiscard]] bool consumeAcceptedClose() noexcept;

    std::unique_ptr<ZzApplicationWindowPrivate> d_ptr;
};

} // namespace ZzPureTools
