#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtCore/QVariant>

#include "ZzExampleSystemPageKind.h"

namespace ZzCore {
class ZzSettingsStore;
}

namespace ZzFluentUI {
class ZzThemeController;
}

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleSystemPage;
class ZzExampleSystemPresenter;
class ZzExampleSystemViewModel;
class ZzExampleWindowShell;

/** @brief 实现系统快照采集、设置持久化和运行时服务调用。 */
class ZzExampleSystemPresenterPrivate final
{
public:
    /** @brief 保存由页面实例保证生命周期的注入端口。 */
    ZzExampleSystemPresenterPrivate(
        ZzExampleSystemPresenter *presenter,
        ZzExampleSystemPage *page,
        ZzExampleSystemViewModel *model,
        std::shared_ptr<ZzExampleApplicationContext> applicationContext,
        ZzPureTools::ZzPureApplication *pureApplication,
        ZzPureTools::ZzApplicationWindow *applicationWindow,
        ZzExampleWindowShell *windowShell);

    /** @brief 按页面种类填充快照或连接设置意图。 */
    void initialize(ZzExampleSystemPageKind kind);

    /** @brief 采集屏幕、窗口与 WindowKit 只读能力。 */
    void populatePlatformRows();

    /** @brief 填充版本、编译器、许可证和第三方信息。 */
    void populateAboutRows();

    /** @brief 读取并应用持久化设置后连接用户意图。 */
    void initializeSettings();

    /** @brief 应用并持久化主题模式。 */
    void applyThemeMode(int mode);

    /** @brief 应用并持久化日志等级。 */
    void applyLogLevel(int level);

    /** @brief 应用并持久化减少动效偏好。 */
    void applyReducedMotion(bool enabled);

    /** @brief 应用并持久化当前窗口 Dock 可见性。 */
    void applyActivityDockVisibility(bool visible);

    /** @brief 写入单项设置并向 View 报告预期错误。 */
    bool writeSetting(const QString &key, const QVariant &value);

    /** @brief 返回设置项或失败时的默认值。 */
    [[nodiscard]] QVariant readSetting(
        const QString &key,
        const QVariant &defaultValue) const;

    /** @brief 同时追加共享活动模型并写入 ZzLog。 */
    void recordActivity(const QString &text);

    ZzExampleSystemPresenter *q_ptr = nullptr;
    ZzExampleSystemPage *view = nullptr;
    ZzExampleSystemViewModel *viewModel = nullptr;
    std::shared_ptr<ZzExampleApplicationContext> context;
    ZzPureTools::ZzPureApplication *application = nullptr;
    ZzPureTools::ZzApplicationWindow *window = nullptr;
    ZzExampleWindowShell *shell = nullptr;
    ZzFluentUI::ZzThemeController *theme = nullptr;
    int currentLogLevel = 2;
};

} // namespace ZzExample
