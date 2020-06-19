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

#include <iostream>

#define USE_COPY_PIO

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

};

int main(int ac, char** av)
{
    if (ac != 2) {
        std::cout << "Usage <InterfaceName> <...>" << std::endl;
        return 0;
    }
    TcpDirect_and_EfVi tcpdirect;
    if (!tcpdirect.init(av[1])) {
        return 0;
    }





    std::cout << "Hello, World!" << std::endl;
    tcpdirect.deinit();
    return 0;
}