#include <iostream>

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

#include <cerrno>

#include "addr.h"

#define MAX_ETH_HEADERS    (14/*ETH*/ + 4/*802.1Q*/)
#define MAX_IP_TCP_HEADERS (20/*IP*/ + 20/*TCP*/ + 12/*TCP options*/)

#define ZF_TRY(func_call) \
    { \
       int ret = func_call; \
       if (ret) { \
           std::cout << "ERROR in '" << #func_call << "': " << ret << " [" << strerror(-ret) << "]" << std::endl; \
           return false; \
       } else { \
           std::cout << "Successful call of '" << #func_call << "'" << std::endl; \
       }\
    }


class TcpDirect_and_EfVi {
  public:
    inline bool init(const char* v_interface_name, const char* hw_interface_name, int vlan_id) noexcept
    {
        ZF_TRY(zf_init());
        ZF_TRY(zf_attr_alloc(&attr));
        ZF_TRY(zf_attr_set_from_str(attr, "interface", v_interface_name));
        ZF_TRY(zf_stack_alloc(attr, &stack));
        std::cout << "TCPDirect inited" << std::endl;
        return init_ev_fi(v_interface_name, hw_interface_name, vlan_id);
    }

    inline bool init_ev_fi(const char* v_interface_name, const char* hw_interface_name, int vlan_id) noexcept
    {
        (void)v_interface_name;
        std::cout << "Trying to init ev_fi with interface: '" << hw_interface_name << "'" << std::endl;
        ZF_TRY(ef_driver_open(&driver_handle));

        if (vlan_id == 0) {
            ZF_TRY(ef_pd_alloc_by_name(&pd, driver_handle, hw_interface_name, EF_PD_DEFAULT));
        } else {
            ZF_TRY(ef_pd_alloc_with_vport(&pd, driver_handle, hw_interface_name, EF_PD_DEFAULT, vlan_id));
        }

        int flags = EF_VI_FLAGS_DEFAULT;
        ZF_TRY(ef_vi_alloc_from_pd(&vi, driver_handle, &pd, driver_handle,
                -1, 0, -1, nullptr, -1, static_cast<enum ef_vi_flags >(flags)) < 0);
        ZF_TRY(ef_pio_alloc(&pio, driver_handle, &pd, -1, driver_handle));
        ZF_TRY(ef_pio_link_vi(&pio, driver_handle, &vi, driver_handle));
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
//                    std::cout << "Ub ret " << unbundle_ret << std::endl;
                    if (unbundle_ret) {
                        pio_in_use = false;
//                        std::cout << "Pio in use: false" << std::endl;
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
        printf("Trying to send %d bytes '%*s'\n", msg_actual_len, msg_actual_len, send_buff);
        create_header();
        prepare_data_for_copy_pio();
        ef_vi_transmit_pio_warm(&tcp_direct_and_ef_vi->vi);
        write_with_copy_pio();
//        std::cout << "Pio in use: true" << std::endl;
        tcp_direct_and_ef_vi->pio_in_use = true;
        complete_send();
    }


    inline void fill_msg_with_rand() noexcept
    {
        for (int i = 0; i < msg_actual_len; ++i) { send_buff[i] = rand() % 90 + 33; }
    }

    inline bool create_header() noexcept
    {
        _zfds.headers_size = MAX_ETH_HEADERS + MAX_IP_TCP_HEADERS;
        _zfds.headers = headers_buf;
        if (zf_delegated_send_prepare(_socket, _max_send_size, 40960, 0, &(_zfds)) != ZF_DELEGATED_SEND_RC_OK) {
            std::cout << "zf_delegated_send_prepare err" << std::endl;
            return false;
        }
        return true;
    }

    inline void prepare_data_for_copy_pio()
    {
        memcpy(headers_buf + _zfds.headers_len, send_buff, msg_actual_len);
        zf_delegated_send_tcp_update(&_zfds, msg_declared_len, _push);
        pio_data_len = _zfds.headers_len + msg_actual_len;
    }

    inline void write_with_copy_pio() noexcept
    {
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
        while (zft_shutdown_tx(_socket) == -EAGAIN)
        {
            zf_reactor_perform(tcp_direct_and_ef_vi->stack);
        }
        if (zft_free(_socket))
        {
            std::cout << "zft_free err" << std::endl;
            return false;
        }
        if (zft_handle_free(_socket_handle))
        {
            std::cout << "zft_handle_free err" << std::endl;
            return false;
        }
        _socket = nullptr;
        _socket_handle = nullptr;
        return true;
    }

    TcpDirect_and_EfVi* tcp_direct_and_ef_vi = {};

    const int msg_declared_len;
    const int msg_actual_len;

    static constexpr uint64_t _max_send_size = 400;
    static constexpr int      _push = 0;

    zft_handle* _socket_handle = nullptr;
    zft*        _socket = nullptr;
    zf_ds       _zfds = {};

    uint64_t pio_offset = 0;
    uint64_t pio_data_len = 0;

    int pio_id = 0;

    char headers_buf[MAX_ETH_HEADERS + MAX_IP_TCP_HEADERS + _max_send_size];
    char send_buff[1024] = {};
};



int main(int ac, char** av)
{
    if (ac != 7) {
        //                           1                    2             3          4             5               6
        std::cout << "Usage <VirtualInterfaceName> <HWInterfaceName> <VlanID> <Host:Port> <MsgLenInHeader> <MsgActualLen>" << std::endl;
        std::cout << "If you don't use VLAN set 'VlanID' to 0 and use same 'VirtualInterfaceName' and 'HWInterfaceName'" << std::endl;
        return 0;
    }

    int vlan_id = atoi(av[3]);
    TcpDirect_and_EfVi tcpdirect;
    if (!tcpdirect.init(av[1], av[2], vlan_id)) {
        return 0;
    }
    std::cout << "Ef vi inited" << std::endl;
    int msg_len_in_header = atoi(av[5]);
    int msg_actual_len = atoi(av[6]);
    Zocket zocket(&tcpdirect, msg_len_in_header, msg_actual_len);
    if (!zocket.open(av[4])) {
        return 0;
    }
    std::cout << "Opened" << std::endl;

    for (int i = 0; i < 10; ++i) {
        if (!tcpdirect.pio_in_use) {
            zocket.do_write();
        }
        while (tcpdirect.pio_in_use) {
            tcpdirect.evq_poll();
        }
        sleep(1);
    }

    sleep(1);
    zocket.close();
    tcpdirect.deinit();
    return 0;
}

