#include <iostream>

//class TCPDelegatedDataChannel;
//#include "delegated_data_event_loop.h"
//#include "delegated_data_channel.h"

#include <zf/zf.h>
#include <netinet/tcp.h>
#include <etherfabric/vi.h>
#include <etherfabric/pio.h>
#include <etherfabric/pd.h>

#include <sys/types.h> //*
#include <sys/socket.h> //*
#include <net/if.h> //*
#include <cstring> //*

#include <vector>
#include <zconf.h>
#include <x86intrin.h>

#include <netdb.h>
#include <netinet/in.h>

#include <iostream>

#define MAX_ETH_HEADERS    (14/*ETH*/ + 4/*802.1Q*/)
#define MAX_IP_TCP_HEADERS (20/*IP*/ + 20/*TCP*/ + 12/*TCP options*/)

#define USE_COPY_PIO

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
};


class TcpDirect_and_EfVi {
  public:
    inline bool init(const char* interface_name) noexcept
    {
        if (zf_init()) {
            std::cout << "zf_init err" << std::endl;
            return false;
        }
        if (zf_attr_alloc(&attr)) {
            std::cout << "zf_attr_alloc err" << std::endl;
            return false;
        }
        if (zf_attr_set_from_str(attr, "interface", interface_name)) {
            std::cout << "zf_attr_set_from_str err" << std::endl;
            return false;
        }
        if (zf_stack_alloc(attr, &stack)) {
            std::cout << "zf_stack_alloc err" << std::endl;
            return false;
        }
//        if (zf_muxer_alloc(stack, &muxer)) {
//            std::cout << "zf_muxer_alloc err" << std::endl;
//        }
        return init_ev_fi(interface_name);
    }

    inline bool init_ev_fi(const char* interface_name) noexcept
    {
        std::cout << "Trying to init ev_fi with interface: '" << interface_name << "'" << std::endl;
        if (ef_driver_open(&driver_handle)) {
            std::cout << "ef_driver_open err" << std::endl;
            return false;
        }

        if (ef_pd_alloc_by_name(&pd, driver_handle, interface_name, EF_PD_DEFAULT)) {
            std::cout << "ef_pd_alloc_by_name err" << std::endl;
            return false;
        }
        int flags = EF_VI_FLAGS_DEFAULT;
        if (ef_vi_alloc_from_pd(&vi, driver_handle, &pd, driver_handle, -1, 0, -1, nullptr, -1, static_cast<enum ef_vi_flags >(flags)) < 0) {
            std::cout << "ef_vi_alloc_from_pd err" << std::endl;
            return false;
        }
        if (ef_pio_alloc(&pio, driver_handle, &pd, -1, driver_handle)) {
            std::cout << "ef_pio_alloc err" << std::endl;
            return false;
        }
        if (ef_pio_link_vi(&pio, driver_handle, &vi, driver_handle)) {
            std::cout << "ef_pio_link_vi err" << std::endl;
            return false;
        }
        return true;
    }

    inline void evq_poll() noexcept
    {
        ef_request_id ids[EF_VI_TRANSMIT_BATCH];
        ef_event evs[EF_VI_EVENT_POLL_MIN_EVS];
        int n_ev;
        int unbundle_ret;

        n_ev = ef_eventq_poll(&vi, evs, sizeof(evs) / sizeof(evs[0]));
        for (int i = 0; i < n_ev; ++i) {
            switch (EF_EVENT_TYPE(evs[i])) {
                case EF_EVENT_TYPE_TX:
                    unbundle_ret = ef_vi_transmit_unbundle(&vi, &evs[i], ids);
                    if (unbundle_ret) {
                        pio_in_use = false;
                    }
                    break;
                default:
                    fprintf(stderr, "ERROR: unexpected event \n");
                    return;
                    break;
            }
        }
    }

    inline void deinit() noexcept
    {
        deinit_ev_fi();
//        zf_muxer_free(muxer);
        if (zf_stack_free(stack)) {
            std::cout << "zf_stack_free err" << std::endl;
        }
        zf_attr_free(attr);
        if (zf_deinit()) {
            std::cout << "zf_deinit err" << std::endl;
        }
    }

