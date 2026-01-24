#include "Utils.hpp"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

size_t getPeakMemoryUsageInBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize;
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
        // macOS: ru_maxrss is in bytes
        return usage.ru_maxrss;
#else
        // Linux: ru_maxrss is in kilobytes
        return (size_t)usage.ru_maxrss * 1024;
#endif
    }
    return 0;
#endif
}
