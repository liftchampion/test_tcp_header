
#pragma once

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
