#include "ZzQWindowKitBackend.h"

#include <cstdint>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QOperatingSystemVersion>
#include <QtCore/QPoint>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QtGlobal>
#include <QtGui/QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QtGui/QWindow>
#include <QtGui/QStyleHints>
#include <QtWidgets/QWidget>

#if defined(Q_OS_WIN)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

#if defined(Q_OS_LINUX) && defined(ZZ_WINDOWKIT_HAS_XCB) \
    && QT_CONFIG(xcb)
#  include <cstdlib>
#  include <xcb/xcb.h>
#endif

#include <QWKWidgets/widgetwindowagent.h>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include "ZzSoftwareBackdrop.h"

#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)
#include "ZzWindowKitDiagnostics.h"
#endif

namespace ZzWindowKit {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzBackendFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<ZzValue>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

[[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState>
zzUnsupportedApplyState()
{
    return ZzCore::ZzResult<ZzWindowApplyState>::success(
        ZzWindowApplyState::Unsupported);
}

#if defined(Q_OS_WIN)
[[nodiscard]] bool zzSetWindowsAlwaysOnTop(
    QWidget *host,
    bool alwaysOnTop)
{
    if (host == nullptr) {
        return false;
    }
    const HWND windowHandle = reinterpret_cast<HWND>(host->winId());
    if (windowHandle == nullptr) {
        return false;
    }
    const HWND insertAfter = alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    return SetWindowPos(
               windowHandle,
               insertAfter,
               0,
               0,
               0,
               0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
                   | SWP_NOOWNERZORDER)
        != FALSE;
}
#endif

#if defined(Q_OS_LINUX) && defined(ZZ_WINDOWKIT_HAS_XCB) \
    && QT_CONFIG(xcb)
[[nodiscard]] bool zzSetX11AlwaysOnTop(
    QWidget *host,
    bool alwaysOnTop)
{
    if (host == nullptr || host->windowHandle() == nullptr
        || !QGuiApplication::platformName().startsWith(
            QStringLiteral("xcb"), Qt::CaseInsensitive)) {
        return false;
    }
    auto *const guiApplication = qobject_cast<QGuiApplication *>(
        QCoreApplication::instance());
    if (guiApplication == nullptr) {
        return false;
    }
    auto *const nativeApplication =
        guiApplication->nativeInterface<QNativeInterface::QX11Application>();
    if (nativeApplication == nullptr || nativeApplication->connection() == nullptr) {
        return false;
    }

    xcb_connection_t *const connection = nativeApplication->connection();
    const QByteArray stateName = QByteArrayLiteral("_NET_WM_STATE");
    const QByteArray aboveName = QByteArrayLiteral("_NET_WM_STATE_ABOVE");
    const auto stateReply = xcb_intern_atom_reply(
        connection,
        xcb_intern_atom(
            connection,
            0,
            static_cast<std::uint16_t>(stateName.size()),
            stateName.constData()),
        nullptr);
    const auto aboveReply = xcb_intern_atom_reply(
        connection,
        xcb_intern_atom(
            connection,
            0,
            static_cast<std::uint16_t>(aboveName.size()),
            aboveName.constData()),
        nullptr);
    if (stateReply == nullptr || aboveReply == nullptr) {
        std::free(stateReply);
        std::free(aboveReply);
        return false;
    }

    const xcb_setup_t *const setup = xcb_get_setup(connection);
    const auto screenIterator = xcb_setup_roots_iterator(setup);
    if (screenIterator.data == nullptr) {
        std::free(stateReply);
        std::free(aboveReply);
        return false;
    }

    xcb_client_message_event_t event{};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = static_cast<xcb_window_t>(host->windowHandle()->winId());
    event.type = stateReply->atom;
    event.data.data32[0] = alwaysOnTop ? 1U : 0U;
    event.data.data32[1] = aboveReply->atom;
    event.data.data32[2] = XCB_ATOM_NONE;
    event.data.data32[3] = 1U;
    event.data.data32[4] = 0U;
    const auto cookie = xcb_send_event_checked(
        connection,
        0,
        screenIterator.data->root,
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
            | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
        reinterpret_cast<const char *>(&event));
    auto *const error = xcb_request_check(connection, cookie);
    xcb_flush(connection);
    std::free(stateReply);
    std::free(aboveReply);
    const bool sent = error == nullptr;
    std::free(error);
    return sent;
}
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
[[nodiscard]] bool zzUsesDarkColors(ZzWindowColorScheme colorScheme)
{
    switch (colorScheme) {
    case ZzWindowColorScheme::System:
        return QGuiApplication::styleHints()->colorScheme()
            == Qt::ColorScheme::Dark;
    case ZzWindowColorScheme::Light:
        return false;
    case ZzWindowColorScheme::Dark:
        return true;
    }
    return false;
}
#endif

#if defined(Q_OS_WIN)
[[nodiscard]] bool zzIsWindows10OrGreater()
{
    return QOperatingSystemVersion::current()
        >= QOperatingSystemVersion::Windows10;
}

[[nodiscard]] bool zzIsWindows11OrGreater()
{
    const auto version = QOperatingSystemVersion::current();
    return version.majorVersion() > 10
        || (version.majorVersion() == 10 && version.microVersion() >= 22000);
}

[[nodiscard]] bool zzIsWindows1122H2OrGreater()
{
    const auto version = QOperatingSystemVersion::current();
    return version.majorVersion() > 10
        || (version.majorVersion() == 10 && version.microVersion() >= 22621);
}

[[nodiscard]] bool zzWindowsSupportsBackdrop(ZzWindowBackdrop backdrop)
{
    switch (backdrop) {
    case ZzWindowBackdrop::None:
        return true;
    case ZzWindowBackdrop::Blur:
        return zzIsWindows10OrGreater();
    case ZzWindowBackdrop::Acrylic:
    case ZzWindowBackdrop::Mica:
        return zzIsWindows11OrGreater();
    case ZzWindowBackdrop::MicaAlt:
        return zzIsWindows1122H2OrGreater();
    case ZzWindowBackdrop::Automatic:
        return zzIsWindows10OrGreater();
    }
    return false;
}
#endif

} // namespace

ZzQWindowKitBackend::ZzQWindowKitBackend()
{
#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)
    Internal::ZzWindowKitDiagnostics::backendConstructed();
#endif
}

ZzQWindowKitBackend::~ZzQWindowKitBackend()
{
#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)
    if (agent_ != nullptr) {
        Internal::ZzWindowKitDiagnostics::agentDetached();
    }
#endif
    agent_.reset();
    host_.clear();
#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)
    Internal::ZzWindowKitDiagnostics::backendDestroyed();
#endif
}

ZzCore::ZzResult<void> ZzQWindowKitBackend::attach(QWidget *window)
{
    if (agent_ != nullptr) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("QWindowKit backend is already attached"));
    }
    if (window == nullptr) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("QWindowKit host must not be null"));
    }

