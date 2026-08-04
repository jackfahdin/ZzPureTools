#include "ZzFakeWindowBackend.h"

#include <QtWidgets/QWidget>

namespace ZzWindowKit {

ZzCore::ZzResult<void> ZzFakeWindowBackend::attach(QWidget *window)
{
    Q_UNUSED(window);
    ++attachCalls_;
    calls_.append(QStringLiteral("attach"));
    return attachResult_;
}

ZzCore::ZzResult<void> ZzFakeWindowBackend::configureChrome(
    const ZzWindowChromeConfiguration &configuration)
{
    ++configureCalls_;
    lastConfiguration_ = configuration;
    calls_.append(QStringLiteral("title-bar"));
    calls_.append(QStringLiteral("icon"));
    calls_.append(QStringLiteral("minimize"));
    calls_.append(QStringLiteral("maximize"));
    calls_.append(QStringLiteral("close"));
    for (const auto *widget : configuration.interactiveWidgets) {
        calls_.append(QStringLiteral("interactive:%1").arg(
            widget->objectName()));
    }
    return configureResult_;
}

ZzWindowCapabilities ZzFakeWindowBackend::capabilities() const noexcept
{
    return capabilities_;
}

ZzCore::ZzResult<ZzWindowApplyState> ZzFakeWindowBackend::setBackdrop(
    ZzWindowBackdrop backdrop)
{
    Q_UNUSED(backdrop);
    ++backdropCalls_;
    calls_.append(QStringLiteral("backdrop"));
    return backdropResult_;
}

ZzCore::ZzResult<ZzWindowApplyState> ZzFakeWindowBackend::setColorScheme(
    ZzWindowColorScheme colorScheme)
{
    Q_UNUSED(colorScheme);
    ++colorSchemeCalls_;
    calls_.append(QStringLiteral("color-scheme"));
    return colorSchemeResult_;
}

ZzCore::ZzResult<void> ZzFakeWindowBackend::showSystemMenu(
    const QPoint &globalPosition)
{
    Q_UNUSED(globalPosition);
    ++systemMenuCalls_;
    calls_.append(QStringLiteral("system-menu"));
    return systemMenuResult_;
}

int ZzFakeWindowBackend::attachCalls() const noexcept
{
    return attachCalls_;
}

int ZzFakeWindowBackend::configureCalls() const noexcept
{
    return configureCalls_;
}

int ZzFakeWindowBackend::backdropCalls() const noexcept
{
    return backdropCalls_;
}

int ZzFakeWindowBackend::colorSchemeCalls() const noexcept
{
    return colorSchemeCalls_;
}

int ZzFakeWindowBackend::systemMenuCalls() const noexcept
{
    return systemMenuCalls_;
}

const QStringList &ZzFakeWindowBackend::calls() const noexcept
{
    return calls_;
}

const ZzWindowChromeConfiguration &
ZzFakeWindowBackend::lastConfiguration() const noexcept
{
    return lastConfiguration_;
}

void ZzFakeWindowBackend::setCapabilities(
    ZzWindowCapabilities capabilities) noexcept
{
    capabilities_ = capabilities;
}

void ZzFakeWindowBackend::setAttachResult(ZzCore::ZzResult<void> result)
{
    attachResult_ = std::move(result);
}

void ZzFakeWindowBackend::setConfigureResult(
    ZzCore::ZzResult<void> result)
{
    configureResult_ = std::move(result);
}

void ZzFakeWindowBackend::setBackdropResult(
    ZzCore::ZzResult<ZzWindowApplyState> result)
{
    backdropResult_ = std::move(result);
}

void ZzFakeWindowBackend::setColorSchemeResult(
    ZzCore::ZzResult<ZzWindowApplyState> result)
{
    colorSchemeResult_ = std::move(result);
}

void ZzFakeWindowBackend::setSystemMenuResult(
    ZzCore::ZzResult<void> result)
{
    systemMenuResult_ = std::move(result);
}

} // namespace ZzWindowKit
