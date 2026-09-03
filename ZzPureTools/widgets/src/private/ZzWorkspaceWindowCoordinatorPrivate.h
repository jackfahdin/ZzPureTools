#pragma once

#include <cstddef>
#include <vector>

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>

#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>
#include <ZzPureTools/ZzWorkspaceWindowFactory.h>

namespace ZzPureTools {

class ZzApplicationWindow;
class ZzWorkspaceShell;
class ZzWorkspaceWindowCoordinator;

/** @brief 保存协调器登记记录及其销毁观察连接。 */
class ZzWorkspaceWindowCoordinatorPrivate final
{
public:
    /** @brief 描述一个窗口、Shell、配置快照和生命周期连接。 */
    struct ZzWindowRecord final
    {
        QPointer<ZzApplicationWindow> window;
        ZzApplicationWindow *windowIdentity = nullptr;
        QPointer<ZzWorkspaceShell> shell;
        ZzWorkspaceShell *shellIdentity = nullptr;
        ZzWorkspaceWindowConfiguration configuration;
        QMetaObject::Connection windowDestroyedConnection;
        QMetaObject::Connection shellDestroyedConnection;
        QMetaObject::Connection tearOffConnection;
        bool primary = false;
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
    ZzWorkspaceWindowFactory windowFactory;
    bool shuttingDown = false;
};

} // namespace ZzPureTools
