#pragma once

#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)

#include <atomic>

#include <QtCore/QtTypes>

#include <ZzWindowKit/ZzWindowKitExport.h>

namespace ZzWindowKit {
namespace Internal {

/**
 * @brief 暴露 benchmark 构建中可观察的 WindowKit 生命周期计数。
 *
 * 该类型只统计 ZzWindowKit 适配层拥有的 backend 和 QWK agent，不能观察
 * QWindowKit 私有平台上下文内部的 native event filter。
 */
class ZZ_WINDOWKIT_EXPORT ZzWindowKitDiagnostics final
{
public:
    ZzWindowKitDiagnostics() = delete;

    /** @brief 记录一个 private backend 已构造。 */
    static void backendConstructed() noexcept;

    /** @brief 记录一个 private backend 已析构。 */
    static void backendDestroyed() noexcept;

    /** @brief 记录一个 QWK agent 已成功附着。 */
    static void agentAttached() noexcept;

    /** @brief 记录一个已附着 QWK agent 已分离。 */
    static void agentDetached() noexcept;

    /** @brief 返回当前存活的 backend 数。 */
    [[nodiscard]] static qsizetype liveBackendCount() noexcept;

    /** @brief 返回当前处于 attached 状态的 agent 数。 */
    [[nodiscard]] static qsizetype liveAgentCount() noexcept;

private:
    static std::atomic<qsizetype> liveBackendCount_;
    static std::atomic<qsizetype> liveAgentCount_;
};

} // namespace Internal
} // namespace ZzWindowKit

#endif
