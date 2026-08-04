#pragma once

#include <memory>

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowCapability.h>

#include "ZzWindowBackend.h"

namespace ZzWindowKit {

class ZzWindowAgent;

/**
 * @brief 实现窗口代理的 GUI 线程状态机和控件归属验证。
 */
class ZzWindowAgentPrivate final
{
public:
    ZzWindowAgentPrivate(
        ZzWindowAgent *agent,
        std::unique_ptr<ZzWindowBackend> windowBackend);
    ~ZzWindowAgentPrivate();

    [[nodiscard]] ZzCore::ZzResult<void> attach(QWidget *window);
    [[nodiscard]] ZzCore::ZzResult<void> configureChrome(
        const ZzWindowChromeConfiguration &configuration);
    [[nodiscard]] ZzWindowCapabilities capabilities() const noexcept;
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setBackdrop(
        ZzWindowBackdrop backdrop);
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setColorScheme(
        ZzWindowColorScheme colorScheme);
    [[nodiscard]] ZzCore::ZzResult<void> showSystemMenu(
        const QPoint &globalPosition);

    [[nodiscard]] ZzCore::ZzResult<void> validateChrome(
        const ZzWindowChromeConfiguration &configuration) const;
    [[nodiscard]] ZzCore::ZzResult<void> validateActiveOperation(
        QString operation);

    ZzWindowAgent *const q_ptr;
    std::unique_ptr<ZzWindowBackend> backend;
    QPointer<QWidget> host;
    ZzWindowAgentState state = ZzWindowAgentState::Detached;
    QMetaObject::Connection hostDestroyedConnection;
};

} // namespace ZzWindowKit
