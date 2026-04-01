#include <toml.hpp>
#include <iostream>
#include "grip/config.hpp"

namespace grip {
    ProjectConfig parseToml(const std::filesystem::path& path, const std::string& profile, const std::string& target) {
        ProjectConfig config;
        auto file = toml::parse_file(path.string());

        config.name = file["project"]["name"].value_or("");
        config.version = file["project"]["version"].value_or("");
        config.standard = file["project"]["standard"].value_or("c++17");
        config.compiler = file["project"]["compiler"].value_or("g++");
        if(auto arr = file["build"]["sources"].as_array()) {
            for(auto& elem : *arr) {
                config.sources.push_back(elem.value_or(""));
            }
        } 
        if(auto arr = file["build"]["include"].as_array()) {
            for(auto& elem : *arr) {
                config.include.push_back(elem.value_or(""));
            }
        }
        config.type = file["build"]["type"].value_or("");
        if(auto table = file["dependencies"].as_table()) {
            for(auto& [key, value] : *table) {
                config.dependencies[std::string(key)] = value.as_string()->get();
            }
        }
        config.target = target;
        if(!target.empty()) {
            std::string targetKey = "target." + target;
            if(auto tgt = file["target"][target].as_table()) {
                config.compiler = (*tgt)["compiler"].value_or(config.compiler);
                config.ar = (*tgt)["ar"].value_or(config.ar);
            } else {
                std::cerr << "Target not found: " << target << std::endl;
            }
            config.output = std::string(file["build"]["output"].value_or("")) + "/" + profile + "/" + target;
        } else {
            config.output = std::string(file["build"]["output"].value_or("")) + "/" + profile;
        }
        std::string profileKey = "profile." + profile;
        if(auto prof = file["profile"][profile].as_table()) {
            config.opt_level = (*prof)["opt_level"].value_or("0");
            config.debug_info = (*prof)["debug"].value_or(true);
            if(auto arr = (*prof)["flags"].as_array()) {
                for(auto& elem : *arr) {
                    config.profile_flags.push_back(elem.value_or(""));
                }
            }
        } else {
            if(profile == "release") {
                config.opt_level = "2";
                config.debug_info = false;
            }
        }
        return config;
    }
}
