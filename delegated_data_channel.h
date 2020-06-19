// Copyright 2019 Gleb Titov

#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <zf/zf.h>
#include <etherfabric/vi.h>
#include <etherfabric/pio.h>
#include <etherfabric/pd.h>
#include <sys/socket.h>
#include <onload/extensions_zc.h>
#include <x86intrin.h>

#include <cstring>

#include <string>
#include <iostream>

#include "delegated_data_event_loop.h"

#define MAX_ETH_HEADERS    (14/*ETH*/ + 4/*802.1Q*/)
#define MAX_IP_TCP_HEADERS (20/*IP*/ + 20/*TCP*/ + 12/*TCP options*/)

class TCPDelegatedDataChannel{
  public:
    explicit inline TCPDelegatedDataChannel(DelegatedEventLoop* event_loop) noexcept
            : _event_loop(event_loop)
    {
        get_params_from_event_loop();
    }

    inline ~TCPDelegatedDataChannel() noexcept { close(); }

    inline uint64_t     get_header_timestamp() const noexcept                 { return _header_timestamp; }
    inline void         set_header_timestamp(uint64_t new_timestamp) noexcept { _header_timestamp = new_timestamp; }
    inline void         set_pio_offset(size_t new_offset) noexcept            { _pio_offset = new_offset; }
    inline void         set_pio_id(size_t new_id) noexcept                    { _pio_id = new_id; }
    inline int          get_pio_id() const noexcept                           { return _pio_id; }
    inline zf_waitable* get_socket_waitable() const noexcept                  { return _waitable_sock; }

    inline bool open(const std::string &ip_port) noexcept
    {
        sockaddr_in addr = Sys::string_to_inaddr(ip_port);
        return open(&addr);
    }

    inline bool open(const sockaddr_in* addr) noexcept
    {
        if (zft_alloc(_stack, _attr, &_socket_handle)) {
            std::cout << "zft_alloc err " << std::endl;
        }
        if (zft_connect(_socket_handle, (const sockaddr*)addr, sizeof(sockaddr), &_socket)) {
            std::cout << "zft_connect err " << std::endl;
        }
        while (zft_state(_socket) == TCP_SYN_SENT) {
            zf_reactor_perform(_stack);
        }
        if (zft_state(_socket) != TCP_ESTABLISHED) {
            std::cout << "connection not established" << std::endl;
            return false;
        }
        _waitable_sock = zft_to_waitable(_socket);
        return true;
    }

    inline bool close() noexcept
    {
        while (zft_shutdown_tx(_socket) == -EAGAIN) {
            zf_reactor_perform(_stack);
        }
        if (zft_free(_socket)) {
            std::cout << "zft_free err" << std::endl;
        }
        if (zft_handle_free(_socket_handle)) {
            std::cout << "zft_handle_free err" << std::endl;
        }
        _socket = nullptr;
        _socket_handle = nullptr;
        _waitable_sock = nullptr;
        return true;
    }

    inline int read_event() noexcept
    {
        _msg.msg.iovcnt = _max_iov;
        zft_zc_recv(_socket, &_msg.msg, 0);
        if (_msg.msg.iovcnt == 0) {
            return 0;
        }
        zft_zc_recv_done(_socket, &_msg.msg);
        return _msg.iov[0].iov_len;
    }

    inline void write_event(char* msg, size_t msg_len) noexcept
    {
        ef_pio_memcpy(_vi, msg, _pio_offset + _zfds.headers_len, msg_len);
        ef_vi_transmit_pio(_vi, _pio_offset, _zfds.headers_len + msg_len, _pio_id);
    }

    inline void prepare_data_for_copy_pio(char* msg, size_t msg_len)
    {
        memcpy(_headers_buf + _zfds.headers_len, msg, msg_len);
        zf_delegated_send_tcp_update(&_zfds, msg_len, _push);
        _pio_data_len = _zfds.headers_len + msg_len;
    }

    inline void write_event_copy_pio() noexcept {
        ef_vi_transmit_copy_pio(_vi, _pio_offset, _headers_buf, _pio_data_len, _pio_id);
    }

    inline void complete_send(char* msg, size_t msg_len) noexcept
    {
        _pio_in_use = true;
        struct iovec iov;
        iov.iov_base = msg;
        iov.iov_len = msg_len;
        if (zf_delegated_send_complete(_socket, &iov, 1, 0) < 0) {
            std::cout << "zf_delegated_send_complete err" << std::endl;
        }
    }

    inline void create_header(uint64_t timestamp, bool put_to_pio) noexcept
    {
        _zfds.headers_size = MAX_ETH_HEADERS + MAX_IP_TCP_HEADERS;
        _zfds.headers = _headers_buf;
        if (zf_delegated_send_prepare(_socket, _max_send_size, 40960, 0, &(_zfds)) != ZF_DELEGATED_SEND_RC_OK) {
            std::cout << "zf_delegated_send_prepare err" << std::endl;
        }
        auto tcp_flags_p = reinterpret_cast<uint8_t*>((size_t)_zfds.headers + _zfds.tcp_seq_offset + _tcp_offset_seq_to_flags);
        if (_push) { *tcp_flags_p |= _tcp_flag_psh; }
        else { *tcp_flags_p &= ~_tcp_flag_psh; }
        if (put_to_pio) {
            if (ef_pio_memcpy(_vi, _zfds.headers, _pio_offset, _zfds.headers_size)) {
                std::cout << "ef_pio_memcpy err" << std::endl;
            }
            _header_timestamp = timestamp;
            set_message_size(314);
        }
    }

    inline void set_message_size(uint16_t size) noexcept
    {
        uint16_t size_to_set = htons(size + _zfds.ip_tcp_hdr_len);
        if (ef_pio_memcpy(_vi, &size_to_set, _pio_offset + _zfds.ip_len_offset, sizeof(size_to_set))) {
            std::cout << "ef_pio_memcpy err" << std::endl;
        }
    }

  protected:

    typedef struct {
        struct zft_msg msg;
        struct iovec iov[1];
    } rx_msg;

    inline void get_params_from_event_loop() noexcept
    {
        _attr = _event_loop->get_attr();
        _stack = _event_loop->get_stack();
        _vi = _event_loop->get_vi();
    }

  protected:
    static constexpr uint64_t _max_send_size = 400;
    static constexpr uint32_t _tcp_flag_psh = 0x8;
    static constexpr int      _push = 1;
    static constexpr int      _tcp_offset_seq_to_flags = 9;

    DelegatedEventLoop*  _event_loop;
    zf_attr*     _attr;
    zf_stack*    _stack;
    zft_handle*  _socket_handle = nullptr;
    zft*         _socket = nullptr;
    zf_waitable* _waitable_sock = nullptr;
    ef_vi*       _vi = nullptr;
    zf_ds        _zfds;

    uint64_t _pio_offset = 0;
    uint64_t _pio_data_len = 0;
    uint64_t _max_iov = 1;
    uint64_t _header_timestamp = 0;

    int _pio_id = 0;
    rx_msg _msg;

    char _headers_buf[MAX_ETH_HEADERS + MAX_IP_TCP_HEADERS + _max_send_size];
    char _send_buff[1024] = {};
    char _recv_buff[1024] = {};

  public:
    bool _pio_in_use = false;
    bool _has_write_event = false;
    bool _has_data_to_send = true;
};
