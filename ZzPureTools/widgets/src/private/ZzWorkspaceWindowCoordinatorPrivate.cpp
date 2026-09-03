#include "ZzWorkspaceWindowCoordinatorPrivate.h"

#include <algorithm>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>

namespace ZzPureTools {

ZzWorkspaceWindowCoordinatorPrivate::ZzWorkspaceWindowCoordinatorPrivate(
    ZzWorkspaceWindowCoordinator *publicObject)
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzWorkspaceWindowCoordinatorPrivate::removeRecord(
    std::size_t index) noexcept
{
    if (index >= records.size()) {
        return;
    }

    auto &record = records.at(index);
    QObject::disconnect(record.windowDestroyedConnection);
    QObject::disconnect(record.shellDestroyedConnection);
    QObject::disconnect(record.tearOffConnection);
    if (record.window) {
        record.window->removeEventFilter(q_ptr);
    }
    if (record.shell) {
        record.shell->removeEventFilter(q_ptr);
    }
    records.erase(records.begin() + static_cast<std::ptrdiff_t>(index));
}

void ZzWorkspaceWindowCoordinatorPrivate::removeRecordForObject(
    const QObject *object) noexcept
{
    const auto iterator = std::find_if(
        records.begin(),
        records.end(),
        [object](const ZzWindowRecord &record) {
            return record.windowIdentity == object
                || record.shellIdentity == object;
        });
    if (iterator != records.end()) {
        removeRecord(static_cast<std::size_t>(
            std::distance(records.begin(), iterator)));
    }
}

void ZzWorkspaceWindowCoordinatorPrivate::clear() noexcept
{
    while (!records.empty()) {
        removeRecord(records.size() - 1);
    }
}

} // namespace ZzPureTools
