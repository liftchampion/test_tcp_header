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

class Addr {
  public:
    static inline int getaddrinfo_hostport(const char* hostport_c,
                                           const struct addrinfo* hints,
                                           struct addrinfo** res) noexcept
    {
        char* hostport = strdup(hostport_c);
        if( hostport == NULL )
            return EAI_MEMORY;
        char* port = std::strrchr(hostport, ':');
        if( port != NULL )
            *port++ = '\0';
        struct addrinfo hints2;
        if( hints == NULL ) {
            memset(&hints2, 0, sizeof(hints2));
            hints = &hints2;
        }
        int rc = getaddrinfo(hostport, port, hints, res);
        free(hostport);
        return rc;
    }

    static inline sockaddr_in string_to_inaddr(const std::string& str) noexcept {
        struct addrinfo*    addrinfo;
        struct sockaddr_in  addr;
        int ret = getaddrinfo_hostport(str.data(), nullptr, &addrinfo);
        if (ret != 0) {
            std::cerr << gai_strerror(ret) << std::endl;
            exit(1);
        }
        ::memcpy(&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);
        freeaddrinfo(addrinfo);
        return addr;
    }

    static inline std::string inaddr_to_str(const sockaddr_in* addr) noexcept {
        char hbuf[256];
        char sbuf[256];
        getnameinfo(reinterpret_cast<const sockaddr*>(addr), sizeof(sockaddr_in),
                    hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
        std::string res(hbuf);
        res.push_back(':');
        for (int i = 0; sbuf[i]; ++i) {
            res.push_back(sbuf[i]);
        }
        return res;
    }
};

class TCPServer {
  public:
    explicit inline TCPServer(const std::string& hostport) noexcept
            : addr_(Addr::string_to_inaddr(hostport))
    {}

    ~TCPServer() {
        if (listening_socket_ != -1) { close(listening_socket_); }
        for (const auto& client : clients_) { close(client.socket); }
        std::cout << "Sended data: " << sended_total_ << std::endl;
        std::cout << "Recved data: " << recved_total_ << std::endl;
        uint64_t delta = sended_total_ > recved_total_ ? sended_total_ - recved_total_ : recved_total_ - sended_total_;
        std::cout << "Delta      : " << delta << std::endl;
    }

    inline const sockaddr_in* get_addr() noexcept { return &addr_; }
    inline std::string get_str_addr() noexcept { return Addr::inaddr_to_str(&addr_); }

