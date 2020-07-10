//// Copyright 2019 Gleb Titov

#include <string>
#include <iostream>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/poll.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>

#include "addr.h"


#define BUF_SIZE 1024

int main(int ac, char **av) {

    if (ac != 2) {
        std::cout << "usage: ./server IP:PORT" << std::endl;
        return 0;
    }

    std::string hostport = av[1];

    sockaddr_in serv_addr = Addr::string_to_inaddr(hostport);

    std::cout << "Requested server at addr " << Addr::inaddr_to_str(&serv_addr) << std::endl;

    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket == -1) {
        std::cout << "socket err" << std::endl;
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    if (bind(listening_socket, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr))) {
        std::cout << "bind err" << std::endl;
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    if (listen(listening_socket, 20)) {
        std::cout << "listen err" << std::endl;
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    socklen_t len = sizeof(serv_addr);
    if (getsockname(listening_socket, reinterpret_cast<sockaddr*>(&serv_addr), &len)) {
        std::cout << "getsockname err" << std::endl;
        std::cout << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Opened server at addr " << Addr::inaddr_to_str(&serv_addr) << std::endl;

    int client = -1;
    while (1) {
        if (client == -1) {
            sockaddr_in from;
            socklen_t   fromlen = sizeof(from);
            client = accept(listening_socket, reinterpret_cast<sockaddr*>(&from), &fromlen);
            if (client == -1 && errno != EAGAIN) {
                std::cout << "accept err" << std::endl;
                std::cout << strerror(errno) << std::endl;
                return 1;
            }
            if (client == -1) { return 1; }
            std::cout << "Connected client " << client << ": " << Addr::inaddr_to_str(&from) << std::endl;
        } else {
            char recv_buf[1024] = {};
            ssize_t recv_ret = 1;
            while (recv_ret && client != -1) {
//        recv_ret = recv(listening_socket, recv_buf, 1024, 0);
                recv_ret = recv(client, recv_buf, 1024, MSG_NOSIGNAL);
                if (recv_ret == -1) {
                    if (errno == ECONNRESET) {
                        std::cout << "CLIENT DISCONNECTED" << std::endl;
                        errno = 0;
                        client = -1;
                    } else {
                        std::cout << "Recv err [" << errno << ']' << std::endl;
                        std::cout << strerror(errno) << std::endl;
                        return 1;
                    }
                } else {
                    printf("Recved %ld bytes: '%.*s'\n", recv_ret, (int)recv_ret, recv_buf);
                }
            }
        }
    }

    return 0;
}


