#pragma once

#include <QtCore/QByteArray>

#include <ZzCore/ZzResult.h>

#include "ZzWorkspaceTopologyStatePrivate.h"

namespace ZzPureTools {

/**
 * @brief 编解码有界 ZZWT schema v2 拓扑，并兼容导入 ZZSW v1 字节。
 *
 * 编解码器只产生纯值对象，不在解码阶段创建或访问 QWidget/QObject。
 */
class ZzWorkspaceTopologyCodecPrivate final
{
public:
    /** @brief 将拓扑按稳定的 ZZWT schema v2 编码并附加 SHA-256。 */
    [[nodiscard]] static ZzCore::ZzResult<QByteArray> encode(
        const ZzWorkspaceTopologyStatePrivate &state);

    /** @brief 解码 ZZWT v2，或将原始 ZZSW v1 包装为默认窗口。 */
    [[nodiscard]] static ZzCore::ZzResult<
        ZzWorkspaceTopologyStatePrivate> decode(const QByteArray &encoded);

    /** @brief `encode` 的显式 schema v2 别名。 */
    [[nodiscard]] static ZzCore::ZzResult<QByteArray> encodeVersionTwo(
        const ZzWorkspaceTopologyStatePrivate &state)
    {
        return encode(state);
    }
};

} // namespace ZzPureTools
