#include "grip/deps.hpp"
#include <fstream>

namespace grip {
    bool needsRecompile(const fs::path& source, const fs::path& object, const std::vector<std::string>& include_dirs) {
        if(!fs::exists(object)) {
            return true;
        } 

        auto otime = fs::last_write_time(object);
        auto cpptime = fs::last_write_time(source);
        if(cpptime > otime) {
            return true;
        }

        std::ifstream file(source);
        if(!file.is_open()) {
            return false;
        }

        std::string line;
        while(std::getline(file, line)) {
            size_t start_pos = line.find("#include \"");
            if(start_pos != std::string::npos) {
                size_t content_start = start_pos + 10;
                size_t end_pos = line.find("\"", content_start);

                if(end_pos != std::string::npos) {
                    std::string header = line.substr(content_start, end_pos - content_start);
                    for(const auto& dir : include_dirs) {
                        fs::path full_path = fs::path(dir) / header;
                        if(fs::exists(full_path)) {
                            if(fs::last_write_time(full_path) > fs::last_write_time(object)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }

        return false;
    }
}
