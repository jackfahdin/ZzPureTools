#include <ZzCore/ZzError.h>

#include <utility>

#include <QtCore/QtGlobal>

#include "private/ZzErrorPrivate.h"

namespace ZzCore {

ZzError::ZzError()
    : d_ptr(std::make_unique<ZzErrorPrivate>())
{
}

ZzError::ZzError(
    ZzErrorCode code,
    QString technicalMessage,
    QString context)
    : d_ptr(std::make_unique<ZzErrorPrivate>(
          code,
          std::move(technicalMessage),
          std::move(context)))
{
    Q_ASSERT(code != ZzErrorCode::None || d_ptr->technicalMessage.isEmpty());
}

ZzError::ZzError(const ZzError &other)
    : d_ptr(other.d_ptr
              ? std::make_unique<ZzErrorPrivate>(*other.d_ptr)
              : std::make_unique<ZzErrorPrivate>())
{
}

ZzError::ZzError(ZzError &&other) noexcept = default;

ZzError &ZzError::operator=(const ZzError &other)
{
    if (this != &other) {
        ZzError copy(other);
        d_ptr.swap(copy.d_ptr);
    }
    return *this;
}

ZzError &ZzError::operator=(ZzError &&other) noexcept = default;

ZzError::~ZzError() = default;

bool ZzError::isError() const noexcept
{
    return code() != ZzErrorCode::None;
}

ZzErrorCode ZzError::code() const noexcept
{
    return d_ptr ? d_ptr->code : ZzErrorCode::None;
}

QString ZzError::technicalMessage() const
{
    return d_ptr ? d_ptr->technicalMessage : QString{};
}

QString ZzError::context() const
{
    return d_ptr ? d_ptr->context : QString{};
}

} // namespace ZzCore
