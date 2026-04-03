#include "grip/http_client.hpp"
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace grip {
    std::string httpGet(const std::string &host, int port, const std::string &path, bool use_tls) {
        bool tls = use_tls || port == 443 || port == 8443;
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock == -1) {
            return "";
        }

        struct hostent* he = gethostbyname(host.c_str());
        if(!he) {
            close(sock);
            return "";
        }
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
        if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            close(sock);
            return "";
        }

        std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
        std::string response;
        char buf[4096];
        int n;
        if(tls) {
            SSL_library_init();
            SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
            if(!ctx) { close(sock); return ""; }

            SSL_CTX_set_default_verify_paths(ctx);
            SSL* ssl = SSL_new(ctx);
            SSL_set_fd(ssl, sock);
            SSL_set_tlsext_host_name(ssl, host.c_str());

            if(SSL_connect(ssl) <= 0) {
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                close(sock);
                return "";
            }

            SSL_write(ssl, request.c_str(), request.size());
            while((n = SSL_read(ssl, buf, sizeof(buf))) > 0) {
                response.append(buf, n);
            }

            SSL_shutdown(ssl);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
        } else {
            send(sock, request.c_str(), request.size(), 0);
            while((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
                response.append(buf, n);
            }
        }

        close(sock);
        size_t pos = response.find("\r\n\r\n");
        if (pos != std::string::npos) {
            return response.substr(pos + 4);
        }

        return "";
    }
}
