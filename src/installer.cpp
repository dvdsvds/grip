#include "grip/installer.hpp"
#include "grip/http_client.hpp"
#include "grip/compiler.hpp"
#include "grip/process.hpp"
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <json.hpp>
#include <sys/types.h>

namespace fs = std::filesystem;

namespace grip {
    std::filesystem::path findProjectRoot() {
        auto current = std::filesystem::current_path();
        while(true) {
            if(std::filesystem::exists(current / "grip.toml")) {
                return current;
            }
            auto parent = current.parent_path();
            if(parent == current) break;
            current = parent;
        }
        return "";
    }

    static fs::path findSourceRoot(const fs::path& basePath) {
        fs::path sourceRoot = basePath;
        int count = 0;
        fs::path singleSubDir;
        for(const auto& entry : fs::directory_iterator(basePath)) {
            if(entry.is_directory()) {
                count++;
                singleSubDir = entry.path();
            }
        }
        if(count == 1) sourceRoot = singleSubDir;
        return sourceRoot;
    }

    static bool needsTargetBuild(const fs::path& sourceRoot, const std::string& sourceDir, const std::string& targetDir) {
        if(sourceDir.empty()) return false;
        fs::path targetLib = sourceRoot / "lib" / targetDir;
        if(!fs::exists(targetLib)) return true;
        for(auto& entry : fs::directory_iterator(targetLib)) {
                if(entry.path().extension() == ".a") return false;
        }
        return true;
    }

    static int buildPackage(const std::string& name, const fs::path& sourceRoot, const nlohmann::json& meta,
                            const std::string& targetDir, const fs::path& root, const ProjectConfig& config) {
        std::string include_dir = meta["include_dir"].get<std::string>();
        std::string source_dir = meta["source_dir"].get<std::string>();
        std::vector<std::string> exclude = meta["exclude"].get<std::vector<std::string>>();
        std::vector<std::string> pkg_flags;
        if(meta.contains("flags")) {
            pkg_flags = meta["flags"].get<std::vector<std::string>>();
        }

        if(source_dir.empty()) return 0;

        fs::create_directories(sourceRoot / "obj" / targetDir);
        fs::create_directories(sourceRoot / "lib" / targetDir);

        std::vector<fs::path> sources;
        for(const auto& dir : fs::directory_iterator(sourceRoot / source_dir)) {
            std::string fname = dir.path().filename().string();
            if(std::find(exclude.begin(), exclude.end(), fname) != exclude.end()) continue;
            auto ext = dir.path().extension();
            if(ext == ".cpp" || ext == ".cc" || ext == ".c") {
                sources.push_back(dir.path());
            }
        }

        auto moduleIncs = findModuleIncludes(root);

        for(const auto& source : sources) {
            std::vector<std::string> args;
            args.push_back(config.compiler);
            args.push_back("-c");
            args.push_back("-std=" + config.standard);
            args.push_back("-I" + (sourceRoot / include_dir).string());
            for(const auto& flag : pkg_flags) {
                std::string f = flag;
                size_t pos = f.find("{sourceRoot}");
                if(pos != std::string::npos) {
                    f.replace(pos, 12, sourceRoot.string());
                }
                args.push_back(f);
            }
            for(auto& inc : moduleIncs) args.push_back("-I" + inc);
            args.push_back("-o");
            args.push_back((sourceRoot / "obj" / targetDir / (source.stem().string() + ".o")).string());
            args.push_back(source.string());

            if(runCommand(args) != 0) return 1;
        }

        std::vector<std::string> arArgs;
        arArgs.push_back(config.ar);
        arArgs.push_back("rcs");
        arArgs.push_back((sourceRoot / "lib" / targetDir / ("lib" + name + ".a")).string());
        for(const auto& obj : fs::directory_iterator(sourceRoot / "obj" / targetDir)) {
            if(obj.path().extension() == ".o") {
                arArgs.push_back(obj.path().string());
            }
        }
        return runCommand(arArgs);
    }

