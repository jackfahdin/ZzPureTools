#pragma once

#include <QtCore/QByteArray>

#include <ZzCore/ZzResult.h>

#include "ZzWorkspaceLayoutStatePrivate.h"

namespace ZzPureTools {

/**
 * @brief 有界读写工作区 envelope，并规范化其中的 Split 纯值布局。
 *
 * codec 不访问 QWidget。schema 1 的 current tab index 仅保存在首次迁移
 * request 的 root leaf 中；既有 Split 字节格式不持久化该迁移专用字段。
 */
class ZzWorkspaceLayoutCodecPrivate final
{
public:
    /** @brief 解码 schema 1 或 schema 2 为完整的纯值布局请求。 */
    [[nodiscard]] static ZzCore::ZzResult<
        ZzWorkspaceLayoutStatePrivate::ZzLayoutRequest>
    decode(const QByteArray &encoded);

    /** @brief 校验纯值请求并按稳定 schema 2 字节合同编码。 */
    [[nodiscard]] static ZzCore::ZzResult<QByteArray>
    encodeVersionTwo(
        const ZzWorkspaceLayoutStatePrivate::ZzLayoutRequest &request);

    /** @brief 解码并使用同一纯值 writer 重新编码 Split 布局。 */
    [[nodiscard]] static ZzCore::ZzResult<QByteArray>
    canonicalizeSplit(const QByteArray &encoded);
};

} // namespace ZzPureTools
