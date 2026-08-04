#include <ZzPureTools/ZzModuleId.h>

#include <utility>

namespace ZzPureTools {

ZzModuleId::ZzModuleId(QString value)
    : value_(std::move(value).trimmed())
{
}

bool ZzModuleId::isValid() const noexcept
{
    return !value_.isEmpty();
}

const QString &ZzModuleId::value() const noexcept
{
    return value_;
}

std::size_t qHash(const ZzModuleId &id, std::size_t seed) noexcept
{
    return ::qHash(id.value(), seed);
}

} // namespace ZzPureTools
