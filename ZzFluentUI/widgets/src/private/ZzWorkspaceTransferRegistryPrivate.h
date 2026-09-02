#pragma once

#include <chrono>
#include <functional>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzTabGroupId.h>
#include <ZzFluentUI/ZzWorkspacePageId.h>

class QWidget;

namespace ZzFluentUI {

class ZzSplitWorkspace;
class ZzTabWidget;

/** @brief 应用级私有一次性拖放令牌记录。 */
struct ZzWorkspaceTransferRecordPrivate final
{
    bool reserved = false;
    QPointer<ZzSplitWorkspace> sourceWorkspace;
    QPointer<ZzTabWidget> sourceTabs;
    ZzTabGroupId sourceGroup;
    int sourceIndex = -1;
    ZzWorkspacePageId pageId;
    QPointer<QWidget> page;
    std::chrono::steady_clock::time_point deadline;
};

/** @brief 在 GUI 线程维护跨工作区拖放令牌的应用级私有注册表。 */
class ZZ_FLUENT_UI_EXPORT ZzWorkspaceTransferRegistryPrivate final : public QObject
{
    Q_OBJECT
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    /** @brief 返回当前应用的惰性创建注册表；无应用对象时返回空。 */
    static ZzWorkspaceTransferRegistryPrivate *instance();

    /** @brief 发布带版本和随机 128-bit 令牌的 MIME 载荷。 */
    [[nodiscard]] ZzCore::ZzResult<QByteArray> publish(
        ZzSplitWorkspace *source,
        ZzTabWidget *sourceTabs,
        const ZzTabGroupId &sourceGroup,
        int sourceIndex,
        const ZzWorkspacePageId &pageId,
        QWidget *page);

    /** @brief 检查令牌但不消费，并验证目标线程和来源对象。 */
    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate> inspect(
        const QByteArray &token,
        ZzSplitWorkspace *target = nullptr) noexcept;

    /** @brief 原子移除并返回令牌记录；每个令牌只能成功消费一次。 */
    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate> consume(
        const QByteArray &token,
        ZzSplitWorkspace *target = nullptr) noexcept;
    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate> reserve(const QByteArray &, ZzSplitWorkspace *target = nullptr);
    [[nodiscard]] bool commit(const QByteArray &token) noexcept;
    [[nodiscard]] bool release(const QByteArray &token) noexcept;
    void sourceTabRemoved(ZzTabWidget *tabs, int index, QWidget *page) noexcept;

    /** @brief 立即失效指定来源工作区发布的全部令牌。 */
    void invalidateWorkspace(ZzSplitWorkspace *workspace) noexcept;
    void invalidate(const QByteArray &token) noexcept;

    /** @brief 测试注入单调时钟，避免安全测试等待真实五秒。 */
    static void setClockForTesting(Clock clock);
    /** @brief 清除测试时钟并恢复真实单调时钟。 */
    static void resetClockForTesting();

    /** @brief 返回当前未过期令牌数量，供安全测试检查惰性清理。 */
    [[nodiscard]] qsizetype size() noexcept;

private:
    explicit ZzWorkspaceTransferRegistryPrivate(QObject *parent);
    [[nodiscard]] std::chrono::steady_clock::time_point now() const;
    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate> lookup(
        const QByteArray &token,
        ZzSplitWorkspace *target,
        bool remove);
    bool eventFilter(QObject *watched, QEvent *event) override;

    QHash<QByteArray, ZzWorkspaceTransferRecordPrivate> records_;
    QHash<QByteArray, QMetaObject::Connection> pageConnections_;
    QHash<QByteArray, QMetaObject::Connection> workspaceConnections_;
    static Clock clock_;
};

} // namespace ZzFluentUI
