#include "argparse.h"

#include <cstring>
#include <format>

bool is_opt_name(const char* arg) {
    return std::strcmp(arg, "--") != 0 && std::strncmp(arg, "--", 2) == 0;
}

// only support string args
std::map<std::string_view, const char*> parse_opts(int argc, const char* argv[]) {
    std::map<std::string_view, const char*> result;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--") == 0) {
            break;
        }
        if (is_opt_name(argv[i])) {
            if (i + 1 >= argc || is_opt_name(argv[i + 1])) {
                throw ArgParseException(std::format("missing string value for {}", argv[i]));
            }
            result[argv[i]] = argv[i + 1];
        }
    }
    return result;
}
