#include "test.h"

#include <cstring>
#include <fstream>

int main() {
    std::ofstream gha_summary("/dev/null");
    if (!gha_summary.is_open()) {
        LOG_ERROR("cannot open /dev/null");
        return 4;
    }
    const char* gh_actions_env = std::getenv("GITHUB_ACTIONS");
    bool is_gha = gh_actions_env != nullptr && strcmp(gh_actions_env, "true") == 0;
    const char* gh_summary_env = std::getenv("GITHUB_STEP_SUMMARY");
    if (is_gha && gh_summary_env != nullptr) {
        gha_summary = std::ofstream(gh_summary_env, std::ios::app);
        if (!gha_summary.is_open()) {
            LOG_ERROR("cannot open {}", gh_summary_env);
            return 4;
        }
    }
    return test::run_tests(gha_summary);
}
