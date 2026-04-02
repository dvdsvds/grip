#include "grip/process.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace grip {
    int runCommand(const std::vector<std::string>& args) {
        pid_t pid = fork();
        if(pid == 0) {
            std::vector<char*> argv;
            for(auto& arg : const_cast<std::vector<std::string>&>(args)) {
                argv.push_back(arg.data());
            }
            argv.push_back(nullptr);
            if(execvp(argv[0], argv.data()) == -1) {
                _exit(1);
            }
        } else if(pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if(WIFEXITED(status)) return WEXITSTATUS(status);
        }
        return -1;
    }
}
