#include "grip/thread_pool.hpp"
#include "grip/compiler.hpp"
#include "grip/deps.hpp"
#include "grip/installer.hpp"
#include <json.hpp>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

namespace grip {
    std::vector<std::string> findModuleIncludes(const fs::path& root) {
        std::vector<std::string> includePaths;
        fs::path modulesPath = root / "grip_modules";

        if(!fs::exists(modulesPath) || !fs::is_directory(modulesPath)) {
            return includePaths;
        }

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

    std::vector<fs::path> findModuleSources(const fs::path& root) {
        std::vector<fs::path> module_sources;
        fs::path sourcePath = root / "grip_modules";
        if(!fs::exists(sourcePath) || !fs::is_directory(sourcePath)) {
            return module_sources;
        }
        
        for(const auto& entry : fs::directory_iterator(sourcePath)) {
            if(entry.is_directory()) {
                for(const auto& verDir : fs::directory_iterator(entry.path())) {
                    fs::path metaPath = verDir.path() / "package.json";
                    if(fs::exists(metaPath)) {
                        std::ifstream file(metaPath);
                        auto json = nlohmann::json::parse(file);
                        std::string sourceDir = json["source_dir"].get<std::string>();
                        std::vector<std::string> excludes;
                        if (json.contains("exclude")) {
                            for (auto& e : json["exclude"]) {
                                excludes.push_back(e.get<std::string>());
                            }
                        }
                        for(const auto& inner : fs::directory_iterator(verDir.path())) {
                            if(inner.is_directory() && inner.path().filename() != "package.json") {
                                fs::path srcDir = inner.path() / sourceDir;
                                if(fs::exists(srcDir)) {
                                    for(const auto& file : fs::directory_iterator(srcDir)) {
                                        std::string fname = file.path().filename().string();
                                        bool skip = false;
                                        for (auto& ex : excludes) {
                                            if (fname == ex) { skip = true; break; }
                                        }
                                        if(!skip && (file.path().extension() == ".cpp" || file.path().extension() == ".cc")) {
                                            module_sources.push_back(file.path());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return module_sources;
    }

    std::vector<std::string> findModuleFlags(const fs::path& root) {
        std::vector<std::string> flags; 
        fs::path flagPath = root / "grip_modules";

        if(!fs::exists(flagPath) || !fs::is_directory(flagPath)) {
            return flags;
        }
        
        for(auto& entry : fs::recursive_directory_iterator(flagPath)) {
            std::cerr << entry.path() << std::endl;
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

    int compile(const ProjectConfig &config, const std::vector<std::filesystem::path> &sources) {
        if(!fs::exists(config.output)) {
            fs::create_directories(config.output);
        }
        std::vector<std::string> args;
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
            
            futures.push_back(pool.submit([&config, &objects, &obj_mtx, file, filename]() -> int {
                    std::vector<std::string> args;

                    args.push_back(config.compiler);
                    args.push_back("-std=" + config.standard);
                    args.push_back("-O" + config.opt_level);
                    if(config.debug_info) {
                        args.push_back("-g");
                    }
                    for(auto& flag : config.profile_flags) {
                        args.push_back(flag);
                    }
                    for(auto& inc : config.include) {
                        args.push_back("-I" + inc);
                    }

                    auto root = findProjectRoot();
                    auto moduleIncs = findModuleIncludes(root);
                    for(auto& inc : moduleIncs) {
                        args.push_back("-I" + inc);
                    }
                    auto moduleFlags = findModuleFlags(root);
                    for(auto& flag : moduleFlags) {
                        args.push_back(flag);
                    }
                    args.push_back("-c");
                    args.push_back(file.string());
                    args.push_back("-o");
                    args.push_back((fs::path(config.output) / filename).string());
                    
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

                        if(WIFEXITED(status)) {
                            if(WEXITSTATUS(status) == 0) {
                                {
                                    std::lock_guard<std::mutex> lock(obj_mtx);
                                    objects.push_back(fs::path(config.output) / filename);
                                }
                                return 0;
                            } else {
                                std::cout << "Build failed" << std::endl;
                                return 1;
                            }
                        }
                    } else {
                        return -1;
                    }
                    return -1;
                })
            );
        }

        for(auto& f : futures) {
            if(f.get() != 0) return 1;
        }

        args.push_back(config.compiler);
        args.push_back("-O" + config.opt_level);
        if(config.debug_info) {
            args.push_back("-g");
        }
        for(auto& flag : config.profile_flags) {
            args.push_back(flag);
        }
        for(auto& obj : objects) {
            args.push_back(obj.string());
        }
        std::string targetDir = config.target.empty() ? "native" : config.target;
        for(auto& gm : fs::recursive_directory_iterator(findProjectRoot() / "grip_modules")) {
            if(gm.path().extension() == ".a" && gm.path().parent_path().filename() == targetDir) {
                args.push_back("-L" + gm.path().parent_path().string());
                args.push_back("-l" + gm.path().stem().string().substr(3));
            }
        }
        args.push_back("-o");
        args.push_back((fs::path(config.output) / config.name).string());

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

            if(WIFEXITED(status)) {
                if(WEXITSTATUS(status) == 0) {
                    std::cout << "Build successful" << std::endl;
                    return 0;
                } else {
                    std::cout << "Build failed" << std::endl;
                    return 1;
                }
            }
        } else {
            return -1;
        }
        
        return -1;
    }
}
