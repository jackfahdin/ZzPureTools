#include <ZzPureTools/ZzRouteId.h>

#include <utility>

namespace ZzPureTools {

ZzRouteId::ZzRouteId(QString value)
    : value_(std::move(value).trimmed())
{
}

bool ZzRouteId::isValid() const noexcept
{
    return !value_.isEmpty();
}

const QString &ZzRouteId::value() const noexcept
{
    return value_;
}

std::size_t qHash(const ZzRouteId &id, std::size_t seed) noexcept
{
    return ::qHash(id.value(), seed);
}

} // namespace ZzPureTools
