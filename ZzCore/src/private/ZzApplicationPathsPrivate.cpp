#include "ZzApplicationPathsPrivate.h"

#include <utility>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringView>

namespace ZzCore {
namespace {

bool isValidPathSegment(QStringView segment)
{
    return !segment.isEmpty()
        && segment != QStringView(u".")
        && segment != QStringView(u"..")
        && !segment.contains(u'/')
        && !segment.contains(u'\\')
        && !segment.contains(QDir::separator());
}

QString appendApplicationPath(
    const QString &base,
    const QString &organizationName,
    const QString &applicationName)
{
    if (base.isEmpty()) {
        return {};
    }
    const auto organizationDirectory =
        QDir(base).filePath(organizationName);
    return QDir::cleanPath(
        QDir(organizationDirectory).filePath(applicationName));
}

} // namespace

ZzApplicationPathsPrivate::ZzApplicationPathsPrivate(
    QString organizationName,
    QString applicationName)
{
    organizationName = organizationName.trimmed();
    applicationName = applicationName.trimmed();
    valid = isValidPathSegment(organizationName)
        && isValidPathSegment(applicationName);
    if (!valid) {
        return;
    }

    configDirectory = appendApplicationPath(
        QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation),
        organizationName,
        applicationName);
    dataDirectory = appendApplicationPath(
        QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation),
        organizationName,
        applicationName);
    cacheDirectory = appendApplicationPath(
        QStandardPaths::writableLocation(
            QStandardPaths::GenericCacheLocation),
        organizationName,
        applicationName);
    if (!dataDirectory.isEmpty()) {
        logDirectory = QDir::cleanPath(
            QDir(dataDirectory).filePath(QStringLiteral("logs")));
    }
}

} // namespace ZzCore
