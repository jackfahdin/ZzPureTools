#include <concepts>
#include <cstdint>
#include <format>
#include <source_location>
#include <string_view>
#include <type_traits>

#include <ZzLog/ZzLog.h>

static_assert(std::same_as<
    std::underlying_type_t<ZzLog::ZzLogLevel>,
    std::uint8_t>);

static_assert(std::same_as<
    std::underlying_type_t<ZzLog::ZzLogOverflowPolicy>,
    std::uint8_t>);

int main()
{
    ZzLog::ZzLogConfig config;
    config.console.enabled = false;

    const auto result = ZzLog::initialize(config);
    if (!result) {
        return 1;
    }

    ZzLog::write(
        ZzLog::ZzLogLevel::Info,
        std::source_location::current(),
        std::format_string<int>{"value={}"},
        42);
    ZzLog::shutdown();
    return 0;
}
