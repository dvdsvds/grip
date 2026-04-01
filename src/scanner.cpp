#include "grip/scanner.hpp"
#include <filesystem>

namespace grip {
    std::vector<std::filesystem::path> scanSource(const ProjectConfig& config) {
        std::vector<std::filesystem::path> result;
        for(auto& source : config.sources) {
            size_t pos = source.find("/**");
            std::string src = source.substr(0, pos);
            for(auto& entry : std::filesystem::recursive_directory_iterator(src)) {
                if(entry.path().extension() == ".cpp") {
                    result.push_back(entry.path());
                } 
            }
        }
        return result;
    }
}
