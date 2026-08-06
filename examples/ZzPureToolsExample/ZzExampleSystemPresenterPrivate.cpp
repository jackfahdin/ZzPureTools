#include "ZzExampleSystemPresenterPrivate.h"

#include <array>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QSysInfo>
#include <QtCore/QStringList>
#include <QtCore/QStringView>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <ZzCore/ZzApplicationPaths.h>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzSettingsStore.h>

#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

#include <ZzLog/ZzLog.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzPureApplication.h>

#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowCapability.h>

#include "ZzExampleApplicationContext.h"
#include "ZzExampleSystemPage.h"
#include "ZzExampleSystemPresenter.h"
#include "ZzExampleSystemViewModel.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

constexpr int zzThemeModeCount = 4;
constexpr int zzLogLevelCount = 7;

/** @brief 返回当前编译器的稳定用户可读名称。 */
[[nodiscard]] QString zzCompilerName()
{
#if defined(__clang__)
    return QStringLiteral("Clang %1.%2.%3")
        .arg(__clang_major__)
        .arg(__clang_minor__)
        .arg(__clang_patchlevel__);
#elif defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_VER);
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2.%3")
        .arg(__GNUC__)
        .arg(__GNUC_MINOR__)
        .arg(__GNUC_PATCHLEVEL__);
#else
    return QStringLiteral("Unknown");
#endif
}

/** @brief 返回兼容 MSVC 与标准 __cplusplus 的语言版本名称。 */
[[nodiscard]] QString zzCppStandardName()
{
#if defined(_MSVC_LANG)
    constexpr long version = _MSVC_LANG;
#else
    constexpr long version = __cplusplus;
#endif
    return QStringLiteral("C++%1").arg(version / 100 % 100);
}

