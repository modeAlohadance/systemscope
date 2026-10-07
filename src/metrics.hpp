#pragma once
#include <algorithm>
#include <cstdint>
inline double cpuUsage(uint64_t idleBefore, uint64_t totalBefore, uint64_t idleNow,
                       uint64_t totalNow) {
    if (totalNow <= totalBefore || idleNow < idleBefore)
        return 0;
    auto total = totalNow - totalBefore;
    auto idle = idleNow - idleBefore;
    return 100.0 * (total - std::min(total, idle)) / total;
}
inline double gib(uint64_t bytes) {
    return double(bytes) / 1073741824.0;
}