    inline bool start_listening() noexcept {
        listening_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listening_socket_ == -1) {
            std::cout << "socket err" << std::endl;
        }
        switch_to_nonblocking(listening_socket_);
        if (bind(listening_socket_, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_))) {
            std::cout << "bind err" << std::endl;
            return false;
        }
        if (listen(listening_socket_, listen_backlog_)) {
            std::cout << "listen err" << std::endl;
            return false;
        }
        socklen_t len = sizeof(addr_);
        if (getsockname(listening_socket_, reinterpret_cast<sockaddr*>(&addr_), &len)) {
            std::cout << "getsockname err" << std::endl;
            return false;
        }
        return true;
    }

    inline void end_listening() noexcept {
        close(listening_socket_);
        listening_socket_ = -1;
    }

    inline bool accept_client() noexcept {
        sockaddr_in from;
        socklen_t   fromlen = sizeof(from);
        int client = accept(listening_socket_, reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (client == -1 && errno != EAGAIN) {
            std::cout << "accept err" << std::endl;
            return false;
        }
        switch_to_nonblocking(client);
        if (client == -1) { return false; }
        clients_.emplace_back(from, client);
        if (log_lvl_ >= 0) { std::cout << "Connected client " << clients_.size() << ": " << Addr::inaddr_to_str(&from) << std::endl; }
        return true;
    }

    inline void accept_clients(timeval timeout) noexcept {
        fd_set rfds;
        int select_ret;
        FD_ZERO(&rfds);
        FD_SET(listening_socket_, &rfds);
        while ((select_ret = select(listening_socket_ + 1, &rfds, NULL, NULL, &timeout)) > 0) {
            accept_client();
        }
        if (select_ret == -1 && errno != EINTR) { std::cerr << "Select error" << std::endl; }
    }

    inline int echo(timeval timeout) noexcept {
        // std::cout << "Echo iter started" << std::endl;
        (void)timeout;
        int need_stop = 0;
        fd_set rfds;
        fd_set wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        int max_fd = 0;
        fill_fd_sets_and_find_max_fd(&rfds, &wfds, &max_fd);
        uint8_t buf[384];
        if (clients_.empty()) {return 0;}
        pollfd ufd = {clients_[0].socket, POLLIN | POLLOUT, 0};
        if (poll(&ufd, 1, 1) == 0) {return 0;}
        for (auto c_it = clients_.begin(); c_it != clients_.end();) {
            ssize_t recv_res = 0;
            ssize_t send_res = 0;
            ::bzero(buf, sizeof(buf));
            if (ufd.revents & POLLIN) {
                //std::cout << "Recving" << std::endl;
                recv_res = recv(c_it->socket, buf, sizeof(buf), MSG_NOSIGNAL | MSG_DONTWAIT);
                if (recv_res > 0) { recved_total_ += recv_res; }
                if (recv_res == -1 && errno != ECONNRESET && errno != EAGAIN) { std::cerr << "recv error" << std::endl; }
                if ((errno == ECONNRESET) && recv_res == -1) {
                    close(c_it->socket);
                    std::cout << "Disconnected(1) " << Addr::inaddr_to_str(&c_it->addr) << std::endl;
                    c_it = clients_.erase(c_it);
                    continue;
                }
                if (recv_res == 1 && buf[0] == 'C') {
                    --recved_total_;
                    clients_.clear();
                    return 0;
                }
                if (recv_res != -1 && recv_res != 0) {
                    std::cout << "[" /**<< Time::now()*/ << "] Recved '" << buf << "' len: "
                              << ::strlen(reinterpret_cast<char*>(buf)) << " ret: " << recv_res << std::endl;
                }
                if (recv_res != -1 && false) {
                    for (ssize_t j = 0; j < recv_res; ++j) {
                        if (isprint(buf[j])) { printf("%c", buf[j]); }
                        else { printf("[%d]", buf[j]); }
                    }
                    printf("\n");
                }
                //std::cout << "Recved " << recv_res << " of '" << buf[0] << "' from " << Sys::inaddr_to_str(&c_it->addr)
                //          << std::endl;

                if (strncmp(reinterpret_cast<char*>(buf), "Stop", 4) == 0) { need_stop = 1; }

                if (/*FD_ISSET(c_it->socket, &wfds) && recv_res != -1*/ (ufd.revents & POLLOUT) && recv_res != -1) {
                    //std::cout << "Sending" << std::endl;
                    send_res = send(c_it->socket, buf, recv_res, MSG_NOSIGNAL | MSG_DONTWAIT);
                    if (send_res == -1 && errno != EPIPE && errno != ECONNRESET) { std::cerr << "Send error" << std::endl; }
                    if ((errno == EPIPE || errno == ECONNRESET) && send_res == -1) {
                        close(c_it->socket);
                        std::cout << "Disconnected(2) " << Addr::inaddr_to_str(&c_it->addr) << std::endl;
                        c_it = clients_.erase(c_it);
                        continue;
                    }
                    if (send_res != 0) {
                        std::cout << /**"[" << Time::now() <<*/ "] Sended '" << buf
                                  << "' len: " << ::strlen(reinterpret_cast<char*>(buf)) << " ret: " << send_res << std::endl;
                    }
                    //std::cout << "Sended " << send_res << " of '" << buf[0] << "' to " << Sys::inaddr_to_str(&c_it->addr)
                    //          << std::endl;
                    if (send_res > 0) { sended_total_ += send_res; }
                }
            }
            if (recv_res != -1 && send_res != -1) {
                //   std::cout << "No errors in cycle iter" << std::endl;
                ++c_it;
            }
        }
        if (!clients_.empty()) { std::cout << "Recved total: " << recved_total_ << " Sended total: " << sended_total_ << std::endl; }
        //std::cout << "Echo cycle ended. Need stop: " << need_stop << std::endl;
        return need_stop;
    }

    inline int one_iteration(const timeval& timeout) noexcept {
        static int disconnected = 0;
        std::vector<Client> new_clients;
        //std::cout << "New iter. Clients count: " << clients_.size() << " Disconnected: " << disconnected << std::endl;
        accept_clients(timeout);
        for (auto c_it = clients_.begin(); c_it != clients_.end(); ++c_it) {
            if (!is_socket_open(c_it->socket)) {
                std::cout << "Disconnected(3) " << Addr::inaddr_to_str(&c_it->addr) << std::endl;
                ++disconnected;
            }
            else {
                new_clients.push_back(*c_it);
            }
        }
        clients_ = new_clients;
        return echo(timeout);;
    }

  private:
    struct Client {
        Client(sockaddr_in c_addr, int c_socket) : addr(c_addr), socket(c_socket) {}
        sockaddr_in addr;
        int         socket;
    };

    void send_to_socket(const uint8_t* data, size_t size, int fd) {
        size_t total = 0;
        int    send_ret;
        while(total < size) {
            send_ret = send(fd, data + total, size - total, 0);
            if (send_ret == -1) { std::cerr << "Send error" << std::endl; exit(1);}
            total += send_ret;
        }
    }

    inline void fill_fd_sets_and_find_max_fd(fd_set* rfds, fd_set* wfds, int* max_fd) noexcept {
        for (const auto& c : clients_) {
            if (c.socket != -1) {
                FD_SET(c.socket, rfds);
                FD_SET(c.socket, wfds);
                *max_fd = std::max(*max_fd, c.socket);
            }
        }
    }
    bool is_socket_open(int fd) {
        int error = 0;
        socklen_t len = sizeof(error);
        int retval = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
        struct sockaddr addr;
        len = sizeof(addr);
        int nameret = getpeername(fd, &addr, &len);
        return retval == 0 && error == 0 && nameret == 0 /*&& recv_res == 0*/;
    }

    static inline void switch_to_nonblocking(int fd) {
        if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
            std::cerr << "fcntl error" << std::endl; exit(1);
        }
    }

    sockaddr_in addr_;
    std::vector<Client> clients_;
    uint64_t sended_total_ = 0;
    uint64_t recved_total_ = 0;
    int listening_socket_ = -1;
    const int listen_backlog_ = 20;
    const int log_lvl_ = 0;
};