/** @brief 将 WindowAgent 生命周期状态转换为展示文本。 */
[[nodiscard]] QString zzAgentStateName(
    ZzWindowKit::ZzWindowAgentState state)
{
    switch (state) {
    case ZzWindowKit::ZzWindowAgentState::Detached:
        return QStringLiteral("Detached");
    case ZzWindowKit::ZzWindowAgentState::Attached:
        return QStringLiteral("Attached");
    case ZzWindowKit::ZzWindowAgentState::Configured:
        return QStringLiteral("Configured");
    case ZzWindowKit::ZzWindowAgentState::Invalidated:
        return QStringLiteral("Invalidated");
    case ZzWindowKit::ZzWindowAgentState::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

/** @brief 将 WindowKit 能力位转换为稳定逗号分隔文本。 */
[[nodiscard]] QString zzCapabilityNames(
    ZzWindowKit::ZzWindowCapabilities capabilities)
{
    using ZzCapability = ZzWindowKit::ZzWindowCapability;
    const std::array<std::pair<ZzCapability, const char *>, 7> values{{
        {ZzCapability::SystemMenu, "SystemMenu"},
        {ZzCapability::Blur, "Blur"},
        {ZzCapability::Acrylic, "Acrylic"},
        {ZzCapability::Mica, "Mica"},
        {ZzCapability::MicaAlt, "MicaAlt"},
        {ZzCapability::NativeSystemButtons, "NativeSystemButtons"},
        {ZzCapability::SnapLayout, "SnapLayout"},
    }};
    QStringList names;
    for (const auto &[capability, name] : values) {
        if (capabilities.testFlag(capability)) {
            names.append(QString::fromLatin1(name));
        }
    }
    return names.isEmpty()
        ? QStringLiteral("None")
        : names.join(QStringLiteral(", "));
}

/** @brief 返回当前窗口状态的稳定展示文本。 */
[[nodiscard]] QString zzWindowStateName(const QWidget &window)
{
    if (window.isFullScreen()) {
        return QStringLiteral("FullScreen");
    }
    if (window.isMaximized()) {
        return QStringLiteral("Maximized");
    }
    if (window.isMinimized()) {
        return QStringLiteral("Minimized");
    }
    return QStringLiteral("Normal");
}

/** @brief 将设置值收敛为指定半开区间内的整数。 */
[[nodiscard]] int zzBoundedSetting(
    const QVariant &value,
    int fallback,
    int upperBound)
{
    bool ok = false;
    const int converted = value.toInt(&ok);
    return ok && converted >= 0 && converted < upperBound
        ? converted
        : fallback;
}

} // namespace

ZzExampleSystemPresenterPrivate::ZzExampleSystemPresenterPrivate(
    ZzExampleSystemPresenter *presenter,
    ZzExampleSystemPage *page,
    ZzExampleSystemViewModel *model,
    std::shared_ptr<ZzExampleApplicationContext> applicationContext,
    ZzPureTools::ZzPureApplication *pureApplication,
    ZzPureTools::ZzApplicationWindow *applicationWindow,
    ZzExampleWindowShell *windowShell)
    : q_ptr(presenter)
    , view(page)
    , viewModel(model)
    , context(std::move(applicationContext))
    , application(pureApplication)
    , window(applicationWindow)
    , shell(windowShell)
{
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(view != nullptr);
    Q_ASSERT(viewModel != nullptr);
    Q_ASSERT(context != nullptr);
    Q_ASSERT(application != nullptr);
    Q_ASSERT(window != nullptr);
    Q_ASSERT(shell != nullptr);
    theme = application->themeController();
    Q_ASSERT(theme != nullptr);
}

void ZzExampleSystemPresenterPrivate::initialize(
    ZzExampleSystemPageKind kind)
{
    switch (kind) {
    case ZzExampleSystemPageKind::Platform:
        populatePlatformRows();
        break;
    case ZzExampleSystemPageKind::Settings:
        initializeSettings();
        break;
    case ZzExampleSystemPageKind::About:
        populateAboutRows();
        break;
    }
}

void ZzExampleSystemPresenterPrivate::populatePlatformRows()
{
    QList<QPair<QString, QString>> rows{
        {QStringLiteral("目标平台"), context->platformName()},
        {QStringLiteral("Qt 平台插件"), QGuiApplication::platformName()},
        {QStringLiteral("操作系统"), QSysInfo::prettyProductName()},
        {QStringLiteral("CPU 架构"), QSysInfo::currentCpuArchitecture()},
        {QStringLiteral("构建 ABI"), QSysInfo::buildAbi()},
        {QStringLiteral("窗口状态"), zzWindowStateName(*window)},
        {QStringLiteral("窗口尺寸"),
         QStringLiteral("%1 x %2").arg(window->width()).arg(window->height())},
    };
    if (const QScreen *screen = window->screen(); screen != nullptr) {
        const QRect geometry = screen->availableGeometry();
        rows.append({QStringLiteral("屏幕"), screen->name()});
        rows.append({
            QStringLiteral("可用区域"),
            QStringLiteral("%1 x %2").arg(geometry.width()).arg(geometry.height())});
        rows.append({
            QStringLiteral("逻辑 DPI"),
            QString::number(screen->logicalDotsPerInch(), 'f', 1)});
        rows.append({
            QStringLiteral("设备像素比"),
            QString::number(screen->devicePixelRatio(), 'f', 2)});
    }
    if (auto *agent = window->windowAgent(); agent != nullptr) {
        rows.append({
            QStringLiteral("WindowAgent 状态"),
            zzAgentStateName(agent->state())});
        rows.append({
            QStringLiteral("WindowKit 能力"),
            zzCapabilityNames(agent->capabilities())});
    }
    viewModel->setRows(rows);
    view->setStatusText(QStringLiteral("平台能力为当前窗口的只读快照"));
}

void ZzExampleSystemPresenterPrivate::populateAboutRows()
{
    viewModel->setRows({
        {QStringLiteral("产品"), QStringLiteral("ZzPureTools")},
        {QStringLiteral("作者"), QStringLiteral("Jackfahdin")},
        {QStringLiteral("版本"), QCoreApplication::applicationVersion()},
        {QStringLiteral("Qt"), QString::fromLatin1(qVersion())},
        {QStringLiteral("C++ 标准"), zzCppStandardName()},
        {QStringLiteral("编译器"), zzCompilerName()},
        {QStringLiteral("许可证"), QStringLiteral("MIT")},
        {QStringLiteral("窗口后端"), QStringLiteral("QWindowKit")},
        {QStringLiteral("日志后端"), QStringLiteral("spdlog + fmt")},
        {QStringLiteral("支持平台"),
         QStringLiteral("Linux / Windows / macOS")},
    });
    view->setStatusText(
        QStringLiteral("第三方组件保留各自许可证与来源记录"));
}

void ZzExampleSystemPresenterPrivate::initializeSettings()
{
    const int currentTheme = static_cast<int>(theme->mode());
    const int themeMode = zzBoundedSetting(
        readSetting(
            QStringLiteral("appearance/themeMode"), currentTheme),
        currentTheme,
        zzThemeModeCount);
    currentLogLevel = zzBoundedSetting(
        readSetting(QStringLiteral("logging/level"), currentLogLevel),
        currentLogLevel,
        zzLogLevelCount);
    const bool reducedMotion = readSetting(
        QStringLiteral("appearance/reducedMotion"),
        theme->reducedMotion()).toBool();
    const bool dockVisible = readSetting(
        QStringLiteral("window/activityDockVisible"),
        shell->isActivityDockVisible()).toBool();

    theme->setMode(static_cast<ZzFluentUI::ZzThemeMode>(themeMode));
    theme->setReducedMotion(reducedMotion);
    static_cast<void>(ZzLog::setConsoleLevel(
        static_cast<ZzLog::ZzLogLevel>(currentLogLevel)));
    static_cast<void>(ZzLog::setFileLevel(
        static_cast<ZzLog::ZzLogLevel>(currentLogLevel)));
    shell->setActivityDockVisible(dockVisible);
    view->setSettingsSnapshot(
        themeMode, currentLogLevel, reducedMotion, dockVisible);
    viewModel->setRows({
        {QStringLiteral("配置目录"), context->paths().configDirectory()},
        {QStringLiteral("日志目录"), context->paths().logDirectory()},
        {QStringLiteral("设置后端"), QStringLiteral("INI")},
    });
    view->setStatusText(QStringLiteral("设置已加载"));

    QObject::connect(
        view,
        &ZzExampleSystemPage::themeModeRequested,
        q_ptr,
        [this](int mode) { applyThemeMode(mode); });
    QObject::connect(
        view,
        &ZzExampleSystemPage::logLevelRequested,
        q_ptr,
        [this](int level) { applyLogLevel(level); });
    QObject::connect(
        view,
        &ZzExampleSystemPage::reducedMotionRequested,
        q_ptr,
        [this](bool enabled) { applyReducedMotion(enabled); });
    QObject::connect(
        view,
        &ZzExampleSystemPage::activityDockVisibilityRequested,
        q_ptr,
        [this](bool visible) { applyActivityDockVisibility(visible); });
    QObject::connect(
        theme,
        &ZzFluentUI::ZzThemeController::snapshotChanged,
        q_ptr,
        [this] {
            view->setSettingsSnapshot(
                static_cast<int>(theme->mode()),
                currentLogLevel,
                theme->reducedMotion(),
                shell->isActivityDockVisible());
        });
    QObject::connect(
        shell,
        &ZzExampleWindowShell::activityDockVisibilityChanged,
        q_ptr,
        [this](bool visible) {
            view->setSettingsSnapshot(
                static_cast<int>(theme->mode()),
                currentLogLevel,
                theme->reducedMotion(),
                visible);
        });
}

void ZzExampleSystemPresenterPrivate::applyThemeMode(int mode)
{
    if (mode < 0 || mode >= zzThemeModeCount) {
        view->setStatusText(QStringLiteral("主题设置无效"));
        return;
    }
    theme->setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));
    if (writeSetting(QStringLiteral("appearance/themeMode"), mode)) {
        view->setStatusText(QStringLiteral("主题设置已保存"));
    }
}

