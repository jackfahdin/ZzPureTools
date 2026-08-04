#pragma once

#include <memory>

#include <QtCore/QSettings>

class QThread;

namespace ZzCore {

class ZzQtSettingsStorePrivate final
{
public:
    explicit ZzQtSettingsStorePrivate(QString filePath);

    [[nodiscard]] bool isOwnerThread() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;

    QThread *ownerThread = nullptr;
    std::unique_ptr<QSettings> settings;
};

} // namespace ZzCore
