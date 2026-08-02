#include <cstdio>

#include "ZzLog/ZzLog.h"

int main() {
    zz::log::Config config;
    const auto result = zz::log::initialize(config);
    if (!result) {
        std::fprintf(stderr, "ZzLog initialization failed: %s\n", result.message.c_str());
        return 1;
    }

    zz::log::info("ZzLog is ready, value={}", 42);
    zz::log::shutdown();
    return 0;
}
