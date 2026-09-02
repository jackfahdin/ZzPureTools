#include <ZzFluentUI/ZzWorkspacePageId.h>

namespace ZzFluentUI {

namespace {

bool isHex(QChar value) noexcept
{
    return value.isDigit() || (value >= QLatin1Char('a')
                               && value <= QLatin1Char('f'))
        || (value >= QLatin1Char('A') && value <= QLatin1Char('F'));
}

bool isCanonicalUuid(QStringView value) noexcept
{
    const bool hasBraces = value.size() == 38;
    if (value.size() != 36 && !hasBraces) {
        return false;
    }
    if (hasBraces
        && (value.front() != QLatin1Char('{')
            || value.back() != QLatin1Char('}'))) {
        return false;
    }

    const QStringView uuid = hasBraces ? value.sliced(1, 36) : value;
    for (qsizetype index = 0; index < uuid.size(); ++index) {
        const bool isDash = index == 8 || index == 13 || index == 18
            || index == 23;
        if (isDash ? uuid.at(index) != QLatin1Char('-')
                   : !isHex(uuid.at(index))) {
            return false;
        }
    }
    return true;
}

} // namespace

ZzWorkspacePageId::ZzWorkspacePageId(QUuid value) noexcept
    : value_(value)
{
}

ZzWorkspacePageId ZzWorkspacePageId::create()
{
    return ZzWorkspacePageId(QUuid::createUuid());
}

ZzWorkspacePageId ZzWorkspacePageId::fromString(QStringView value)
{
    if (!isCanonicalUuid(value)) {
        return {};
    }
    const QUuid parsed = QUuid::fromString(value);
    return parsed.isNull() ? ZzWorkspacePageId{} : ZzWorkspacePageId(parsed);
}

bool ZzWorkspacePageId::isValid() const noexcept
{
    return !value_.isNull();
}

QString ZzWorkspacePageId::toString() const
{
    return value_.toString(QUuid::WithoutBraces);
}

std::size_t qHash(const ZzWorkspacePageId &id, std::size_t seed) noexcept
{
    return ::qHash(id.value_, seed);
}

} // namespace ZzFluentUI
