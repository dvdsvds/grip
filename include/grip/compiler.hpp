#pragma once

#include "grip/config.hpp"
#include <filesystem>

namespace fs = std::filesystem;
namespace grip {
    std::vector<std::string> findModuleFlags(const fs::path& root);
    int compile(const ProjectConfig& config, const std::vector<std::filesystem::path>& sources);
}
