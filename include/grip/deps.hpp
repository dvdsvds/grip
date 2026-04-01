#pragma once

#include <filesystem>
#include <string>
#include <vector>
namespace fs = std::filesystem;

namespace grip {
    bool needsRecompile(const fs::path& source, const fs::path& object, const std::vector<std::string>& include_dir);
}
