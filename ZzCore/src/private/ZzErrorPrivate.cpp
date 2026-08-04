#include "ZzErrorPrivate.h"

#include <utility>

namespace ZzCore {

ZzErrorPrivate::ZzErrorPrivate(
    ZzErrorCode errorCode,
    QString errorMessage,
    QString errorContext)
    : code(errorCode)
    , technicalMessage(std::move(errorMessage))
    , context(std::move(errorContext))
{
}

} // namespace ZzCore
