#include <filesystem>
#include <fstream>
#include <iostream>
#include "grip/compiler.hpp"
#include "grip/config.hpp"
#include "grip/scanner.hpp"
#include "grip/installer.hpp"
#include "grip/test.hpp"

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cout << "Usage: grip <command>" << std::endl;
        return 1;
    } else {
        if(std::string(argv[1]) == "new") {
            if(argc < 3) {
                std::cerr << "Usage: " << argv[0] << " new <project_name>" << std::endl;
                return 1;
            }

            std::filesystem::path projectRoot = argv[2];
            std::vector<std::string> subDirs = {"src", "include", "build"};
            try {
                for(const auto& dir : subDirs) {
                    std::filesystem::create_directories(projectRoot / dir);
                }

                std::ofstream tomlFile(projectRoot / "grip.toml");
                tomlFile << "[project]\n"
                         << "name = \"" << argv[2] << "\"\n" 
                         << "version = \"0.1.0\"\n"
                         << "standard = \"c++20\"\n"
                         << "compiler = \"g++\"\n\n"
                         << "[build]\n"
                         << "sources = [\"src/**/*.cpp\"]\n"
                         << "include = [\"include\"]\n"
                         << "output = \"build\"\n"
                         << "type = \"executable\"\n";

                std::filesystem::path mainCppPath = projectRoot / "src" / "main.cpp";
                std::ofstream mainFile(mainCppPath);
                if(mainFile.is_open()) {
                    mainFile << "#include <iostream>\n\n"
                             << "int main() {\n"
                             << "    std::cout << \"Hello, Grip World!\" << std::endl;\n"
                             << "    return 0;\n"
                             << "}\n";
                    mainFile.close();
                }
                std::cout << "Create directory structure: [" << argv[2] << "]" << std::endl;
            } catch(const std::filesystem::filesystem_error& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }
        } else if(std::string(argv[1]) == "build") {
            std::string profile = "debug";
            std::string target = "";
            for(int i = 2; i < argc; i++) {
                if(std::string(argv[i]) == "--release") {
                    profile = "release";
                } else if(std::string(argv[i]) == "--target" && i + 1 < argc) {
                    target = argv[++i];
                }
            }
            grip::ProjectConfig config = grip::parseToml("grip.toml", profile, target);
            auto root = grip::findProjectRoot();

            if(grip::lockExists(root)) {
                auto locked = grip::readLock(root);
                std::vector<grip::LockEntry> dummy;
                for(auto& entry : locked) {
                    grip::install("127.0.0.1", 8080, entry.name + "@" + entry.version, dummy, config);
                }
            } else {
                std::vector<grip::LockEntry> lockEntries;
                for(auto& [name, version] : config.dependencies) {
                    grip::install("127.0.0.1", 8080, name + "@" + version, lockEntries, config);
                }
                grip::writeLock(root, lockEntries);
            }

            auto source = grip::scanSource(config);
            return grip::compile(config, source);
        } else if(std::string(argv[1]) == "run") {
            grip::ProjectConfig config = grip::parseToml("grip.toml");
            auto source = grip::scanSource(config);
            int result = grip::compile(config, source);
            if(result == 0) {
                system((std::filesystem::path(config.output) / config.name).string().c_str());
            }
            
        } else if(std::string(argv[1]) == "clean") {
            grip::ProjectConfig config = grip::parseToml("grip.toml");
            std::filesystem::remove_all(config.output);
            std::cout << "Clean [" << config.output << "] successful" << std::endl;
        } else if(std::string(argv[1]) == "install") {
            grip::ProjectConfig config = grip::parseToml("grip.toml");
            std::vector<grip::LockEntry> lockEntries;
            grip::install("127.0.0.1", 8080, argv[2], lockEntries, config);
            auto root = grip::findProjectRoot();
            grip::writeLock(root, lockEntries);
        } else if(std::string(argv[1]) == "test") {
            std::string profile = "debug";
            for(int i = 2; i < argc; i++) {
                if(std::string(argv[i]) == "--release") {
                    profile = "release";
                }
            }
            grip::ProjectConfig config = grip::parseToml("grip.toml", profile);
            auto source = grip::scanSource(config);
            auto root = grip::findProjectRoot();
            if(grip::lockExists(root)) {
                auto locked = grip::readLock(root);
                std::vector<grip::LockEntry> dummy;
                for(auto& entry : locked) {
                    grip::install("127.0.0.1", 8080, entry.name + "@" + entry.version, dummy, config);
                }
            } else {
                std::vector<grip::LockEntry> lockEntries;
                for(auto& [name, version] : config.dependencies) {
                    grip::install("127.0.0.1", 8080, name + "@" + version, lockEntries, config);
                }
                grip::writeLock(root, lockEntries);
            }
            grip::compile(config, source);
            return grip::runTests(config);
        }
        else {
            std::cerr << "Invalid command '" << argv[1] << "'" << std::endl;
            std::cout << "Commands: new, build, run, clean, install" << std::endl;
            return 1;
        }
    }
}
