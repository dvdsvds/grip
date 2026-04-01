#pragma once

#include <filesystem>
#include <vector>
#include "config.hpp"
namespace grip {
    std::vector<std::filesystem::path> scanSource(const ProjectConfig& config);
}
