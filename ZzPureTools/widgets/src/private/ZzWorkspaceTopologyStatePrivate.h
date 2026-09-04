#pragma once

#include <cstdint>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QUuid>

#include <ZzFluentUI/ZzWorkspacePageId.h>
#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>

namespace ZzPureTools {

/**
 * @brief 保存多窗口工作区的有界纯值拓扑。
 *
 * 此类型不持有 QObject/QWidget，也不包含任何恢复回调；它只描述窗口、
 * 页面归属和页面来源栈，供拓扑 codec 在 GUI 线程外安全校验和传递。
 */
class ZzWorkspaceTopologyStatePrivate final
{
public:
    /** @brief 允许保存的窗口数量上限。 */
    static constexpr int MaximumWindows = 32;
    /** @brief 全部窗口合计允许保存的页面数量上限。 */
    static constexpr int MaximumPages = 4096;
    /** @brief 单个工作区允许保存的组数量上限。 */
    static constexpr int MaximumGroupsPerWindow = 64;
    /** @brief 分屏树允许的最大深度。 */
    static constexpr int MaximumTreeDepth = 16;
    /** @brief 字符串允许的 UTF-16 code unit 数量上限。 */
    static constexpr int MaximumStringLength = 256;
    /** @brief 单个 ZZSW 工作区状态的字节上限。 */
    static constexpr int MaximumWorkspaceStateSize = 1024 * 1024;
    /** @brief ZZWT payload 的字节上限。 */
    static constexpr int MaximumPayloadSize = 16 * 1024 * 1024;
    /** @brief 单个页面来源栈的深度上限。 */
    static constexpr int MaximumOriginDepth = 32;

    /** @brief 记录页面曾经所在窗口、组和索引。 */
    struct ZzPageOrigin final
    {
        QUuid windowId;
        QString groupId;
        qint32 index = -1;

        [[nodiscard]] bool operator==(const ZzPageOrigin &) const = default;
    };

    /** @brief 记录页面稳定身份、当前归属和可选布局键。 */
    struct ZzPageState final
    {
        ZzFluentUI::ZzWorkspacePageId pageId;
        QString layoutKey;
        QUuid windowId;
        QString groupId;
        qint32 index = -1;
        QList<ZzPageOrigin> origins;

        [[nodiscard]] bool operator==(const ZzPageState &) const = default;
    };

    /** @brief 记录一个顶层窗口及其不透明 ZZSW 工作区状态。 */
    struct ZzWindowState final
    {
        QUuid windowId;
        ZzWorkspaceWindowConfiguration configuration;
        QRect geometry;
        QString screenName;
        bool visible = false;
        bool maximized = false;
        bool alwaysOnTop = false;
        int treeDepth = 1;
        QByteArray workspaceState;
        QList<ZzPageState> pages;

        /** @brief 比较窗口持久化字段；图标由宿主配置而非拓扑 blob 管理。 */
        [[nodiscard]] bool operator==(const ZzWindowState &other) const noexcept;
    };

    /** @brief 兼容不同调用方命名的页面来源别名。 */
    using ZzOriginState = ZzPageOrigin;
    /** @brief 页面来源记录的兼容别名。 */
    using ZzPageOriginState = ZzPageOrigin;
    /** @brief 兼容不同调用方命名的页面记录别名。 */
    using ZzPage = ZzPageState;
    /** @brief 页面记录的兼容别名。 */
    using ZzPageRecord = ZzPageState;
    /** @brief 兼容不同调用方命名的窗口记录别名。 */
    using ZzWindow = ZzWindowState;
    /** @brief 窗口记录的兼容别名。 */
    using ZzWindowRecord = ZzWindowState;

    QList<ZzWindowState> windows;

    /** @brief 比较两个拓扑是否逐字段相等。 */
    [[nodiscard]] bool operator==(
        const ZzWorkspaceTopologyStatePrivate &other) const noexcept;

    /** @brief 校验指定拓扑是否满足全部 ZZWT 边界和交叉引用约束。 */
    [[nodiscard]] static bool validate(
        const ZzWorkspaceTopologyStatePrivate &state) noexcept;

    /**
     * @brief 校验拓扑的容量、身份、归属和来源栈约束。
     * @return 所有约束满足时返回 true。
     */
    [[nodiscard]] bool isValid() const noexcept;
};

using ZzWorkspaceTopologyState = ZzWorkspaceTopologyStatePrivate;
using ZzWorkspaceTopology = ZzWorkspaceTopologyStatePrivate;
using ZzTopologyState = ZzWorkspaceTopologyStatePrivate;

} // namespace ZzPureTools
