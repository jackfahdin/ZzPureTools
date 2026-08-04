#include "ZzQtSettingsStorePrivate.h"

#include <QtCore/QThread>

namespace ZzCore {

ZzQtSettingsStorePrivate::ZzQtSettingsStorePrivate(
    const QString &filePath)
    : ownerThread(QThread::currentThread())
{
    if (!filePath.trimmed().isEmpty()) {
        settings = std::make_unique<QSettings>(
            filePath, QSettings::IniFormat);
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
