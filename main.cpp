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
        if (ef_vi_alloc_from_pd(&vi, driver_handle, &pd, driver_handle,
                -1, 0, -1, nullptr, -1, static_cast<enum ef_vi_flags >(flags)) < 0) {
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


    zf_attr*         attr  = nullptr;
    zf_stack*        stack = nullptr;
    ef_pd            pd = {};
    ef_pio           pio = {};
    ef_driver_handle driver_handle = {};
    ef_vi            vi = {};


    bool pio_in_use = false;
};


class Zocket {
  public:

    Zocket(TcpDirect_and_EfVi* tcp_direct_and_ef_vi, int msg_declared_len, int msg_actual_len)
      : tcp_direct_and_ef_vi(tcp_direct_and_ef_vi)
      , msg_declared_len(msg_declared_len)
      , msg_actual_len(msg_actual_len)
    {}

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

    inline void do_write() noexcept
    {
        fill_msg_with_rand();
#ifdef USE_COPY_PIO
        create_header(0, false);
        prepare_data_for_copy_pio();
        ef_vi_transmit_pio_warm(&tcp_direct_and_ef_vi->vi);
        write_with_copy_pio();
#else
        create_header(0, true);
        ef_vi_transmit_pio_warm(&tcp_direct_and_ef_vi->vi);
        write();
#endif
        tcp_direct_and_ef_vi->pio_in_use = true;
        complete_send();
    }


    inline void fill_msg_with_rand() noexcept
    {
        for (size_t i = 0; i < msg_actual_len; ++i) { send_buff[i] = rand() % 90 + 33; }
    }

    inline bool create_header(uint64_t timestamp, bool put_to_pio) noexcept
    {
        _zfds.headers_size = MAX_ETH_HEADERS + MAX_IP_TCP_HEADERS;
        _zfds.headers = headers_buf;
        if (zf_delegated_send_prepare(_socket, _max_send_size, 40960, 0, &(_zfds)) != ZF_DELEGATED_SEND_RC_OK) {
            std::cout << "zf_delegated_send_prepare err" << std::endl;
            return false;
        }
        auto tcp_flags_p = reinterpret_cast<uint8_t*>((size_t)_zfds.headers + _zfds.tcp_seq_offset + _tcp_offset_seq_to_flags);
        if (_push) { *tcp_flags_p |= _tcp_flag_psh; }
        else { *tcp_flags_p &= ~_tcp_flag_psh; }
        if (put_to_pio) {
            if (ef_pio_memcpy(&tcp_direct_and_ef_vi->vi, _zfds.headers, 0, _zfds.headers_size)) {
                std::cout << "ef_pio_memcpy err" << std::endl;
                return false;
            }
//            _header_timestamp = timestamp;
            if (!set_message_size(314)) {
                return false;
            }
        }
        return true;
    }

    inline bool set_message_size(uint16_t size) noexcept
    {
        uint16_t size_to_set = htons(size + _zfds.ip_tcp_hdr_len);
        if (ef_pio_memcpy(&tcp_direct_and_ef_vi->vi, &size_to_set, _zfds.ip_len_offset, sizeof(size_to_set))) {
            std::cout << "ef_pio_memcpy err" << std::endl;
            return false;
        }
        return true;
    }

    inline void prepare_data_for_copy_pio()
    {
        memcpy(headers_buf + _zfds.headers_len, send_buff, msg_actual_len);
        zf_delegated_send_tcp_update(&_zfds, msg_actual_len, _push);
        pio_data_len = _zfds.headers_len + msg_actual_len;
    }

    inline void write_with_copy_pio() noexcept {
        ef_vi_transmit_copy_pio(&tcp_direct_and_ef_vi->vi, pio_offset, headers_buf, pio_data_len, pio_id);
    }

    inline void complete_send() noexcept
    {
        struct iovec iov;
        iov.iov_base = send_buff;
        iov.iov_len = msg_actual_len;
        if (zf_delegated_send_complete(_socket, &iov, 1, 0) < 0) {
            std::cout << "zf_delegated_send_complete err" << std::endl;
        }
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

    int msg_declared_len = 80;
    int msg_actual_len = 80;

    static constexpr uint64_t _max_send_size = 400;
    static constexpr uint32_t _tcp_flag_psh = 0x8;
    static constexpr int      _push = 1;
    static constexpr int      _tcp_offset_seq_to_flags = 9;

    zft_handle* _socket_handle = nullptr;
    zft*        _socket = nullptr;
    zf_ds       _zfds = {};

    uint64_t pio_offset = 0;
    uint64_t pio_data_len = 0;
    uint64_t max_iov = 1;
    uint64_t header_timestamp = 0;

    int pio_id = 0;


    char headers_buf[MAX_ETH_HEADERS + MAX_IP_TCP_HEADERS + _max_send_size];
    char send_buff[1024] = {};
    char recv_buff[1024] = {};

//    typedef struct rx_msg;
//    rx_msg _msg;
//    struct {
//        struct zft_msg msg;
//        struct iovec iov[1];
//    } _msg;

};



int main(int ac, char** av)
{
    if (ac != 5) {
        std::cout << "Usage <InterfaceName> <Host:Port> <MsgLenInheader> <MsgActualLen>" << std::endl;
        return 0;
    }
    TcpDirect_and_EfVi tcpdirect;
    if (!tcpdirect.init(av[1])) {
        return 0;
    }
    int msg_len_in_header = atoi(av[3]);
    int msg_actual_len = atoi(av[4]);
    Zocket zocket(&tcpdirect, msg_len_in_header, msg_actual_len);
    if (!zocket.open(av[2])) {
        return 0;
    }

    if (!tcpdirect.pio_in_use) {
        zocket.do_write();
    }

    zocket.close();


    std::cout << "Hello, World!" << std::endl;
    tcpdirect.deinit();
    return 0;
}