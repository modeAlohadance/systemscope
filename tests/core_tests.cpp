#include "../src/metrics.hpp"
#include <cmath>
int main() {
    if (std::abs(cpuUsage(20, 100, 40, 200) - 80) > 0.001)
        return 1;
    if (cpuUsage(10, 100, 20, 100) != 0)
        return 2;
    if (cpuUsage(10, 100, 500, 200) != 0)
        return 3;
    if (cpuUsage(20, 100, 10, 200) != 0)
        return 4;
    if (gib(1073741824) != 1)
        return 5;
    return 0;
}
