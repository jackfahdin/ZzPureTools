#pragma once

namespace ZzCore {

/**
 * @brief 配置 Qt 全局消息处理器到 ZzLog 的转发行为。
 */
struct ZzQtLogBridgeConfig final
{
    /** @brief 转发到 ZzLog 后是否继续调用安装前的 Qt 消息处理器。 */
    bool chainPreviousHandler = false;
};

} // namespace ZzCore
