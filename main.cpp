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


enum ef_vi_capability {
    /** Hardware capable of PIO */
    EF_VI_CAP_PIO = 0,
    /** PIO buffer size supplied to each VI */
    EF_VI_CAP_PIO_BUFFER_SIZE,
    /** Total number of PIO buffers */
    EF_VI_CAP_PIO_BUFFER_COUNT,

    /** Can packets be looped back by hardware */
    EF_VI_CAP_HW_MULTICAST_LOOPBACK,
    /** Can mcast be delivered to many VIs */
    EF_VI_CAP_HW_MULTICAST_REPLICATION,

    /** Hardware timestamping of received packets */
    EF_VI_CAP_HW_RX_TIMESTAMPING,
    /** Hardware timestamping of transmitted packets */
    EF_VI_CAP_HW_TX_TIMESTAMPING,

    /** Is firmware capable of packed stream mode */
    EF_VI_CAP_PACKED_STREAM,
    /** Packed stream buffer sizes supported in kB, bitmask */
    EF_VI_CAP_PACKED_STREAM_BUFFER_SIZES,
    /** NIC switching, ef_pd_alloc_with_vport */
    EF_VI_CAP_VPORTS,

    /** Is physical addressing mode supported? */
    EF_VI_CAP_PHYS_MODE,
    /** Is buffer addressing mode (NIC IOMMU) supported */
    EF_VI_CAP_BUFFER_MODE,

    /** Chaining of multicast filters */
    EF_VI_CAP_MULTICAST_FILTER_CHAINING,
    /** Can functions create filters for 'wrong' MAC addr */
    EF_VI_CAP_MAC_SPOOFING,

    /** Filter on local IP + UDP port */
    EF_VI_CAP_RX_FILTER_TYPE_UDP_LOCAL,
    /** Filter on local IP + TCP port */
    EF_VI_CAP_RX_FILTER_TYPE_TCP_LOCAL,
    /** Filter on local and remote IP + UDP port */
    EF_VI_CAP_RX_FILTER_TYPE_UDP_FULL,
    /** Filter on local and remote IP + TCP port */
    EF_VI_CAP_RX_FILTER_TYPE_TCP_FULL,
    /** Filter on any of above four types with addition of VLAN */
    EF_VI_CAP_RX_FILTER_TYPE_IP_VLAN,

    /** Filter on local IP + UDP port */
    EF_VI_CAP_RX_FILTER_TYPE_UDP6_LOCAL,
    /** Filter on local IP + TCP port */
    EF_VI_CAP_RX_FILTER_TYPE_TCP6_LOCAL,
    /** Filter on local and remote IP + UDP port */
    EF_VI_CAP_RX_FILTER_TYPE_UDP6_FULL,
    /** Filter on local and remote IP + TCP port */
    EF_VI_CAP_RX_FILTER_TYPE_TCP6_FULL,
    /** Filter on any of above four types with addition of VLAN */
    EF_VI_CAP_RX_FILTER_TYPE_IP6_VLAN,

    /** Filter on local MAC address */
    EF_VI_CAP_RX_FILTER_TYPE_ETH_LOCAL,
    /** Filter on local MAC+VLAN */
    EF_VI_CAP_RX_FILTER_TYPE_ETH_LOCAL_VLAN,

    /** Filter on "all unicast" */
    EF_VI_CAP_RX_FILTER_TYPE_UCAST_ALL,
    /** Filter on "all multicast" */
    EF_VI_CAP_RX_FILTER_TYPE_MCAST_ALL,
    /** Filter on "unicast mismatch" */
    EF_VI_CAP_RX_FILTER_TYPE_UCAST_MISMATCH,
    /** Filter on "multicast mismatch" */
    EF_VI_CAP_RX_FILTER_TYPE_MCAST_MISMATCH,

    /** Availability of RX sniff filters */
    EF_VI_CAP_RX_FILTER_TYPE_SNIFF,
    /** Availability of TX sniff filters */
    EF_VI_CAP_TX_FILTER_TYPE_SNIFF,

