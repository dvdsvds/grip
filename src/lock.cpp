#include "grip/lock.hpp"
#include <toml.hpp>
#include <fstream>
#include <iostream>

namespace grip {
    bool lockExists(const std::filesystem::path& root) {
        return std::filesystem::exists(root / "grip.lock");
    }

    std::vector<LockEntry> readLock(const std::filesystem::path& root) {
        std::vector<LockEntry> entries;
        auto file = toml::parse_file((root / "grip.lock").string());

        if(auto arr = file["package"].as_array()) {
            for(auto& elem : *arr) {
                auto* tbl = elem.as_table();
                if(!tbl) continue;

                LockEntry entry;
                entry.name = (*tbl)["name"].value_or("");
                entry.version = (*tbl)["version"].value_or("");
                if(auto deps = (*tbl)["dependencies"].as_array()) {
                    for(auto& d : *deps) {
                        entry.dependencies.push_back(d.value_or(""));
                    }
                }
                entries.push_back(entry);
            }
        }
        return entries;
    }

    void writeLock(const std::filesystem::path& root, const std::vector<LockEntry>& entries) {
        std::ofstream out(root / "grip.lock");
        for(auto& entry : entries) {
            out << "[[package]]\n";
            out << "name = \"" << entry.name << "\"\n";
            out << "version = \"" << entry.version << "\"\n";
            if(!entry.dependencies.empty()) {
                out << "dependencies = [";
                for(size_t i = 0; i < entry.dependencies.size(); i++) {
                    out << "\"" << entry.dependencies[i] << "\"";
                    if(i + 1 < entry.dependencies.size()) out << ", ";
                }
                out << "]\n";
            }
            out << "\n";
        }
    }
}
