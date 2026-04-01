#pragma once

#include <string>
namespace grip {
    std::string httpGet(const std::string& host, int port, const std::string& path);
}
