#pragma once

#include <cstddef>
#include <vector>

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QUuid>

#include <ZzFluentUI/ZzTabGroupId.h>
#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>
#include <ZzPureTools/ZzWorkspaceWindowFactory.h>

class QWidget;

namespace ZzPureTools {

class ZzApplicationWindow;
class ZzWorkspaceShell;
class ZzWorkspaceWindowCoordinator;

/** @brief 保存协调器登记记录及其销毁观察连接。 */
class ZzWorkspaceWindowCoordinatorPrivate final
{
public:
    struct ZzPageOrigin final
    {
        QUuid windowId;
        ZzFluentUI::ZzTabGroupId group;
        int index = -1;
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
        bool primary = false;
        bool delegatedClosePending = false;
        bool delegatedCloseApproved = false;
        bool closeBypass = false;
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

    ZzWorkspaceWindowCoordinator *const q_ptr;
    std::vector<ZzWindowRecord> records;
    std::vector<ZzPageOrigins> pageOrigins;
    ZzWorkspaceWindowFactory windowFactory;
    bool shuttingDown = false;
    bool reclaiming = false;
    bool handlingCloseEvent = false;
};

} // namespace ZzPureTools
