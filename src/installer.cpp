#include "grip/installer.hpp"
#include "grip/http_client.hpp"
#include "grip/compiler.hpp"
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
            auto versions = pkgjson["versions"];
            version = versions.back().get<std::string>();
        }

        for(auto& e : lockEntries) {
            if(e.name == name && e.version == version) return;
        }

        std::string targetDir = config.target.empty() ? "native" : config.target;
        fs::path modulePath = root / "grip_modules" / name / version;

        if(fs::exists(modulePath / "package.json")) {
            std::ifstream f(modulePath / "package.json");
            auto j = nlohmann::json::parse(f);

            std::string source_dir = j.contains("source_dir") ? j["source_dir"].get<std::string>() : "";

            fs::path sourceRoot = modulePath;
            int cnt = 0;
            fs::path singleSubDir;
            for(const auto& entry : fs::directory_iterator(modulePath)) {
                if(entry.is_directory()) {
                    cnt++;
                    singleSubDir = entry.path();
                }
            }
            if(cnt == 1) sourceRoot = singleSubDir;

            bool needsBuild = false;
            if(!source_dir.empty()) {
                fs::path targetLib = sourceRoot / "lib" / targetDir;
                if(!fs::exists(targetLib)) {
                    needsBuild = true;
                } else {
                    bool found = false;
                    for(auto& entry : fs::directory_iterator(targetLib)) {
                        if(entry.path().extension() == ".a") {
                            found = true;
                            break;
                        }
                    }
                    if(!found) needsBuild = true;
                }
            }

            if(!needsBuild) {
                std::cout << "Already installed: " << name << "@" << version << std::endl;
                LockEntry entry;
                entry.name = name;
                entry.version = version;
                if(j.contains("dependencies")) {
                    for(auto& d : j["dependencies"]) {
                        entry.dependencies.push_back(d.get<std::string>());
                    }
                }
                lockEntries.push_back(entry);
                for(auto& dep : entry.dependencies) {
                    install(host, port, dep, lockEntries, config);
                }
                return;
            }

            std::string include_dir = j["include_dir"].get<std::string>();
            std::vector<std::string> exclude = j["exclude"].get<std::vector<std::string>>();
            std::vector<std::string> pkg_flags;
            if(j.contains("flags")) {
                pkg_flags = j["flags"].get<std::vector<std::string>>();
            }

            fs::create_directories(sourceRoot / "obj" / targetDir);
            fs::create_directories(sourceRoot / "lib" / targetDir);

            std::vector<fs::path> sources;
            for(const auto& dir : fs::directory_iterator(sourceRoot / source_dir)) {
                if(std::find(exclude.begin(), exclude.end(), dir.path().filename().string()) != exclude.end()) {
                    continue;
                }
                if(dir.path().extension() == ".cpp" || dir.path().extension() == ".cc" || dir.path().extension() == ".c") {
                    sources.push_back(dir.path());
                }
            }

            for(const auto& source : sources) {
                std::vector<std::string> args;
                args.push_back(config.compiler);
                args.push_back("-c");
                args.push_back("-std=" + config.standard);
                args.push_back("-I" + (sourceRoot / include_dir).string());
                for(const auto& pkg_flag : pkg_flags) {
                    args.push_back(pkg_flag);
                }
                auto moduleIncs = findModuleIncludes(root);
                for(auto& inc : moduleIncs) {
                    args.push_back("-I" + inc);
                }
                args.push_back("-o");
                args.push_back((sourceRoot / "obj" / targetDir / (source.stem().string() + ".o")).string());
                args.push_back(source.string());

                pid_t pid = fork();
                if(pid == 0) {
                    std::vector<char*> argv;
                    for(auto& arg : args) {
                        argv.push_back(arg.data());
                    }
                    argv.push_back(nullptr);
                    if(execvp(argv[0], argv.data()) == -1) {
                        _exit(1);
                    }
                } else if(pid > 0) {
                    int status;
                    waitpid(pid, &status, 0);
                    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        std::cerr << "Build failed" << std::endl;
                        return;
                    }
                }
            }

            std::vector<std::string> args;
            args.push_back(config.ar);
            args.push_back("rcs");
            args.push_back((sourceRoot / "lib" / targetDir / ("lib" + name + ".a")).string());
            for(const auto& source : fs::directory_iterator(sourceRoot / "obj" / targetDir)) {
                if(source.path().extension() == ".o") {
                    args.push_back(source.path().string());
                }
            }
            pid_t pid = fork();
            if(pid == 0) {
                std::vector<char*> argv;
                for(auto& arg : args) {
                    argv.push_back(arg.data());
                }
                argv.push_back(nullptr);
                if(execvp(argv[0], argv.data()) == -1) {
                    _exit(1);
                }
            } else if(pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                    std::cerr << "Build failed" << std::endl;
                    return;
                }
            }

            std::cout << "Built " << name << "@" << version << " for " << targetDir << std::endl;
            LockEntry entry;
            entry.name = name;
            entry.version = version;
            if(j.contains("dependencies")) {
                for(auto& d : j["dependencies"]) {
                    entry.dependencies.push_back(d.get<std::string>());
                }
            }
            lockEntries.push_back(entry);
            for(auto& dep : entry.dependencies) {
                install(host, port, dep, lockEntries, config);
            }
            return;
        }

        std::string meta_data = grip::httpGet(host, port, "/packages/" + name + "/" + version);
        if(meta_data.empty()) {
            std::cerr << "Failed to fetch package: " << name << "@" << version << std::endl;
            return;
        }
        auto json = nlohmann::json::parse(meta_data);
        std::string url = json["url"].get<std::string>();

        fs::path cachePath = root / "grip_modules" / name / version;
        fs::create_directories(cachePath);
        std::string archive = (cachePath / "archive.tar.gz").string();
        std::ofstream metaFile(cachePath / "package.json");
        metaFile << meta_data;
        metaFile.close();

        system(("curl -L " + url + " -o " + archive).c_str());
        system(("tar -xzf " + archive + " -C " + cachePath.string()).c_str());

        fs::path sourceRoot = cachePath;
        fs::path singleSubDir;
        int count = 0;
        if(fs::exists(cachePath) && fs::is_directory(cachePath)) {
            for(const auto& entry : fs::directory_iterator(cachePath)) {
                if(entry.is_directory()) {
                    count++;
                    singleSubDir = entry.path();
                }
            }

            if(count == 1) {
                sourceRoot = singleSubDir;
            }

            std::string include_dir = json["include_dir"].get<std::string>();
            std::string source_dir = json["source_dir"].get<std::string>();
            std::vector<std::string> exclude = json["exclude"].get<std::vector<std::string>>();
            std::vector<std::string> pkg_flags;
            if(json.contains("flags")) {
                pkg_flags = json["flags"].get<std::vector<std::string>>();
            }

            if(!source_dir.empty()) {
                fs::create_directories(sourceRoot / "obj" / targetDir);
                fs::create_directories(sourceRoot / "lib" / targetDir);

                std::vector<fs::path> sources;
                for(const auto& dir : fs::directory_iterator(sourceRoot / source_dir)) {
                    if(std::find(exclude.begin(), exclude.end(), dir.path().filename().string()) != exclude.end()) {
                        continue;
                    }
                    if(dir.path().extension() == ".cpp" || dir.path().extension() == ".cc" || dir.path().extension() == ".c") {
                        sources.push_back(dir.path());
                    }
                }

                for(const auto& source : sources) {
                    std::vector<std::string> args;
                    args.push_back(config.compiler);
                    args.push_back("-c");
                    args.push_back("-std=" + config.standard);
                    args.push_back("-I" + (sourceRoot / include_dir).string());
                    for(const auto& pkg_flag : pkg_flags) {
                        args.push_back(pkg_flag);
                    }
                    auto moduleIncs = findModuleIncludes(root);
                    for(auto& inc : moduleIncs) {
                        args.push_back("-I" + inc);
                    }
                    args.push_back("-o");
                    args.push_back((sourceRoot / "obj" / targetDir / (source.stem().string() + ".o")).string());
                    args.push_back(source.string());

                    pid_t pid = fork();
                    if(pid == 0) {
                        std::vector<char*> argv;
                        for(auto& arg : args) {
                            argv.push_back(arg.data());
                        }
                        argv.push_back(nullptr);
                        if(execvp(argv[0], argv.data()) == -1) {
                            _exit(1);
                        }
                    } else if(pid > 0) {
                        int status;
                        waitpid(pid, &status, 0);
                        if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                            std::cerr << "Build failed" << std::endl;
                            return;
                        }
                    }
                }

                std::vector<std::string> args;
                args.push_back(config.ar);
                args.push_back("rcs");
                args.push_back((sourceRoot / "lib" / targetDir / ("lib" + name + ".a")).string());
                for(const auto& source : fs::directory_iterator(sourceRoot / "obj" / targetDir)) {
                    if(source.path().extension() == ".o") {
                        args.push_back(source.path().string());
                    }
                }
                pid_t pid = fork();
                if(pid == 0) {
                    std::vector<char*> argv;
                    for(auto& arg : args) {
                        argv.push_back(arg.data());
                    }
                    argv.push_back(nullptr);
                    if(execvp(argv[0], argv.data()) == -1) {
                        _exit(1);
                    }
                } else if(pid > 0) {
                    int status;
                    waitpid(pid, &status, 0);
                    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        std::cerr << "Build failed" << std::endl;
                        return;
                    }
                }
            }
        }

        std::ofstream flags(root / "compile_flags.txt");
        flags << "-std=c++20\n";
        flags << "-Iinclude\n";
        auto moduleIncs = grip::findModuleIncludes(root);
        for(auto& inc : moduleIncs) {
            flags << "-I" + inc + "\n";
        }
        auto moduleFlags = grip::findModuleFlags(root);
        for(auto& flag : moduleFlags) {
            flags << flag + "\n";
        }
        flags.close();

        LockEntry lockEntry;
        lockEntry.name = name;
        lockEntry.version = version;
        if(json.contains("dependencies")) {
            for(auto& d : json["dependencies"]) {
                lockEntry.dependencies.push_back(d.get<std::string>());
            }
        }
        lockEntries.push_back(lockEntry);

        for(auto& dep : json["dependencies"]) {
            install(host, port, dep.get<std::string>(), lockEntries, config);
        }

        std::cout << "Installed " << name << "@" << version << std::endl;
    }
}