#if defined(Q_OS_MACOS) && !ZZ_WINDOWKIT_FORCE_QT_CONTEXT
    const auto platformName = QGuiApplication::platformName();
    if (platformName != QStringLiteral("cocoa")) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::Unsupported,
            QStringLiteral(
                "QWindowKit Cocoa backend requires the cocoa Qt platform; "
                "active platform: %1")
                .arg(platformName));
    }
#endif

    auto agent = std::make_unique<QWK::WidgetWindowAgent>(nullptr);
    if (!agent->setup(window)) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("QWindowKit failed to attach the host window"));
    }

    host_ = window;
    agent_ = std::move(agent);
    softwareBackdrop_ = std::make_unique<ZzSoftwareBackdrop>();
    if (!softwareBackdrop_->attach(window)) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral(
                "ZzWindowKit failed to attach the software backdrop layer"));
    }
#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)
    Internal::ZzWindowKitDiagnostics::agentAttached();
#endif

#if ZZ_WINDOWKIT_FORCE_QT_CONTEXT
    capabilities_ = {};
#elif defined(Q_OS_WIN)
    if (zzIsWindows10OrGreater()) {
        capabilities_ = ZzWindowCapability::SystemMenu
            | ZzWindowCapability::Blur;
        if (zzIsWindows11OrGreater()) {
            capabilities_ |= ZzWindowCapability::Acrylic;
            capabilities_ |= ZzWindowCapability::Mica;
            capabilities_ |= ZzWindowCapability::SnapLayout;
            if (zzIsWindows1122H2OrGreater()) {
                capabilities_ |= ZzWindowCapability::MicaAlt;
            }
        }
    }
#elif defined(Q_OS_MACOS)
    capabilities_ = ZzWindowCapability::Blur
        | ZzWindowCapability::NativeSystemButtons;
