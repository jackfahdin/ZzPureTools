#pragma once

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

namespace ZzPureTools {

/** @brief 不可变地描述已注册面板的纯值身份。 */
class ZzWorkspaceLayoutStatePrivate final
{
public:
    /** @brief 指定面板在工作区中的容器类别。 */
    enum class ZzPanelKind : unsigned char
    {
        Side,
        Bottom,
        Dock
    };

    /** @brief 保存不依赖 QWidget 的面板身份。 */
    struct ZzPanelIdentity final
    {
        QString id;
        ZzPanelKind kind = ZzPanelKind::Side;

        [[nodiscard]] bool operator==(const ZzPanelIdentity &) const = default;
    };

    /** @brief 保存单个物理侧栏的目标状态。 */
    struct ZzSideProjection final
    {
        QStringList order;
        QStringList visible;
        QString current;
        bool collapsed = true;
        int width = 280;

        [[nodiscard]] bool operator==(const ZzSideProjection &) const = default;
    };

    /** @brief 保存中央底部面板的目标状态。 */
    struct ZzBottomProjection final
    {
        QStringList order;
        QStringList visible;
        QString current;
        bool collapsed = true;
        int height = 0;

        [[nodiscard]] bool operator==(const ZzBottomProjection &) const = default;
    };

    /** @brief 保存原生 Dock 的无 QWidget 布局投影。 */
    struct ZzDockProjection final
    {
        QByteArray state;
        QStringList visible;

        [[nodiscard]] bool operator==(const ZzDockProjection &) const = default;
    };

    /** @brief 保存中央分屏中可见组及其 splitter 尺寸。 */
    struct ZzSplitProjection final
    {
        QStringList visible;
        QList<int> sizes;
        int currentIndex = -1;

        [[nodiscard]] bool operator==(const ZzSplitProjection &) const = default;
    };

    /** @brief 保存四个 Activity 分组和由 Side 推导的当前项。 */
    struct ZzActivityProjection final
    {
        QStringList leftPrimary;
        QStringList leftSecondary;
        QStringList rightPrimary;
        QStringList rightSecondary;
        QString leftCurrent;
        QString rightCurrent;

        [[nodiscard]] bool operator==(const ZzActivityProjection &) const = default;
    };

    /** @brief 保存标题策略及其纯值输入。 */
    struct ZzTitleProjection final
    {
        ZzWorkspaceTitleMode mode = ZzWorkspaceTitleMode::Application;
        QString applicationTitle;
        QString customTitle;

        [[nodiscard]] bool operator==(const ZzTitleProjection &) const = default;
    };

    /** @brief 描述一次布局应用所需的全部目标值。 */
    struct ZzWorkspaceProjection
    {
        ZzSideProjection leftSide;
        ZzSideProjection rightSide;
        ZzBottomProjection bottom;
        ZzDockProjection dock;
        ZzSplitProjection split;
        ZzActivityProjection activity;
        ZzTitleProjection title;

        [[nodiscard]] bool operator==(const ZzWorkspaceProjection &) const = default;
    };

    /** @brief 记录规划开始时的投影和已注册面板身份。 */
    struct ZzWorkspaceSnapshot final : ZzWorkspaceProjection
    {
        QList<ZzPanelIdentity> identities;
    };

    /** @brief 保存解码布局中的可选目标和明确的 Side current 请求。 */
    struct ZzLayoutRequest final
    {
        std::optional<ZzWorkspaceProjection> projection;
        QString leftCurrent;
        QString rightCurrent;
    };

    /** @brief 保存一次 Activity 面板移动请求。 */
    struct ZzActivityMoveRequest final
    {
        QString panelId;
        ZzFluentUI::ZzActivityArea targetArea =
            ZzFluentUI::ZzActivityArea::LeftPrimary;
        int targetRow = 0;
    };

    /**
     * @brief 从快照和解码请求构造独立的恢复目标。
     *
     * 返回值不保留任何可写观察态引用，且 Activity current 始终由
     * Side 目标重新推导。
     */
    [[nodiscard]] static std::optional<ZzWorkspaceProjection>
    buildRestoreTarget(
        const ZzWorkspaceSnapshot &snapshot,
        const ZzLayoutRequest &request);

    /** @brief 从快照复制后，规划一次指定 Activity 区域中的面板移动。 */
    [[nodiscard]] static std::optional<ZzWorkspaceProjection>
    buildActivityMoveTarget(
        const ZzWorkspaceSnapshot &snapshot,
        const QString &panelId,
        ZzFluentUI::ZzActivityArea targetArea,
        int targetRow);

    /** @brief 比较两个已归一化布局目标的全部纯值字段。 */
    [[nodiscard]] static bool equals(
        const ZzWorkspaceProjection &left,
        const ZzWorkspaceProjection &right) noexcept;
};

} // namespace ZzPureTools
