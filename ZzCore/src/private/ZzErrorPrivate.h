#pragma once

#include <QtCore/QString>

#include <ZzCore/ZzErrorCode.h>

namespace ZzCore {

class ZzErrorPrivate final
{
public:
    ZzErrorPrivate() = default;
    ZzErrorPrivate(
        ZzErrorCode errorCode,
        QString errorMessage,
        QString errorContext);

    ZzErrorCode code = ZzErrorCode::None;
    QString technicalMessage;
    QString context;
};

} // namespace ZzCore