    /** Filter on IPv4 protocol */
    EF_VI_CAP_RX_FILTER_IP4_PROTO,
    /** Filter on ethertype */
    EF_VI_CAP_RX_FILTER_ETHERTYPE,

    /** Available RX queue sizes, bitmask */
    EF_VI_CAP_RXQ_SIZES,
    /** Available TX queue sizes, bitmask */
    EF_VI_CAP_TXQ_SIZES,
    /** Available event queue sizes, bitmask */
    EF_VI_CAP_EVQ_SIZES,

    /** Availability of zero length RX packet prefix */
    EF_VI_CAP_ZERO_RX_PREFIX,

    /** Is always enabling TX push supported? */
    EF_VI_CAP_TX_PUSH_ALWAYS,

    /** Availability of NIC pace feature */
    EF_VI_CAP_NIC_PACE,

    /** Availability of RX event merging mode */
    EF_VI_CAP_RX_MERGE,

    /** Availability of TX alternatives */
    EF_VI_CAP_TX_ALTERNATIVES,

    /** Number of TX alternatives vFIFOs */
    EF_VI_CAP_TX_ALTERNATIVES_VFIFOS,

    /** Number of TX alternatives common pool buffers */
    EF_VI_CAP_TX_ALTERNATIVES_CP_BUFFERS,

    /** RX firmware variant */
    EF_VI_CAP_RX_FW_VARIANT,

    /** TX firmware variant */
    EF_VI_CAP_TX_FW_VARIANT,

    /** Availability of CTPIO */
    EF_VI_CAP_CTPIO,

    /** Size of TX alternatives common pool buffers **/
    EF_VI_CAP_TX_ALTERNATIVES_CP_BUFFER_SIZE,

    /** RX queue is configured to force event merging **/
    EF_VI_CAP_RX_FORCE_EVENT_MERGING,

    /** Maximum value of capabilities enumeration */
    EF_VI_CAP_MAX, /* Keep this last */
};


#define EFCH_INTF_VER  "770d9384f653b51b9d53b6d0cbfb7b2b"




#include <etherfabric/base.h>
#include <etherfabric/pd.h>
#include "ef_vi_internal.h"
#include "driver_access.h"
#include "logging.h"

#include <net/if.h>

void ef_vi_set_intf_ver2(char* intf_ver, size_t len)
{
    /* Bodge interface requested to match the one used in
     * openonload-201405-u1.  The interface has changed since then, but in
     * ways that are forward and backward compatible with
     * openonload-201405-u1.  (This is almost true: The exception is addition
     * of EFCH_PD_FLAG_MCAST_LOOP).
     *
     * We check that the current interface is the one expected, because if
     * not then something has changed and compatibility may not have been
     * preserved.
     */
    strncpy(intf_ver, "1518b4f7ec6834a578c7a807736097ce", len);
    /* when built from repo */
    if( strcmp(EFCH_INTF_VER, "770d9384f653b51b9d53b6d0cbfb7b2b") &&
        /* when built from distro */
        strcmp(EFCH_INTF_VER, "5c1c482de0fe41124c3dddbeb0bd5a1a") ) {
        fprintf(stderr, "ef_vi: ERROR: char interface has changed\n");
        abort();
    }
}


