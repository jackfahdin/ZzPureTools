#pragma once

#include <memory>
#include <utility>

#include <ZzWindowKit/ZzWindowAgent.h>

#include "ZzWindowBackend.h"

namespace ZzWindowKit {

/**
 * @brief 仅供单元测试注入窗口后端的私有构造入口。
 */
class ZzWindowAgentTestAccess final
{
public:
    /**
     * @brief 创建独占给定假后端的窗口代理。
     * @param backend 非空后端，所有权转移给代理。
     * @return 堆分配且不可移动的窗口代理。
     */
    [[nodiscard]] static std::unique_ptr<ZzWindowAgent> create(
        std::unique_ptr<ZzWindowBackend> backend)
    {
        return std::unique_ptr<ZzWindowAgent>(
            new ZzWindowAgent(std::move(backend), nullptr));
    }
};

} // namespace ZzWindowKit
