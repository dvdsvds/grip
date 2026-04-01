#include "grip/http_client.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace grip {
    std::string httpGet(const std::string &host, int port, const std::string &path) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock == -1) {
            return "";
        }
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            close(sock);
            return "";
        }

        std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
        send(sock, request.c_str(), request.size(), 0);

        std::string response;
        char buf[4096];
        int n;
        while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
            response.append(buf, n);
        }

        close(sock);
        size_t pos = response.find("\r\n\r\n");
        if (pos != std::string::npos) {
            return response.substr(pos + 4);
        }

        return "";
    }
}