#elif defined(Q_OS_LINUX)
    const auto platformName = QGuiApplication::platformName();
    if (platformName == QStringLiteral("xcb")
        || platformName.startsWith(QStringLiteral("wayland"))) {
        capabilities_ = ZzWindowCapability::SystemMenu;
    }
#else
    capabilities_ = {};
#endif

    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzQWindowKitBackend::configureChrome(
    const ZzWindowChromeConfiguration &configuration)
{
    if (agent_ == nullptr) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("QWindowKit backend is not attached"));
    }

    agent_->setTitleBar(configuration.titleBar);
    agent_->setSystemButton(
        QWK::WindowAgentBase::WindowIcon, configuration.windowIcon);
    agent_->setSystemButton(
        QWK::WindowAgentBase::Minimize, configuration.minimizeButton);
    agent_->setSystemButton(
        QWK::WindowAgentBase::Maximize, configuration.maximizeButton);
    agent_->setSystemButton(
        QWK::WindowAgentBase::Close, configuration.closeButton);
    for (auto *widget : configuration.interactiveWidgets) {
        agent_->setHitTestVisible(widget, true);
    }
    return ZzCore::ZzResult<void>::success();
}

ZzWindowCapabilities ZzQWindowKitBackend::capabilities() const noexcept
{
    return capabilities_;
}

ZzCore::ZzResult<ZzWindowApplyState> ZzQWindowKitBackend::setBackdrop(
    ZzWindowBackdrop backdrop)
{
    if (agent_ == nullptr || host_.isNull()) {
        return zzBackendFailure<ZzWindowApplyState>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("QWindowKit backend is not attached"));
    }

#if ZZ_WINDOWKIT_FORCE_QT_CONTEXT
    if (backdrop == ZzWindowBackdrop::None) {
        const auto software = setSoftwareBackdrop(false);
        if (!software) {
            return software;
        }
        backdrop_ = ZzWindowBackdrop::None;
        return ZzCore::ZzResult<ZzWindowApplyState>::success(
            ZzWindowApplyState::Applied);
    }
    if (backdrop == ZzWindowBackdrop::Automatic) {
        return setSoftwareBackdrop(true);
    }
    return zzUnsupportedApplyState();
#elif defined(Q_OS_WIN)
    const auto automatic = backdrop == ZzWindowBackdrop::Automatic;
    auto resolvedBackdrop = backdrop;
    if (resolvedBackdrop == ZzWindowBackdrop::Automatic) {
        if (zzIsWindows11OrGreater()) {
            resolvedBackdrop = ZzWindowBackdrop::Mica;
        } else if (zzIsWindows10OrGreater()) {
            resolvedBackdrop = ZzWindowBackdrop::Blur;
        }
    }
    if (!zzWindowsSupportsBackdrop(resolvedBackdrop)
        || resolvedBackdrop == ZzWindowBackdrop::Automatic) {
        if (automatic) {
            backdrop_ = ZzWindowBackdrop::Automatic;
            return setSoftwareBackdrop(true);
        }
        return zzUnsupportedApplyState();
    }

    const auto micaDisabled = agent_->setWindowAttribute(
        QStringLiteral("mica"), false);
    const auto micaAltDisabled = agent_->setWindowAttribute(
        QStringLiteral("mica-alt"), false);
    const auto acrylicDisabled = agent_->setWindowAttribute(
        QStringLiteral("acrylic-material"), false);
    const auto blurDisabled = agent_->setWindowAttribute(
        QStringLiteral("dwm-blur"), false);

    bool previousDisabled = true;
    switch (backdrop_) {
    case ZzWindowBackdrop::Mica:
        previousDisabled = micaDisabled;
        break;
    case ZzWindowBackdrop::MicaAlt:
        previousDisabled = micaAltDisabled;
        break;
    case ZzWindowBackdrop::Acrylic:
        previousDisabled = acrylicDisabled;
        break;
    case ZzWindowBackdrop::Blur:
        previousDisabled = blurDisabled;
        break;
    case ZzWindowBackdrop::None:
    case ZzWindowBackdrop::Automatic:
        break;
    }

    bool requestedEnabled = true;
    switch (resolvedBackdrop) {
    case ZzWindowBackdrop::Mica:
        requestedEnabled = agent_->setWindowAttribute(
            QStringLiteral("mica"), true);
        break;
    case ZzWindowBackdrop::MicaAlt:
        requestedEnabled = agent_->setWindowAttribute(
            QStringLiteral("mica-alt"), true);
        break;
    case ZzWindowBackdrop::Acrylic:
        requestedEnabled = agent_->setWindowAttribute(
            QStringLiteral("acrylic-material"), true);
        break;
    case ZzWindowBackdrop::Blur:
        requestedEnabled = agent_->setWindowAttribute(
            QStringLiteral("dwm-blur"), true);
        break;
    case ZzWindowBackdrop::None:
    case ZzWindowBackdrop::Automatic:
        break;
    }
    if (!previousDisabled || !requestedEnabled) {
        if (automatic) {
            backdrop_ = ZzWindowBackdrop::Automatic;
            const auto software = setSoftwareBackdrop(true);
            if (software) {
                return software;
            }
        }
        return zzBackendFailure<ZzWindowApplyState>(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("QWindowKit failed to update the window backdrop"));
    }

    auto software = setSoftwareBackdrop(false);
    if (!software) {
        return software;
    }
    backdrop_ = resolvedBackdrop;
    return ZzCore::ZzResult<ZzWindowApplyState>::success(
        hasNativeHandle()
            ? ZzWindowApplyState::Applied
            : ZzWindowApplyState::Deferred);
