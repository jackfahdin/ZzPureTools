#pragma once

#include <cstdint>

namespace ZzCore {

/**
 * @brief 描述跨组件通用失败类别。
 */
enum class ZzErrorCode : std::uint16_t
{
    /** @brief 没有错误。 */
    None = 0,
    /** @brief 参数不满足接口约束。 */
    InvalidArgument,
    /** @brief 当前对象或系统状态不允许执行操作。 */
    InvalidState,
    /** @brief 操作被协作取消。 */
    Cancelled,
    /** @brief 操作未在时限内完成。 */
    TimedOut,
    /** @brief 请求的资源不存在。 */
    NotFound,
    /** @brief 当前平台或后端不支持该能力。 */
    Unsupported,
    /** @brief 文件或其他输入输出操作失败。 */
    Io,
    /** @brief 私有后端报告失败。 */
    Backend,
    /** @brief 无法映射到已知类别的失败。 */
    Unknown
};

} // namespace ZzCore
