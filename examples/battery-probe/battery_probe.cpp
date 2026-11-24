// Minimal external battery probe example for PeriphHub.
// Build with: make probe-example
// Usage: battery-probe-example.exe --json
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    if (argc < 2 || std::strcmp(argv[1], "--json") != 0) {
        std::fprintf(stderr, "usage: %s --json\n", argc > 0 ? argv[0] : "battery-probe-example");
        return 1;
    }
    std::printf(
        "[{\"key\":\"ext:example:0001\",\"name\":\"Example Device\","
        "\"via\":\"2.4 GHz receiver\",\"kind\":\"mouse\",\"percent\":78,"
        "\"connected\":true,\"asleep\":false,\"charging\":false,"
        "\"level\":\"\",\"detail\":\"2.4G dongle\"}]\n");
    return 0;
}
