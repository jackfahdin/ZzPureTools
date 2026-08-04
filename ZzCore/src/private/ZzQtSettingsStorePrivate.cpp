#include "ZzQtSettingsStorePrivate.h"

#include <utility>

#include <QtCore/QThread>

namespace ZzCore {

ZzQtSettingsStorePrivate::ZzQtSettingsStorePrivate(QString filePath)
    : ownerThread(QThread::currentThread())
{
    if (!filePath.trimmed().isEmpty()) {
        settings = std::make_unique<QSettings>(
            std::move(filePath), QSettings::IniFormat);
    }
}

bool ZzQtSettingsStorePrivate::isOwnerThread() const noexcept
{
    return QThread::currentThread() == ownerThread;
}

bool ZzQtSettingsStorePrivate::isValid() const noexcept
{
    return settings != nullptr;
}

} // namespace ZzCore
