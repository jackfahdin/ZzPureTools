#pragma once

#include <QtCore/QString>

namespace ZzCore {

class ZzApplicationPathsPrivate final
{
public:
    ZzApplicationPathsPrivate(
        QString organizationName,
        QString applicationName);

    bool valid = false;
    QString configDirectory;
    QString dataDirectory;
    QString cacheDirectory;
    QString logDirectory;
};

} // namespace ZzCore
