#pragma once

#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActivityArea.h>

namespace ZzFluentUI {

class ZzDockPanel;

} // namespace ZzFluentUI

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

    /** @brief 指定标题投影使用的纯值组合策略。 */
    enum class ZzTitleMode : unsigned char
    {
        Application,
        CurrentTab,
        CurrentTabAndApplication,
        Custom
    };

    /** @brief 保存任意 QObject 子系统的受保护身份。 */
    struct ZzSubsystemIdentity final
    {
        QPointer<QObject> object;
        QObject *rawObject = nullptr;

        [[nodiscard]] bool operator==(
            const ZzSubsystemIdentity &) const = default;
    };

    /** @brief 保存已注册面板的运行时身份与注册代次。 */
    struct ZzPanelIdentity final
    {
        QString id;
        ZzPanelKind kind = ZzPanelKind::Side;
        QPointer<QWidget> widget;
        QWidget *rawWidget = nullptr;
        quint64 registrationGeneration = 0;
        QPointer<QObject> dock;
        ZzFluentUI::ZzDockPanel *rawDock = nullptr;

        [[nodiscard]] bool operator==(const ZzPanelIdentity &) const = default;
    };

    /** @brief 记录内容所属 stack 及从 Pane 到 stack 的祖先身份要求。 */
    struct ZzContentPlacement final
    {
        QString panelId;
        ZzSubsystemIdentity stackIdentity;
        QList<ZzSubsystemIdentity> ancestry;

        [[nodiscard]] bool operator==(
            const ZzContentPlacement &) const = default;
    };

    /** @brief 保存单个物理侧栏的目标状态。 */
    struct ZzSideProjection final
    {
        ZzSubsystemIdentity paneIdentity;
        ZzSubsystemIdentity stackIdentity;
        QStringList order;
        QStringList visible;
        QList<int> sizes;
        QString current;
        bool collapsed = true;
        int width = 280;
        QList<ZzContentPlacement> contents;

        [[nodiscard]] bool operator==(const ZzSideProjection &) const = default;
    };

    /** @brief 保存中央底部面板的目标状态。 */
    struct ZzBottomProjection final
    {
        ZzSubsystemIdentity paneIdentity;
        ZzSubsystemIdentity stackIdentity;
        QStringList order;
        QStringList visible;
        QString current;
        bool collapsed = true;
        int height = 0;
        QList<ZzContentPlacement> contents;

        [[nodiscard]] bool operator==(const ZzBottomProjection &) const = default;
    };

    /** @brief 保存一个 Dock、内容及其实际宿主的完整状态。 */
    struct ZzDockPlacement final
    {
        ZzPanelIdentity panel;
        Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
        bool floating = false;
        bool visible = false;
        ZzSubsystemIdentity actualOwnerIdentity;

        [[nodiscard]] bool operator==(
            const ZzDockPlacement &) const = default;
    };

    /** @brief 保存原生 Dock 的可序列化状态和运行时身份。 */
    struct ZzDockProjection final
    {
        QByteArray state;
        QStringList visible;
        QList<ZzDockPlacement> docks;

        [[nodiscard]] bool operator==(const ZzDockProjection &) const = default;
    };

    /** @brief 保存规范化 Split 树中的一个 leaf 或 branch。 */
    struct ZzSplitNode final
    {
        bool leaf = true;
        QString groupId;
        Qt::Orientation orientation = Qt::Horizontal;
        QList<ZzSplitNode> children;
        QList<int> sizes;
        int currentIndex = -1;

        [[nodiscard]] bool operator==(const ZzSplitNode &) const = default;
    };

    /** @brief 保存 keyed Split 页面在目标组中的稳定位置。 */
    struct ZzSplitSavedPage final
    {
        QString key;
        QString groupId;
        int order = 0;
        bool current = false;

        [[nodiscard]] bool operator==(
            const ZzSplitSavedPage &) const = default;
    };

    /** @brief 保存 Split 递归树、页面映射与 canonical blob。 */
    struct ZzSplitProjection final
    {
        ZzSplitNode root;
        QString activeGroup;
        QStringList groupOrder;
        QList<ZzSplitSavedPage> savedPages;
        QByteArray canonicalState;

        [[nodiscard]] bool operator==(const ZzSplitProjection &) const = default;
    };

    /** @brief 保存四个 Activity 分组和由 Side 推导的当前项。 */
    struct ZzActivityProjection final
    {
        ZzSubsystemIdentity modelIdentity;
        QStringList leftPrimary;
        QStringList leftSecondary;
        QStringList rightPrimary;
        QStringList rightSecondary;
        QString leftCurrent;
        QString rightCurrent;
        QSet<QString> leftActive;
        QSet<QString> rightActive;

        [[nodiscard]] bool operator==(const ZzActivityProjection &) const = default;
    };

    /** @brief 保存标题策略及其纯值输入。 */
    struct ZzTitleProjection final
    {
        ZzTitleMode mode = ZzTitleMode::Application;
        QString applicationTitle;
        QString customTitle;
        QString hostTitle;
        QString titleBarTitle;

        [[nodiscard]] bool operator==(const ZzTitleProjection &) const = default;
    };

    /** @brief 描述一次布局应用所需的全部目标值。 */
    struct ZzWorkspaceProjection
    {
        QList<ZzPanelIdentity> identities;
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
    };

    /** @brief 保存解码布局中的可选目标和明确的 Side current 请求。 */
    struct ZzLayoutRequest final
    {
        /** @brief 标记请求源自旧版首次迁移还是当前格式解码。 */
        enum class ZzSourceSchema : unsigned char
        {
            VersionOne = 1,
            VersionTwo = 2
        };

        std::optional<ZzWorkspaceProjection> projection;
        QString leftCurrent;
        QString rightCurrent;
        /** @brief 限制仅旧版解码请求可携带首次迁移的标签页索引。 */
        ZzSourceSchema sourceSchema = ZzSourceSchema::VersionTwo;
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
