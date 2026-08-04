#pragma once

#include <cstdint>

namespace ZzWindowKit {

/**
 * @brief 描述平台效果请求的非失败结果。
 */
enum class ZzWindowApplyState : std::uint8_t
{
    /** @brief 效果已经应用到现有原生窗口。 */
    Applied,
    /** @brief 请求已保存，将在原生窗口创建后生效。 */
    Deferred,
    /** @brief 当前平台、版本或后端不支持该效果。 */
    Unsupported
};

} // namespace ZzWindowKit
