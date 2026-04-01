#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace grip {
    struct LockEntry {
        std::string name;
        std::string version;
        std::vector<std::string> dependencies;
    };

    std::vector<LockEntry> readLock(const std::filesystem::path& root);
    void writeLock(const std::filesystem::path& root, const std::vector<LockEntry>& entries);
    bool lockExists(const std::filesystem::path& root);
}