static int __ef_pd_alloc2(ef_pd* pd, ef_driver_handle pd_dh,
                         int ifindex, int flags, int vlan_id)
{
    ci_resource_alloc_t ra;
    const char* s;
    int rc;

    if( (s = getenv("EF_VI_PD_FLAGS")) != NULL ) {
        if( ! strcmp(s, "vf") )
            flags = EF_PD_VF;
        else if( ! strcmp(s, "phys") )
            flags = EF_PD_PHYS_MODE;
        else if( ! strcmp(s, "default") )
            flags = 0;
    }

    if( flags & EF_PD_VF )
        flags |= EF_PD_PHYS_MODE;

    memset(&ra, 0, sizeof(ra));
    ef_vi_set_intf_ver2(ra.intf_ver, sizeof(ra.intf_ver));
    ra.ra_type = EFRM_RESOURCE_PD;
    ra.u.pd.in_ifindex = ifindex;
    ra.u.pd.in_flags = 0;
    if( flags & EF_PD_VF )
        ra.u.pd.in_flags |= EFCH_PD_FLAG_VF;
    if( flags & EF_PD_PHYS_MODE )
        ra.u.pd.in_flags |= EFCH_PD_FLAG_PHYS_ADDR;
    if( flags & EF_PD_RX_PACKED_STREAM )
        ra.u.pd.in_flags |= EFCH_PD_FLAG_RX_PACKED_STREAM;
    if( flags & EF_PD_VPORT )
        ra.u.pd.in_flags |= EFCH_PD_FLAG_VPORT;
    if( flags & EF_PD_MCAST_LOOP )
        ra.u.pd.in_flags |= EFCH_PD_FLAG_MCAST_LOOP;
    if( flags & EF_PD_MEMREG_64KiB )
        /* FIXME: We're overloading the packed-stream flag here.  The only
         * effect it has is to force ef_memreg to use at least 64KiB buffer
         * table entries.  Unfortunately this won't work if the adapter is not
         * in packed-stream mode.
         */
        ra.u.pd.in_flags |= EFCH_PD_FLAG_RX_PACKED_STREAM;
    if( flags & EF_PD_IGNORE_BLACKLIST )
        ra.u.pd.in_flags |= EFCH_PD_FLAG_IGNORE_BLACKLIST;
    ra.u.pd.in_vlan_id = vlan_id;

    rc = ci_resource_alloc(pd_dh, &ra);
    if( rc < 0 ) {
        std::cout << "FUCK 1" << std::endl;
//        LOGVV(ef_log("ef_pd_alloc: ci_resource_alloc %d", rc));
        return rc;
    }

    pd->pd_flags = (ef_pd_flags)flags;
    pd->pd_resource_id = ra.out_id.index;

    pd->pd_intf_name = (char*)malloc(IF_NAMESIZE);
    if( pd->pd_intf_name == NULL ) {
        std::cout << "FUCK 2" << std::endl;
//        LOGVV(ef_log("ef_pd_alloc: malloc failed"));
        return -ENOMEM;
    }
    if( if_indextoname(ifindex, pd->pd_intf_name) == NULL ) {
        free(pd->pd_intf_name);
        std::cout << "FUCK 3" << std::endl;
//        ef_log("ef_pd_alloc: warning: if_indextoname failed %d", errno);
        pd->pd_intf_name = NULL;
        /* TODO the above is a work around
         * base interface resides in different namespace
         * allocating PD was allowed nevertheless.
         * we intend to do this for license checking only, but
         * FIXME: pd alloc() should be allowed to be done through
         * upper (MACVLAN/VLAN) interface.
         */
    }

    pd->pd_cluster_name = NULL;
    pd->pd_cluster_sock = -1;
    pd->pd_cluster_dh = 0;
    pd->pd_cluster_viset_resource_id = 0;


    std::cout << "MY RET" << std::endl;
    return 0;
}


int ef_pd_alloc2(ef_pd* pd, ef_driver_handle pd_dh,
                int ifindex, enum ef_pd_flags flags)
{
    return __ef_pd_alloc2(pd, pd_dh, ifindex, flags, -1);
}


int ef_pd_alloc_with_vport2(ef_pd* pd, ef_driver_handle pd_dh,
                           const char* intf_name,
                           enum ef_pd_flags flags, int vlan_id)
{
    int ifindex = if_nametoindex(intf_name);
    if( ifindex == 0 ) {
        std::cout << "FAILED NAMETOINDEX" << std::endl;
        return -errno;
    }
    return __ef_pd_alloc2(pd, pd_dh, ifindex, flags | EF_PD_VPORT, vlan_id);
}






#define MAX_ETH_HEADERS    (14/*ETH*/ + 4/*802.1Q*/)
#define MAX_IP_TCP_HEADERS (20/*IP*/ + 20/*TCP*/ + 12/*TCP options*/)

