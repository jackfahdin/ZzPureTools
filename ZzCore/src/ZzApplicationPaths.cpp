#include <ZzCore/ZzApplicationPaths.h>

#include <array>
#include <utility>

#include <QtCore/QDir>
#include <QtCore/QtGlobal>

#include <ZzCore/ZzError.h>

#include "private/ZzApplicationPathsPrivate.h"

namespace ZzCore {

ZzApplicationPaths::ZzApplicationPaths(
    QString organizationName,
    QString applicationName)
    : d_ptr(std::make_unique<ZzApplicationPathsPrivate>(
          std::move(organizationName),
          std::move(applicationName)))
{
    Q_ASSERT(d_ptr->valid);
}

ZzApplicationPaths::ZzApplicationPaths(const ZzApplicationPaths &other)
    : d_ptr(std::make_unique<ZzApplicationPathsPrivate>(*other.d_ptr))
{
}

ZzApplicationPaths &ZzApplicationPaths::operator=(
    const ZzApplicationPaths &other)
{
    if (this != &other) {
        ZzApplicationPaths copy(other);
        d_ptr.swap(copy.d_ptr);
    }
    return *this;
}

ZzApplicationPaths::~ZzApplicationPaths() = default;

QString ZzApplicationPaths::configDirectory() const
{
    return d_ptr->valid ? d_ptr->configDirectory : QString{};
}

QString ZzApplicationPaths::dataDirectory() const
{
    return d_ptr->valid ? d_ptr->dataDirectory : QString{};
}

QString ZzApplicationPaths::cacheDirectory() const
{
    return d_ptr->valid ? d_ptr->cacheDirectory : QString{};
}

QString ZzApplicationPaths::logDirectory() const
{
    return d_ptr->valid ? d_ptr->logDirectory : QString{};
}

ZzResult<void> ZzApplicationPaths::ensureDirectories() const
{
    if (!d_ptr->valid) {
        return ZzResult<void>::failure(ZzError(
            ZzErrorCode::InvalidArgument,
            QStringLiteral("invalid organization or application name")));
    }

    const std::array<QString, 4> directories{
        d_ptr->configDirectory,
        d_ptr->dataDirectory,
        d_ptr->cacheDirectory,
        d_ptr->logDirectory};
    for (const auto &directory : directories) {
        if (directory.isEmpty() || !QDir().mkpath(directory)) {
            return ZzResult<void>::failure(ZzError(
                ZzErrorCode::Io,
                QStringLiteral("failed to create application directory"),
                QDir::cleanPath(directory)));
        }
    }
    return ZzResult<void>::success();
}

} // namespace ZzCore
