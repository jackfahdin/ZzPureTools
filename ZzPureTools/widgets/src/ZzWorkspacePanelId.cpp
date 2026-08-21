#include <ZzPureTools/ZzWorkspacePanelId.h>

#include <utility>

namespace ZzPureTools {

ZzWorkspacePanelId::ZzWorkspacePanelId(QString value)
    : value_(std::move(value).trimmed())
{
}

bool ZzWorkspacePanelId::isValid() const noexcept
{
    return !value_.isEmpty();
}

const QString &ZzWorkspacePanelId::value() const noexcept
{
    return value_;
}

size_t qHash(const ZzWorkspacePanelId &id, size_t seed) noexcept
{
    return ::qHash(id.value(), seed);
}

} // namespace ZzPureTools