class TcpDirect_and_EfVi {
  public:
    inline bool init(const char* v_interface_name, const char* hw_interface_name, int vlan_id) noexcept
    {
        if (zf_init()) {
            std::cout << "zf_init err" << std::endl;
            return false;
        }
        if (zf_attr_alloc(&attr)) {
            std::cout << "zf_attr_alloc err" << std::endl;
            return false;
        }
        if (zf_attr_set_from_str(attr, "interface", v_interface_name)) {
            std::cout << "zf_attr_set_from_str err" << std::endl;
            return false;
        }
        int ret = zf_stack_alloc(attr, &stack);
        if (ret) {
            std::cout << "zf_stack_alloc err: " << ret << " [" << strerror(-ret) << "]" << std::endl;
            return false;
        }
        std::cout << "TCPDirect inited" << std::endl;
        return init_ev_fi(v_interface_name, hw_interface_name, vlan_id);
    }

    inline bool init_ev_fi(const char* v_interface_name, const char* hw_interface_name, int vlan_id) noexcept
    {
        std::cout << "Trying to init ev_fi with interface: '" << hw_interface_name << "'" << std::endl;
        if (ef_driver_open(&driver_handle)) {
            std::cout << "ef_driver_open err" << std::endl;
            return false;
        }

        {
            int IFINDEX_TO_CHECK = 1;
            std::cout << "Resolving name of interface with index " << IFINDEX_TO_CHECK << ": ";
            char name_buf[IFNAMSIZ] = {};
            char* if_name = if_indextoname(IFINDEX_TO_CHECK, name_buf);
            if (if_name == nullptr) {
                std::cout << "if_indextoname err: errno: " << errno << " [" << strerror(errno) << "]" << std::endl;
            } else {
                std::cout << "[" << if_name << "]" << std::endl;
            }
        }


        std::cout << "Resolving ifindex of HW-interface [" << hw_interface_name << "]: ";
        {
            unsigned int r = if_nametoindex(hw_interface_name);
            if (r == 0) {
                std::cout << "if_nametoindex err: ret: " << r << ", errno: " << errno << " [" << strerror(errno) << "]" << std::endl;
            } else {
                std::cout << "found ifindex: " << r << std::endl;
            }
        }
        std::cout << "Resolving ifindex of V-interface [" << v_interface_name << "] : ";
        {
            unsigned int r = if_nametoindex(hw_interface_name);
            if (r == 0) {
                std::cout << "if_nametoindex err: ret: " << r << ", errno: " << errno << " [" << strerror(errno) << "]" << std::endl;
            } else {
                std::cout << "found ifindex: " << r << std::endl;
            }
        }


        if (vlan_id == 0) {
            int ret = ef_pd_alloc_by_name(&pd, driver_handle, hw_interface_name, EF_PD_DEFAULT);
            if (ret) {
                std::cout << "ef_pd_alloc_by_name err: " << ret << " [" << strerror(-ret) << "]" << std::endl;
                return false;
            }
        } else {
            errno = 0;
            int ret = ef_pd_alloc_with_vport2(&pd, driver_handle, hw_interface_name, EF_PD_DEFAULT, vlan_id); /// EPROTO
            if (ret) {
                std::cout << "ef_pd_alloc_with_vport err: " << ret << " [" << strerror(-ret) << "]" << std::endl;
                return false;
            }
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
///    int msg_len_in_header = atoi(av[5]);
///    int msg_actual_len = atoi(av[6]);
///    Zocket zocket(&tcpdirect, msg_len_in_header, msg_actual_len);
///    if (!zocket.open(av[4])) {
///        return 0;
///    }
///    std::cout << "Opened" << std::endl;
///
///    for (int i = 0; i < 10; ++i) {
///        if (!tcpdirect.pio_in_use) {
///            zocket.do_write();
///        }
///        while (tcpdirect.pio_in_use) {
///            tcpdirect.evq_poll();
///        }
///        sleep(1);
///    }

    sleep(1);
///    zocket.close();
    tcpdirect.deinit();
    return 0;
}
