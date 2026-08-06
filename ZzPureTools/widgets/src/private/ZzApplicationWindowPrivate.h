#pragma once

#include <memory>

#include <QtCore/QPointer>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzRouteId.h>

namespace ZzFluentUI {
class ZzFluentTitleBar;
class ZzNavigationPane;
class ZzThemeController;
}

namespace ZzWindowKit {
class ZzWindowAgent;
}

namespace ZzPureTools {

class ZzApplicationWindow;
class ZzNavigationController;
class ZzNavigationModel;
class ZzPageHost;

/** @brief 实现单个应用窗口的展示层与无边框适配装配。 */
class ZzApplicationWindowPrivate final
{
public:
    /** @brief 绑定尚未初始化的公开窗口。 */
    explicit ZzApplicationWindowPrivate(ZzApplicationWindow *window);

    /** @brief 先销毁观察 Qt 子树的独占对象。 */
    ~ZzApplicationWindowPrivate();

    /** @brief 按固定顺序完成窗口装配和首次导航。 */
    [[nodiscard]] ZzCore::ZzResult<void> initialize(
        const QList<ZzPageRegistration> &registrations,
        const QList<ZzNavigationNode> &nodes,
        const ZzRouteId &initialRoute,
        ZzFluentUI::ZzThemeController *themeController);

    /** @brief 刷新窗口级可翻译静态文本和导航标题缓存。 */
    void refreshTranslations();

    /** @brief 同步最大化或还原按钮的视觉状态。 */
    void syncWindowState();

    /** @brief 按控制器当前强类型路由同步导航面板唯一选择。 */
    void syncNavigationSelection();

    ZzApplicationWindow *const q_ptr;
    std::unique_ptr<ZzWindowKit::ZzWindowAgent> agent;
    std::unique_ptr<ZzNavigationModel> model;
    std::unique_ptr<ZzNavigationController> controller;
    ZzFluentUI::ZzFluentTitleBar *titleBar = nullptr;
    ZzFluentUI::ZzNavigationPane *navigationPane = nullptr;
    ZzPageHost *host = nullptr;
    QPointer<ZzFluentUI::ZzThemeController> theme;
    bool initialized = false;
    bool acceptedClosePending = false;
};

} // namespace ZzPureTools