    inline void deinit_ev_fi() noexcept
    {
        if (ef_pio_unlink_vi(&pio, driver_handle, &vi, driver_handle)) {
            std::cout << "ef_pio_unlink_vi err" << std::endl;
        }
        if (ef_pio_free(&pio, driver_handle)) {
            std::cout << "ef_pio_free err" << std::endl;
        }
        if (ef_vi_free(&vi, driver_handle)) {
            std::cout << "ef_vi_free err" << std::endl;
        }
        if (ef_pd_free(&pd, driver_handle)) {
            std::cout << "ef_pd_free err" << std::endl;
        }
        if (ef_driver_close(driver_handle)) {
            std::cout << "ef_driver_close err" << std::endl;
        }
    }


    zf_attr*         attr = nullptr;
    zf_stack*        stack = nullptr;
//    zf_muxer_set*    _muxer = nullptr;
    ef_pd            pd = {};
    ef_pio           pio = {};
    ef_driver_handle driver_handle = {};
    ef_vi            vi = {};


    bool pio_in_use = false;
};


class Zocket {
  public:

    Zocket(TcpDirect_and_EfVi* tcp_direct_and_ef_vi) : tcp_direct_and_ef_vi(tcp_direct_and_ef_vi) {}

    inline bool open(const std::string &ip_port) noexcept
    {
        sockaddr_in addr = Addr::string_to_inaddr(ip_port);
        return open(&addr);
    }

    inline bool open(const sockaddr_in* addr) noexcept
    {
        if (zft_alloc(tcp_direct_and_ef_vi->stack, tcp_direct_and_ef_vi->attr, &_socket_handle)) {
            std::cout << "zft_alloc err " << std::endl;
            return false;
        }
        if (zft_connect(_socket_handle, (const sockaddr*)addr, sizeof(sockaddr), &_socket)) {
            std::cout << "zft_connect err " << std::endl;
            return false;
        }
        while (zft_state(_socket) == TCP_SYN_SENT) {
            zf_reactor_perform(tcp_direct_and_ef_vi->stack);
        }
        if (zft_state(_socket) != TCP_ESTABLISHED) {
            std::cout << "connection not established" << std::endl;
            return false;
        }
        return true;
    }


    inline bool close() noexcept
    {
        while (zft_shutdown_tx(_socket) == -EAGAIN) {
            zf_reactor_perform(tcp_direct_and_ef_vi->stack);
        }
        if (zft_free(_socket)) {
            std::cout << "zft_free err" << std::endl;
            return false;
        }
        if (zft_handle_free(_socket_handle)) {
            std::cout << "zft_handle_free err" << std::endl;
            return false;
        }
        _socket = nullptr;
        _socket_handle = nullptr;
        return true;
    }

    TcpDirect_and_EfVi* tcp_direct_and_ef_vi = {};



    static constexpr uint64_t _max_send_size = 400;
    static constexpr uint32_t _tcp_flag_psh = 0x8;
    static constexpr int      _push = 1;
    static constexpr int      _tcp_offset_seq_to_flags = 9;

    zft_handle* _socket_handle = nullptr;
    zft*        _socket = nullptr;

    uint64_t _pio_offset = 0;
    uint64_t _pio_data_len = 0;
    uint64_t _max_iov = 1;
    uint64_t _header_timestamp = 0;

    int _pio_id = 0;


    char _headers_buf[MAX_ETH_HEADERS + MAX_IP_TCP_HEADERS + _max_send_size];
    char _send_buff[1024] = {};
    char _recv_buff[1024] = {};

//    typedef struct rx_msg;
//    rx_msg _msg;
//    struct {
//        struct zft_msg msg;
//        struct iovec iov[1];
//    } _msg;

};



int main(int ac, char** av)
{
    if (ac != 3) {
        std::cout << "Usage <InterfaceName> <Host:Port>" << std::endl;
        return 0;
    }
    TcpDirect_and_EfVi tcpdirect;
    if (!tcpdirect.init(av[1])) {
        return 0;
    }
    Zocket zocket(&tcpdirect);
    if (!zocket.open(av[2])) {
        return 0;
    }

    if (!tcpdirect.pio_in_use) {

    }

    zocket.close();


    std::cout << "Hello, World!" << std::endl;
    tcpdirect.deinit();
    return 0;
}