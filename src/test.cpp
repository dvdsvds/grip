#include "grip/test.hpp"
#include "grip/compiler.hpp"
#include "grip/installer.hpp"
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace grip {
    int runTests(const ProjectConfig& config) {
        fs::path testDir = "tests";
        if(!fs::exists(testDir)) {
            std::cerr << "tests/ directory not found" << std::endl;
            return 1;
        }

        std::vector<std::string> projectObjects;
        for(auto& entry : fs::directory_iterator(config.output)) {
            if(entry.path().extension() == ".o" && entry.path().stem() != "main") {
                projectObjects.push_back(entry.path().string());
            }
        }

        auto root = findProjectRoot();
        std::vector<std::string> moduleLibFlags;
        for(auto& gm : fs::recursive_directory_iterator(root / "grip_modules")) {
            if(gm.path().extension() == ".a") {
                moduleLibFlags.push_back("-L" + gm.path().parent_path().string());
                moduleLibFlags.push_back("-l" + gm.path().stem().string().substr(3));
            }
        }

        auto moduleIncs = findModuleIncludes(root);
        auto moduleFlags = findModuleFlags(root);

        std::vector<fs::path> testFiles;
        for(auto& entry : fs::directory_iterator(testDir)) {
            if(entry.path().extension() == ".cpp") {
                testFiles.push_back(entry.path());
            }
        }

        if(testFiles.empty()) {
            std::cout << "No test files found" << std::endl;
            return 0;
        }

        fs::create_directories("build/test");

        int passed = 0;
        int total = testFiles.size();

        for(auto& testFile : testFiles) {
            std::string testName = testFile.stem().string();
            fs::path testBin = fs::path("build/test") / testName;

            std::vector<std::string> args;
            args.push_back(config.compiler);
            args.push_back("-std=" + config.standard);
            for(auto& inc : config.include) {
                args.push_back("-I" + inc);
            }
            for(auto& inc : moduleIncs) {
                args.push_back("-I" + inc);
            }
            for(auto& flag : moduleFlags) {
                args.push_back(flag);
            }
            args.push_back(testFile.string());
            for(auto& obj : projectObjects) {
                args.push_back(obj);
            }
            for(auto& lf : moduleLibFlags) {
                args.push_back(lf);
            }
            args.push_back("-o");
            args.push_back(testBin.string());

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
                if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    std::cout << "[BUILD FAIL] " << testName << std::endl;
                    continue;
                }
            }

            pid = fork();
            if(pid == 0) {
                execl(testBin.c_str(), testBin.c_str(), nullptr);
                _exit(1);
            } else if(pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    std::cout << "[PASS] " << testName << std::endl;
                    passed++;
                } else {
                    std::cout << "[FAIL] " << testName << std::endl;
                }
            }
        }

        std::cout << "Results: " << passed << "/" << total << " passed" << std::endl;
        return (passed == total) ? 0 : 1;
    }
}