#elif defined(Q_OS_MACOS)
    const auto automatic = backdrop == ZzWindowBackdrop::Automatic;
    auto resolvedBackdrop = backdrop;
    if (resolvedBackdrop == ZzWindowBackdrop::Automatic) {
        resolvedBackdrop = ZzWindowBackdrop::Blur;
    }
    if (resolvedBackdrop != ZzWindowBackdrop::None
        && resolvedBackdrop != ZzWindowBackdrop::Blur) {
        if (automatic) {
            backdrop_ = ZzWindowBackdrop::Automatic;
            return setSoftwareBackdrop(true);
        }
        return zzUnsupportedApplyState();
    }

    const auto attribute = resolvedBackdrop == ZzWindowBackdrop::None
        ? QStringLiteral("none")
        : (zzUsesDarkColors(colorScheme_)
               ? QStringLiteral("dark")
               : QStringLiteral("light"));
    if (!agent_->setWindowAttribute(
            QStringLiteral("blur-effect"), attribute)) {
        if (automatic) {
            backdrop_ = ZzWindowBackdrop::Automatic;
            const auto software = setSoftwareBackdrop(true);
            if (software) {
                return software;
            }
        }
        return zzBackendFailure<ZzWindowApplyState>(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("QWindowKit failed to update the window backdrop"));
    }
    auto software = setSoftwareBackdrop(false);
    if (!software) {
        return software;
    }
    backdrop_ = resolvedBackdrop;
    return ZzCore::ZzResult<ZzWindowApplyState>::success(
        hasNativeHandle()
            ? ZzWindowApplyState::Applied
            : ZzWindowApplyState::Deferred);
#elif defined(Q_OS_LINUX)
    if (backdrop == ZzWindowBackdrop::Automatic) {
        backdrop_ = ZzWindowBackdrop::Automatic;
        return setSoftwareBackdrop(true);
    }
    if (backdrop != ZzWindowBackdrop::None) {
        return zzUnsupportedApplyState();
    }
    auto software = setSoftwareBackdrop(false);
    if (!software) {
        return software;
    }
    backdrop_ = ZzWindowBackdrop::None;
    return ZzCore::ZzResult<ZzWindowApplyState>::success(
        ZzWindowApplyState::Applied);
#else
    Q_UNUSED(backdrop);
    return zzUnsupportedApplyState();
#endif
}

ZzCore::ZzResult<ZzWindowApplyState> ZzQWindowKitBackend::setColorScheme(
    ZzWindowColorScheme colorScheme)
{
    if (agent_ == nullptr || host_.isNull()) {
        return zzBackendFailure<ZzWindowApplyState>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("QWindowKit backend is not attached"));
    }

#if ZZ_WINDOWKIT_FORCE_QT_CONTEXT
    Q_UNUSED(colorScheme);
    return zzUnsupportedApplyState();
#elif defined(Q_OS_WIN)
    if (!zzIsWindows10OrGreater()) {
        return zzUnsupportedApplyState();
    }
    if (!agent_->setWindowAttribute(
            QStringLiteral("dark-mode"), zzUsesDarkColors(colorScheme))) {
        return zzBackendFailure<ZzWindowApplyState>(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("QWindowKit failed to update the color scheme"));
    }
    colorScheme_ = colorScheme;
    return ZzCore::ZzResult<ZzWindowApplyState>::success(
        hasNativeHandle()
            ? ZzWindowApplyState::Applied
            : ZzWindowApplyState::Deferred);
