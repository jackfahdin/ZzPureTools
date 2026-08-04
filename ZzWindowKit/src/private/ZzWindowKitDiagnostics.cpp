#include "ZzWindowKitDiagnostics.h"

#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)

namespace ZzWindowKit {
namespace Internal {

std::atomic<qsizetype> ZzWindowKitDiagnostics::liveBackendCount_{0};
std::atomic<qsizetype> ZzWindowKitDiagnostics::liveAgentCount_{0};

void ZzWindowKitDiagnostics::backendConstructed() noexcept
{
    liveBackendCount_.fetch_add(1, std::memory_order_relaxed);
}

void ZzWindowKitDiagnostics::backendDestroyed() noexcept
{
    liveBackendCount_.fetch_sub(1, std::memory_order_relaxed);
}

void ZzWindowKitDiagnostics::agentAttached() noexcept
{
    liveAgentCount_.fetch_add(1, std::memory_order_relaxed);
}

void ZzWindowKitDiagnostics::agentDetached() noexcept
{
    liveAgentCount_.fetch_sub(1, std::memory_order_relaxed);
}

qsizetype ZzWindowKitDiagnostics::liveBackendCount() noexcept
{
    return liveBackendCount_.load(std::memory_order_relaxed);
}

qsizetype ZzWindowKitDiagnostics::liveAgentCount() noexcept
{
    return liveAgentCount_.load(std::memory_order_relaxed);
}

} // namespace Internal
} // namespace ZzWindowKit

#endif