    static void addLockEntry(const std::string& name, const std::string& version, const nlohmann::json& meta,
                             std::vector<LockEntry>& lockEntries, const std::string& host, int port, const ProjectConfig& config) {
        LockEntry entry;
        entry.name = name;
        entry.version = version;
        if(meta.contains("dependencies")) {
            for(auto& d : meta["dependencies"]) {
                entry.dependencies.push_back(d.get<std::string>());
            }
        }
        lockEntries.push_back(entry);
        for(auto& dep : entry.dependencies) {
            install(host, port, dep, lockEntries, config);
        }
    }

    void install(const std::string &host, int port, const std::string &package, std::vector<LockEntry>& lockEntries, const ProjectConfig& config) {
        auto root = findProjectRoot();
        if(root.empty()) {
            std::cerr << "grip.toml not found" << std::endl;
            return;
        }

        size_t at = package.find('@');
        std::string name;
        std::string version;
        if(at != std::string::npos) {
            name = package.substr(0, at);
            version = package.substr(at + 1);
        } else {
            name = package;
            std::string response = grip::httpGet(host, port, "/packages/" + name);
            if(response.empty()) {
                std::cerr << "Failed to fetch package: " << name << std::endl;
                return;
            }
            auto pkgjson = nlohmann::json::parse(response);
            version = pkgjson["versions"].back().get<std::string>();
        }

        for(auto& e : lockEntries) {
            if(e.name == name && e.version == version) return;
        }

        std::string targetDir = config.target.empty() ? "native" : config.target;
        fs::path modulePath = root / "grip_modules" / name / version;

        if(fs::exists(modulePath / "package.json")) {
            std::ifstream f(modulePath / "package.json");
            auto meta = nlohmann::json::parse(f);
            std::string source_dir = meta.contains("source_dir") ? meta["source_dir"].get<std::string>() : "";
            fs::path sourceRoot = findSourceRoot(modulePath);

            if(!needsTargetBuild(sourceRoot, source_dir, targetDir)) {
                std::cout << "Already installed: " << name << "@" << version << std::endl;
                addLockEntry(name, version, meta, lockEntries, host, port, config);
                return;
            }

            if(buildPackage(name, sourceRoot, meta, targetDir, root, config) != 0) return;
            std::cout << "Built " << name << "@" << version << " for " << targetDir << std::endl;
            addLockEntry(name, version, meta, lockEntries, host, port, config);
            return;
        }

        std::string meta_data = grip::httpGet(host, port, "/packages/" + name + "/" + version);
        if(meta_data.empty()) {
            std::cerr << "Failed to fetch package: " << name << "@" << version << std::endl;
            return;
        }
        auto meta = nlohmann::json::parse(meta_data);
        std::string url = meta["url"].get<std::string>();

        fs::path cachePath = modulePath;
        fs::create_directories(cachePath);
        std::ofstream metaFile(cachePath / "package.json");
        metaFile << meta_data;
        metaFile.close();

        std::string archive = (cachePath / "archive.tar.gz").string();
        system(("curl -sL " + url + " -o " + archive).c_str());
        system(("tar -xzf " + archive + " -C " + cachePath.string()).c_str());

        fs::path sourceRoot = findSourceRoot(cachePath);
        if(buildPackage(name, sourceRoot, meta, targetDir, root, config) != 0) return;

        std::ofstream flags(root / "compile_flags.txt");
        flags << "-std=c++20\n-Iinclude\n";
        for(auto& inc : grip::findModuleIncludes(root)) flags << "-I" + inc + "\n";
        for(auto& flag : grip::findModuleFlags(root)) flags << flag + "\n";
        flags.close();

        addLockEntry(name, version, meta, lockEntries, host, port, config);
        std::cout << "Installed " << name << "@" << version << std::endl;
    }
}
