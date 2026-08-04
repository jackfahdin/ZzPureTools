#pragma once

#include <utility>

#include <QtCore/QPoint>
#include <QtCore/QStringList>

#include <ZzCore/ZzError.h>

#include "ZzWindowBackend.h"

namespace ZzWindowKit {

/**
 * @brief 为 facade 单元测试记录完整调用序列的可配置假后端。
 */
class ZzFakeWindowBackend final : public ZzWindowBackend
{
public:
    [[nodiscard]] ZzCore::ZzResult<void> attach(QWidget *window) override;
    [[nodiscard]] ZzCore::ZzResult<void> configureChrome(
        const ZzWindowChromeConfiguration &configuration) override;
    [[nodiscard]] ZzWindowCapabilities capabilities() const noexcept override;
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setBackdrop(
        ZzWindowBackdrop backdrop) override;
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setColorScheme(
        ZzWindowColorScheme colorScheme) override;
    [[nodiscard]] ZzCore::ZzResult<void> showSystemMenu(
        const QPoint &globalPosition) override;

    [[nodiscard]] int attachCalls() const noexcept;
    [[nodiscard]] int configureCalls() const noexcept;
    [[nodiscard]] int backdropCalls() const noexcept;
    [[nodiscard]] int colorSchemeCalls() const noexcept;
    [[nodiscard]] int systemMenuCalls() const noexcept;
    [[nodiscard]] const QStringList &calls() const noexcept;
    [[nodiscard]] const ZzWindowChromeConfiguration &lastConfiguration()
        const noexcept;

    void setCapabilities(ZzWindowCapabilities capabilities) noexcept;
    void setAttachResult(ZzCore::ZzResult<void> result);
    void setConfigureResult(ZzCore::ZzResult<void> result);
    void setBackdropResult(
        ZzCore::ZzResult<ZzWindowApplyState> result);
    void setColorSchemeResult(
        ZzCore::ZzResult<ZzWindowApplyState> result);
    void setSystemMenuResult(ZzCore::ZzResult<void> result);

private:
    int attachCalls_ = 0;
    int configureCalls_ = 0;
    int backdropCalls_ = 0;
    int colorSchemeCalls_ = 0;
    int systemMenuCalls_ = 0;
    QStringList calls_;
    ZzWindowChromeConfiguration lastConfiguration_;
    ZzWindowCapabilities capabilities_;
    ZzCore::ZzResult<void> attachResult_ =
        ZzCore::ZzResult<void>::success();
    ZzCore::ZzResult<void> configureResult_ =
        ZzCore::ZzResult<void>::success();
    ZzCore::ZzResult<ZzWindowApplyState> backdropResult_ =
        ZzCore::ZzResult<ZzWindowApplyState>::success(
            ZzWindowApplyState::Applied);
    ZzCore::ZzResult<ZzWindowApplyState> colorSchemeResult_ =
        ZzCore::ZzResult<ZzWindowApplyState>::success(
            ZzWindowApplyState::Applied);
    ZzCore::ZzResult<void> systemMenuResult_ =
        ZzCore::ZzResult<void>::success();
};

} // namespace ZzWindowKit
