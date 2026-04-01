#pragma once

#include "grip/lock.hpp"
#include "grip/config.hpp"
#include <string>
#include <filesystem>
#include <vector>
namespace grip {
    std::filesystem::path findProjectRoot();
    std::vector<std::string> findModuleIncludes(const std::filesystem::path& root);
    void install(const std::string& host, int port, const std::string& package, std::vector<LockEntry>& lockEntries, const ProjectConfig& config);
}
