#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {
std::string environment(const char *name)
{
#if defined(_WIN32)
    char value[64]{};
    const DWORD length = GetEnvironmentVariableA(name, value, 64);
    return length > 0 && length < 64 ? std::string(value, length) : std::string{};
#else
    const char *value = std::getenv(name);
    return value ? value : "";
#endif
}

int runSmoke()
{
    // A Windows GUI-subsystem executable must remain alive long enough to
    // detect a caller that launches it without waiting, holding a stage file.
    std::ofstream locked("smoke.lock");
    locked << "in use" << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::ofstream completed("smoke-completed.txt");
    completed << environment("QT_QPA_PLATFORM");
    return std::atoi(environment("ZZ_SMOKE_FIXTURE_EXIT_CODE").c_str());
}
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return runSmoke();
}
#else
int main()
{
    return runSmoke();
}
#endif
