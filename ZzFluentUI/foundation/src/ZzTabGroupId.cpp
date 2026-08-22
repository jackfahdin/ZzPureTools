#include <ZzFluentUI/ZzTabGroupId.h>

#include <utility>

namespace ZzFluentUI {

ZzTabGroupId::ZzTabGroupId(QString value)
    : value_(std::move(value).trimmed())
{
}

bool ZzTabGroupId::isValid() const noexcept
{
    return !value_.isEmpty();
}

const QString &ZzTabGroupId::value() const noexcept
{
    return value_;
}

size_t qHash(const ZzTabGroupId &id, size_t seed) noexcept
{
    return ::qHash(id.value(), seed);
}

} // namespace ZzFluentUI