int main(int ac, char **av) {
    std::string hostport = "0.0.0.0:0";
    if (ac == 2) { hostport = av[1]; }
    TCPServer   server(hostport);
    std::cout << "Requested server at addr: " << server.get_str_addr() << std::endl;

    server.start_listening();

    std::string server_addr = server.get_str_addr();
    std::cout << "Opened server at addr: " << server_addr << std::endl;

    struct timeval timeout = {0, 1000};

    while (server.one_iteration(timeout) == 0) {
    }
    return 0;
}


//#include <iostream>
//#include <cstdlib>
//#include <netinet/in.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <arpa/inet.h>
//#include <netdb.h>
//#include <type_traits>
//#include <unistd.h>
//
//// Checks If a c-style string is an integer
//bool is_int(char *c) {
//    while (*c != '\0')
//    {
//        if (*c < '0' || *c > '9')
//            return false;
//        c++;
//    }
//    return true;
//}
//
//int main(int argc, char **argv) {
//    if (argc != 2 || !is_int(argv[1])) {
//        std::cerr << "[ERROR] Port is not provided via command line parameters!\n";
//        return -1;
//    }
//
//    // Create a socket & get the file descriptor
//    int sock_listener = socket(AF_INET, SOCK_STREAM, 0);
//    // Check If the socket is created
//    if (sock_listener < 0) {
//        std::cerr << "[ERROR] Socket cannot be created!\n";
//        return -2;
//    }
//
//    std::cout << "[INFO] Socket has been created.\n";
//    // Address info to bind socket
//    sockaddr_in server_addr;
//    server_addr.sin_family = AF_INET;
//    server_addr.sin_port = htons(std::atoi(argv[1]));
//    //server_addr.sin_addr.s_addr = INADDR_ANY;
//    // OR
//    inet_pton(AF_INET, "0.0.0.0", &server_addr.sin_addr);
//
//    char buf[INET_ADDRSTRLEN];
//
//    // Bind socket
//    if (bind(sock_listener, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//        std::cerr << "[ERROR] Created socket cannot be binded to ( "
//                  << inet_ntop(AF_INET, &server_addr.sin_addr, buf, INET_ADDRSTRLEN)
//                  << ":" << ntohs(server_addr.sin_port) << ")\n";
//        return -3;
//    }
//
//    std::cout << "[INFO] Sock is binded to ("
//              << inet_ntop(AF_INET, &server_addr.sin_addr, buf, INET_ADDRSTRLEN)
//              << ":" << ntohs(server_addr.sin_port) << ")\n";
//
//
//    // Start listening
//    if (listen(sock_listener, SOMAXCONN) < 0) {
//        std::cerr << "[ERROR] Socket cannot be switched to listen mode!\n";
//        return -4;
//    }
//    std::cout << "[INFO] Socket is listening now.\n";
//
//    // Accept a call
//    sockaddr_in client_addr;
//    socklen_t client_addr_size = sizeof(client_addr);
//    int sock_client;
//    if ((sock_client = accept(sock_listener, (sockaddr*)&client_addr, &client_addr_size)) < 0) {
//        std::cerr << "[ERROR] Connections cannot be accepted for a reason.\n";
//        return -5;
//    }
//
//    std::cout << "[INFO] A connection is accepted now.\n";
//
//    // Close main listener socket
//    close(sock_listener);
//    std::cout << "[INFO] Main listener socket is closed.\n";
//
//    // Get name info
//    char host[NI_MAXHOST];
//    char svc[NI_MAXSERV];
//    if (getnameinfo(
//            (sockaddr*)&client_addr, client_addr_size,
//            host, NI_MAXHOST,
//            svc, NI_MAXSERV, 0) != 0) {
//        std::cout << "[INFO] Client: (" << inet_ntop(AF_INET, &client_addr.sin_addr, buf, INET_ADDRSTRLEN)
//                  << ":" << ntohs(client_addr.sin_port) << ")\n";
//    } else {
//        std::cout << "[INFO] Client: (host: " << host << ", service: " << svc << ")\n";
//    }
//
//
//    char msg_buf[4096];
//    int bytes;
//    // While receiving - display & echo msg
//    while (true) {
//        bytes = recv(sock_client, &msg_buf, 4096, 0);
//        // Check how many bytes recieved
//        // If there is no data, it means server is disconnected
//        if (bytes == 0) {
//            std::cout << "[INFO] Client is disconnected.\n";
//            break;
//        }
//            // If something gone wrong
//        else if (bytes < 0) {
//            std::cerr << "[ERROR] Something went wrong while receiving data!.\n";
//            break;
//        }
//            // If there is some bytes
//        else {
//            // Print message
//            std::cout << "client> " << std::string(msg_buf, 0, bytes) << "\n";
//            // Resend the same message
//            if (send(sock_client, &msg_buf, bytes, 0) < 0) {
//                std::cerr << "[ERROR] Message cannot be send, exiting...\n";
//                break;
//            }
//        }
//
//    }
//
//    // Close client socket
//    close(sock_client);
//    std::cout << "[INFO] Client socket is closed.\n";
//
//    return 0;
//}