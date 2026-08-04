#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include "ZzWindowBackend.h"

namespace QWK {

class WidgetWindowAgent;

} // namespace QWK

namespace ZzWindowKit {

/**
 * @brief 使用 QWindowKit WidgetWindowAgent 的私有生产后端。
 */
class ZzQWindowKitBackend final : public ZzWindowBackend
{
public:
    ZzQWindowKitBackend();
    ~ZzQWindowKitBackend() override;

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

private:
    [[nodiscard]] bool hasNativeHandle() const noexcept;

    std::unique_ptr<QWK::WidgetWindowAgent> agent_;
    QPointer<QWidget> host_;
    ZzWindowCapabilities capabilities_;
    ZzWindowBackdrop backdrop_ = ZzWindowBackdrop::None;
    ZzWindowColorScheme colorScheme_ = ZzWindowColorScheme::System;
};

} // namespace ZzWindowKit
