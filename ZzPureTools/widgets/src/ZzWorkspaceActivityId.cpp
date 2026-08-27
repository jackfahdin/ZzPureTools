#include <ZzPureTools/ZzWorkspaceActivityId.h>

#include <utility>

namespace ZzPureTools {

ZzWorkspaceActivityId::ZzWorkspaceActivityId(QString value)
    : value_(std::move(value).trimmed())
{
}

bool ZzWorkspaceActivityId::isValid() const noexcept
{
    return !value_.isEmpty();
}

const QString &ZzWorkspaceActivityId::value() const noexcept
{
    return value_;
}

size_t qHash(const ZzWorkspaceActivityId &id, size_t seed) noexcept
{
    return ::qHash(id.value(), seed);
}

} // namespace ZzPureTools
