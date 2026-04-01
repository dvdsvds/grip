#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace grip {
    struct ProjectConfig {
        std::string name;
        std::string version;
        std::string standard;
        std::string compiler;
        std::vector<std::string> sources;
        std::vector<std::string> include;
        std::string output;
        std::string type;
        std::map<std::string, std::string> dependencies;
        std::string opt_level = "0";
        bool debug_info = true;
        std::vector<std::string> profile_flags;
        std::string ar = "ar";
        std::string target;
    };

    ProjectConfig parseToml(const std::filesystem::path& filepath, const std::string& profile = "debug", const std::string& target = "");
}