#elif defined(Q_OS_MACOS)
    if (backdrop_ != ZzWindowBackdrop::Blur) {
        return zzUnsupportedApplyState();
    }
    const auto attribute = zzUsesDarkColors(colorScheme)
        ? QStringLiteral("dark")
        : QStringLiteral("light");
    if (!agent_->setWindowAttribute(
            QStringLiteral("blur-effect"), attribute)) {
        return zzBackendFailure<ZzWindowApplyState>(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("QWindowKit failed to update the color scheme"));
    }
    colorScheme_ = colorScheme;
    return ZzCore::ZzResult<ZzWindowApplyState>::success(
        hasNativeHandle()
            ? ZzWindowApplyState::Applied
            : ZzWindowApplyState::Deferred);
#else
    Q_UNUSED(colorScheme);
    return zzUnsupportedApplyState();
#endif
}

ZzCore::ZzResult<void> ZzQWindowKitBackend::setAlwaysOnTop(
    bool alwaysOnTop)
{
    if (agent_ == nullptr || host_.isNull()) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("QWindowKit backend is not attached"));
    }

    const Qt::WindowFlags requestedFlags = [&] {
        Qt::WindowFlags flags = host_->windowFlags();
        flags.setFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
        return flags;
    }();
    QWindow *const windowHandle = host_->windowHandle();
    if (host_->isVisible() && windowHandle != nullptr) {
        // QWindow 更新原生标志不会触发 QWidget 的隐藏/显示重建。
        windowHandle->setFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
        host_->overrideWindowFlags(requestedFlags);
#if defined(Q_OS_WIN)
        if (!zzSetWindowsAlwaysOnTop(host_, alwaysOnTop)) {
            return zzBackendFailure<void>(
                ZzCore::ZzErrorCode::Backend,
                QStringLiteral(
                    "Windows failed to update the topmost window state"));
        }
#elif defined(Q_OS_LINUX) && defined(ZZ_WINDOWKIT_HAS_XCB) \
    && QT_CONFIG(xcb)
        if (QGuiApplication::platformName().startsWith(
                QStringLiteral("xcb"), Qt::CaseInsensitive)
            && !zzSetX11AlwaysOnTop(host_, alwaysOnTop)) {
            return zzBackendFailure<void>(
                ZzCore::ZzErrorCode::Backend,
                QStringLiteral(
                    "X11 failed to submit the topmost window request"));
        }
#endif
        return ZzCore::ZzResult<void>::success();
    }

    // 不可见窗口尚未暴露原生句柄，使用 QWidget API 不会造成可见闪烁。
    host_->setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
    return ZzCore::ZzResult<void>::success();
}

ZzCore::ZzResult<void> ZzQWindowKitBackend::showSystemMenu(
    const QPoint &globalPosition)
{
    if (agent_ == nullptr || host_.isNull()) {
        return zzBackendFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("QWindowKit backend is not attached"));
    }

#if ZZ_WINDOWKIT_FORCE_QT_CONTEXT || defined(Q_OS_MACOS)
    Q_UNUSED(globalPosition);
    return zzBackendFailure<void>(
        ZzCore::ZzErrorCode::Unsupported,
        QStringLiteral("system menu is unavailable on this window backend"));
#elif defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    agent_->showSystemMenu(globalPosition);
    return ZzCore::ZzResult<void>::success();
#else
    Q_UNUSED(globalPosition);
    return zzBackendFailure<void>(
        ZzCore::ZzErrorCode::Unsupported,
        QStringLiteral("system menu is unavailable on this platform"));
#endif
}

bool ZzQWindowKitBackend::hasNativeHandle() const noexcept
{
    return !host_.isNull()
        && host_->testAttribute(Qt::WA_WState_Created)
        && host_->windowHandle() != nullptr;
}

ZzCore::ZzResult<ZzWindowApplyState>
ZzQWindowKitBackend::setSoftwareBackdrop(bool enabled)
{
    if (softwareBackdrop_ == nullptr
        || !softwareBackdrop_->setEnabled(enabled)) {
        return zzBackendFailure<ZzWindowApplyState>(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral(
                "ZzWindowKit failed to update the software backdrop layer"));
    }
    return ZzCore::ZzResult<ZzWindowApplyState>::success(
        ZzWindowApplyState::Applied);
}

} // namespace ZzWindowKit
