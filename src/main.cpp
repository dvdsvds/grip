#include <filesystem>
#include <fstream>
#include <iostream>
#include "grip/compiler.hpp"
#include "grip/config.hpp"
#include "grip/scanner.hpp"
#include "grip/installer.hpp"
#include "grip/test.hpp"
#include "grip/http_client.hpp"
#include "json.hpp"

namespace fs = std::filesystem;

struct CLIOptions {
    std::string command;
    std::string profile = "debug";
    std::string target;
    std::string arg;
};

static CLIOptions parseArgs(int argc, char* argv[]) {
    CLIOptions opts;
    if(argc >= 2) opts.command = argv[1];
    for(int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if(a == "--release") {
            opts.profile = "release";
        } else if(a == "--target" && i + 1 < argc) {
            opts.target = argv[++i];
        } else if(opts.arg.empty()) {
            opts.arg = a;
        }
    }
    return opts;
}

static void installDeps(const grip::ProjectConfig& config, const fs::path& root) {
    if(grip::lockExists(root)) {
        auto locked = grip::readLock(root);
        std::vector<grip::LockEntry> dummy;
        for(auto& entry : locked) {
            grip::install("grip.ynetcpp.dev", 8443, entry.name + "@" + entry.version, dummy, config);
        }
    } else {
        std::vector<grip::LockEntry> lockEntries;
        for(auto& [name, version] : config.dependencies) {
            grip::install("grip.ynetcpp.dev", 8443, name + "@" + version, lockEntries, config);
        }
        grip::writeLock(root, lockEntries);
    }
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cout << "Usage: grip <command>\n"
                  << "Commands: new, build, run, clean, install, test" << std::endl;
        return 1;
    }

    auto opts = parseArgs(argc, argv);

    if(opts.command == "new") {
        if(opts.arg.empty()) {
            std::cerr << "Usage: grip new <project_name>" << std::endl;
            return 1;
        }
        fs::path projectRoot = opts.arg;
        try {
            for(auto& dir : {"src", "include", "build"}) {
                fs::create_directories(projectRoot / dir);
            }
            std::ofstream toml(projectRoot / "grip.toml");
            toml << "[project]\n"
                 << "name = \"" << opts.arg << "\"\n"
                 << "version = \"0.1.0\"\n"
                 << "standard = \"c++20\"\n"
                 << "compiler = \"g++\"\n\n"
                 << "[build]\n"
                 << "sources = [\"src\"]\n"
                 << "include = [\"include\"]\n"
                 << "output = \"build\"\n"
                 << "type = \"bin\"\n";

            std::ofstream main(projectRoot / "src" / "main.cpp");
            main << "#include <iostream>\n\n"
                 << "int main() {\n"
                 << "    std::cout << \"Hello, Grip World!\" << std::endl;\n"
                 << "    return 0;\n"
                 << "}\n";

            std::cout << "Created project: " << opts.arg << std::endl;
        } catch(const fs::filesystem_error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    } else if(opts.command == "build") {
        auto config = grip::parseToml("grip.toml", opts.profile, opts.target);
        auto root = grip::findProjectRoot();
        installDeps(config, root);
        return grip::compile(config, grip::scanSource(config));

    } else if(opts.command == "run") {
        auto config = grip::parseToml("grip.toml", opts.profile, opts.target);
        auto root = grip::findProjectRoot();
        installDeps(config, root);
        if(grip::compile(config, grip::scanSource(config)) == 0) {
            return system((fs::path(config.output) / config.name).string().c_str());
        }
        return 1;

    } else if(opts.command == "clean") {
        auto config = grip::parseToml("grip.toml");
        fs::path buildDir = fs::path(config.output).parent_path();
        if(buildDir.empty()) buildDir = config.output;
        fs::remove_all(buildDir);
        std::cout << "Clean [" << buildDir.string() << "] successful" << std::endl; 

    } else if(opts.command == "install") {
        if(opts.arg.empty()) {
            std::cerr << "Usage: grip install <package>" << std::endl;
            return 1;
        }
        auto config = grip::parseToml("grip.toml");
        auto root = grip::findProjectRoot();
        std::vector<grip::LockEntry> lockEntries;
        grip::install("grip.ynetcpp.dev", 8443, opts.arg, lockEntries, config);
        grip::writeLock(root, lockEntries); 

    } else if(opts.command == "test") {
        auto config = grip::parseToml("grip.toml", opts.profile, opts.target);
        auto root = grip::findProjectRoot();
        installDeps(config, root);
        grip::compile(config, grip::scanSource(config));
        return grip::runTests(config);

    } else if(opts.command == "search") {
        std::string response = grip::httpGet("grip.ynetcpp.dev", 8443, "/packages");
        if(response.empty()) {
            std::cerr << "Failed to fetch package list" << std::endl;
            return 1;
        }
        auto json = nlohmann::json::parse(response);
        for(auto& pkg : json["packages"]) {
            std::cout << pkg.get<std::string>() << std::endl;
        }
    }
    else {
        std::cerr << "Unknown command: " << opts.command << "\n"
                  << "Commands: new, build, run, clean, install, test" << std::endl;
        return 1;
    }

    return 0;
}