void ZzExampleSystemPresenterPrivate::applyLogLevel(int level)
{
    if (level < 0 || level >= zzLogLevelCount) {
        view->setStatusText(QStringLiteral("日志等级无效"));
        return;
    }
    const auto logLevel = static_cast<ZzLog::ZzLogLevel>(level);
    if (!ZzLog::setConsoleLevel(logLevel)
        || !ZzLog::setFileLevel(logLevel)) {
        view->setStatusText(QStringLiteral("日志等级应用失败"));
        return;
    }
    currentLogLevel = level;
    if (writeSetting(QStringLiteral("logging/level"), level)) {
        view->setStatusText(QStringLiteral("日志等级已保存"));
    }
}

void ZzExampleSystemPresenterPrivate::applyReducedMotion(bool enabled)
{
    theme->setReducedMotion(enabled);
    if (writeSetting(
            QStringLiteral("appearance/reducedMotion"), enabled)) {
        view->setStatusText(QStringLiteral("动效偏好已保存"));
    }
}

void ZzExampleSystemPresenterPrivate::applyActivityDockVisibility(
    bool visible)
{
    shell->setActivityDockVisible(visible);
    if (writeSetting(
            QStringLiteral("window/activityDockVisible"), visible)) {
        view->setStatusText(QStringLiteral("Dock 设置已保存"));
    }
}

bool ZzExampleSystemPresenterPrivate::writeSetting(
    const QString &key,
    const QVariant &value)
{
    const auto result = context->settingsStore().write(
        QStringView(key), value);
    if (result) {
        return true;
    }
    qWarning().noquote()
        << "ZzPureToolsExample setting write failed:"
        << key
        << result.error().technicalMessage()
        << result.error().context();
    view->setStatusText(QStringLiteral("设置保存失败"));
    return false;
}

QVariant ZzExampleSystemPresenterPrivate::readSetting(
    const QString &key,
    const QVariant &defaultValue) const
{
    const auto result = context->settingsStore().read(
        QStringView(key), defaultValue);
    if (result) {
        return result.value();
    }
    qWarning().noquote()
        << "ZzPureToolsExample setting read failed:"
        << key
        << result.error().technicalMessage()
        << result.error().context();
    return defaultValue;
}

} // namespace ZzExample
