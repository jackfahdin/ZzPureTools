#pragma once

#include <memory>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QMetaObject>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzApplicationRuntime.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzRouteId.h>

class QTranslator;

namespace ZzFluentUI {
class ZzThemeController;
}

namespace ZzPureTools {

class ZzApplicationWindow;
class ZzPureApplication;

/** @brief 保存应用级主题、已提交不可变配置、运行时和窗口所有权。 */
class ZzPureApplicationPrivate final
{
public:
    /** @brief 创建 parent=null 的唯一主题控制器。 */
    explicit ZzPureApplicationPrivate(ZzPureApplication *application);

    /** @brief 销毁尚未由公开析构清理的资源。 */
    ~ZzPureApplicationPrivate();

    /** @brief 使用已提交配置创建并接管一个新窗口。 */
    [[nodiscard]] ZzCore::ZzResult<ZzApplicationWindow *> createWindow();

    /** @brief 预留容量、连接关闭协议并接管局部窗口。 */
    [[nodiscard]] ZzCore::ZzResult<ZzApplicationWindow *> adoptWindow(
        std::unique_ptr<ZzApplicationWindow> window);

    /** @brief 为尚未提交的窗口建立同一队列关闭协议。 */
    [[nodiscard]] QMetaObject::Connection connectWindowCloseProtocol(
        ZzApplicationWindow *window);

    /** @brief 一次性提交所有已完成 staging 的资源。 */
    void commitBuild(
        std::unique_ptr<ZzApplicationRuntime> stagedRuntime,
        QList<ZzPageRegistration> stagedRegistrations,
        QList<ZzNavigationNode> stagedNodes,
        ZzRouteId stagedInitialRoute,
        std::vector<std::unique_ptr<QTranslator>> stagedTranslators,
        std::vector<std::unique_ptr<ZzApplicationWindow>> stagedWindows)
        noexcept;

    /** @brief 显示成功提交的首个窗口。 */
    void showInitialWindow();

    /** @brief 幂等执行应用级关闭顺序。 */
    void beginShutdown() noexcept;

    ZzPureApplication *const q_ptr;
    std::unique_ptr<ZzFluentUI::ZzThemeController> theme;
    std::unique_ptr<ZzApplicationRuntime> runtime;
    QList<ZzPageRegistration> registrations;
    QList<ZzNavigationNode> navigationNodes;
    ZzRouteId initialRoute;
    std::vector<std::unique_ptr<QTranslator>> translators;
    std::vector<std::unique_ptr<ZzApplicationWindow>> windows;
    QMetaObject::Connection aboutToQuitConnection;
    bool built = false;
    bool shuttingDown = false;
    bool hasEverBuilt = false;
};

} // namespace ZzPureTools
