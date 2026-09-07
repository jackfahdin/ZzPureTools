#pragma once

#include <cstddef>
#include <vector>

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QStringView>
#include <QtCore/QUuid>

#include <ZzCore/ZzResult.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabGroupId.h>
#include <ZzFluentUI/ZzWorkspacePageId.h>
#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>
#include <ZzPureTools/ZzWorkspaceWindowFactory.h>

class QWidget;
class QElapsedTimer;
class QEvent;

namespace ZzPureTools {

class ZzApplicationWindow;
class ZzWorkspaceShell;
class ZzWorkspaceWindowCoordinator;

/** @brief 保存协调器登记记录及其销毁观察连接。 */
class ZzWorkspaceWindowCoordinatorPrivate final
{
public:
    enum class ZzCloseState
    {
        Idle,
        DelegatePending,
        DelegateApproved,
        Reclaiming,
        DispatchingClose,
        NotifyingClose,
        CloseAccepted
    };

    struct ZzPageOrigin final
    {
        QUuid windowId;
        ZzFluentUI::ZzTabGroupId group;
        int index = -1;
        quint64 sequence = 0;
    };

    struct ZzPageOrigins final
    {
        QPointer<QWidget> page;
        std::vector<ZzPageOrigin> origins;
    };

    /** @brief 描述一个窗口、Shell、配置快照和生命周期连接。 */
    struct ZzWindowRecord final
    {
        QPointer<ZzApplicationWindow> window;
        ZzApplicationWindow *windowIdentity = nullptr;
        QUuid windowId;
        QPointer<ZzWorkspaceShell> shell;
        ZzWorkspaceShell *shellIdentity = nullptr;
        ZzWorkspaceWindowConfiguration configuration;
        QMetaObject::Connection windowDestroyedConnection;
        QMetaObject::Connection shellDestroyedConnection;
        QMetaObject::Connection tearOffConnection;
        QMetaObject::Connection transferConnection;
        QMetaObject::Connection activePageConnection;
        QMetaObject::Connection pageActivityConnection;
        bool primary = false;
        ZzCloseState closeState = ZzCloseState::Idle;
        bool internalCloseDispatch = false;
    };

    /** @brief 由公开协调器创建，并保存其非拥有观察值。 */
    explicit ZzWorkspaceWindowCoordinatorPrivate(
        ZzWorkspaceWindowCoordinator *publicObject);

    /** @brief 删除指定索引记录并解除仍存活对象的事件过滤器。 */
    void removeRecord(std::size_t index) noexcept;

    /** @brief 删除匹配窗口或 Shell 身份的记录。 */
    void removeRecordForObject(const QObject *object) noexcept;

    /** @brief 断开全部连接并清空登记表。 */
    void clear() noexcept;

    void recordTransfer(
        ZzApplicationWindow *targetWindow,
        ZzFluentUI::ZzSplitWorkspace *sourceWorkspace,
        const ZzFluentUI::ZzTabGroupId &sourceGroup,
        int sourceIndex,
        QWidget *page,
        const ZzFluentUI::ZzWorkspacePageId &pageId);

    [[nodiscard]] ZzCore::ZzResult<void> closeWindow(
        ZzApplicationWindow *window,
        bool currentCloseEvent = false);

    [[nodiscard]] ZzCore::ZzResult<void> approveDelegatedClose(
        ZzApplicationWindow *window);

    [[nodiscard]] ZzCore::ZzResult<QByteArray> saveTopology() const;

    [[nodiscard]] ZzCore::ZzResult<void> restoreTopology(
        const QByteArray &state,
        const ZzWorkspacePageResolver &pageResolver);

    bool eventFilter(QObject *watched, QEvent *event);

    [[nodiscard]] bool closeTransactionActive(
        const ZzWindowRecord &record) const noexcept;

    void invalidateCloseRequest(ZzWindowRecord &record) noexcept;

    void writeWindowAudit(
        QStringView event,
        const QUuid &windowId,
        QStringView phase,
        const QElapsedTimer &timer,
        bool success) const noexcept;

    void writePageAudit(
        QStringView event,
        const ZzFluentUI::ZzWorkspacePageId &pageId,
        const QUuid &sourceWindowId,
        const QUuid &targetWindowId,
        QStringView phase,
        const QElapsedTimer &timer,
        bool success) const noexcept;

    /** @brief 写入不包含业务内容的拓扑恢复审计事件。 */
    void writeLayoutAudit(
        const QUuid &operationId,
        QStringView phase,
        const QElapsedTimer &timer,
        bool success) const noexcept;

    ZzWorkspaceWindowCoordinator *const q_ptr;
    std::vector<ZzWindowRecord> records;
    std::vector<ZzPageOrigins> pageOrigins;
    ZzWorkspaceWindowFactory windowFactory;
    quint64 nextTransferSequence = 1;
    bool shuttingDown = false;
    bool lifecycleSignalsSuppressed = false;
};

} // namespace ZzPureTools
