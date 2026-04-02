#include "grip/thread_pool.hpp"
#include "grip/compiler.hpp"
#include "grip/deps.hpp"
#include "grip/installer.hpp"
#include "grip/process.hpp"
#include <json.hpp>
#include <fstream>
#include <iostream>

namespace grip {
    std::vector<std::string> findModuleIncludes(const fs::path& root) {
        std::vector<std::string> includePaths;
        fs::path modulesPath = root / "grip_modules";
        if(!fs::exists(modulesPath) || !fs::is_directory(modulesPath)) return includePaths;

        for(const auto& entry : fs::directory_iterator(modulesPath)) {
            if(entry.is_directory()) {
                for(const auto& subEntry : fs::recursive_directory_iterator(entry.path())) {
                    if(subEntry.is_directory() && subEntry.path().filename() == "include") {
                        includePaths.push_back(subEntry.path().string());
                    }
                }
            }
        }
        return includePaths;
    }

    std::vector<std::string> findModuleFlags(const fs::path& root) {
        std::vector<std::string> flags;
        fs::path flagPath = root / "grip_modules";
        if(!fs::exists(flagPath) || !fs::is_directory(flagPath)) return flags;

        for(auto& entry : fs::recursive_directory_iterator(flagPath)) {
            if(entry.path().filename() == "package.json") {
                std::ifstream file(entry.path());
                auto json = nlohmann::json::parse(file);
                if(json.contains("flags")) {
                    for(auto& f : json["flags"]) {
                        flags.push_back(f.get<std::string>());
                    }
                }
            }
        }
        return flags;
    }

    static std::vector<std::string> buildCompileArgs(const ProjectConfig& config, const fs::path& root) {
        std::vector<std::string> args;
        args.push_back(config.compiler);
        args.push_back("-std=" + config.standard);
        args.push_back("-O" + config.opt_level);
        if(config.debug_info) args.push_back("-g");
        for(auto& flag : config.profile_flags) args.push_back(flag);
        for(auto& inc : config.include) args.push_back("-I" + inc);
        for(auto& inc : findModuleIncludes(root)) args.push_back("-I" + inc);
        for(auto& flag : findModuleFlags(root)) args.push_back(flag);
        return args;
    }

    static std::vector<std::string> findModuleLibs(const fs::path& root, const std::string& targetDir) {
        std::vector<std::string> libs;
        for(auto& gm : fs::recursive_directory_iterator(root / "grip_modules")) {
            if(gm.path().extension() == ".a" && gm.path().parent_path().filename() == targetDir) {
                libs.push_back("-L" + gm.path().parent_path().string());
                libs.push_back("-l" + gm.path().stem().string().substr(3));
            }
        }
        return libs;
    }

    int compile(const ProjectConfig& config, const std::vector<std::filesystem::path>& sources) {
        if(!fs::exists(config.output)) fs::create_directories(config.output);

        auto root = findProjectRoot();
        std::vector<fs::path> objects;
        ThreadPool pool(static_cast<unsigned int>(std::thread::hardware_concurrency()));
        std::mutex obj_mtx;
        std::vector<std::future<int>> futures;

        for(const auto& file : sources) {
            std::string filename = file.stem().string() + ".o";
            fs::path objPath = fs::path(config.output) / filename;

            if(!needsRecompile(file, objPath, config.include)) {
                std::cout << "Skip: " << file.string() << std::endl;
                objects.push_back(objPath);
                continue;
            }

            futures.push_back(pool.submit([&config, &objects, &obj_mtx, file, filename, root]() -> int {
                auto args = buildCompileArgs(config, root);
                args.push_back("-c");
                args.push_back(file.string());
                args.push_back("-o");
                args.push_back((fs::path(config.output) / filename).string());

                int ret = runCommand(args);
                if(ret == 0) {
                    std::lock_guard<std::mutex> lock(obj_mtx);
                    objects.push_back(fs::path(config.output) / filename);
                } else {
                    std::cerr << "Build failed: " << file.string() << std::endl;
                }
                return ret;
            }));
        }

        for(auto& f : futures) {
            if(f.get() != 0) return 1;
        }

        std::string targetDir = config.target.empty() ? "native" : config.target;
        auto args = buildCompileArgs(config, root);
        for(auto& obj : objects) args.push_back(obj.string());
        for(auto& lib : findModuleLibs(root, targetDir)) args.push_back(lib);
        args.push_back("-o");
        args.push_back((fs::path(config.output) / config.name).string());

        int ret = runCommand(args);
        std::cout << (ret == 0 ? "Build successful" : "Build failed") << std::endl;
        return ret;
    }
}
