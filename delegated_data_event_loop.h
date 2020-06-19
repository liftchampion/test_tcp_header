// Copyright 2019 Gleb Titov

#pragma once

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

class DelegatedEventLoop {
  public:
    explicit inline DelegatedEventLoop(const std::string &interface_name, int sends_count) noexcept
            : _send_events_limit(sends_count)
    {
        init(interface_name.data());
        _handlers.reserve(_capacity);
#ifdef USE_COPY_PIO
        std::cout << "Copy PIO mode" << std::endl;
#else
        std::cout << "Memcpy mode" << std::endl;
#endif
    }

    inline ~DelegatedEventLoop() noexcept
    {
        for (auto client : _handlers) { client->close(); }
        deinit();
    }

    inline bool has_free_space() const noexcept { return _size < _capacity; }
    inline zf_attr* get_attr() noexcept { return _attr; }
    inline zf_stack* get_stack() noexcept { return _stack; }
    inline ef_pio* get_pio() noexcept { return &_pio; }
    inline ef_vi* get_vi() noexcept { return &_vi; }

    inline bool add_handler(TCPDelegatedDataChannel* handler) noexcept
    {
        if (!has_free_space()) { return false; }
        _handlers.push_back(handler);
        _events[_size] = {.events = 0, .data = {.ptr = handler}};
        _events[_size].events |= EPOLLIN;
        _events[_size].events |= EPOLLOUT;
        if (zf_muxer_add(_muxer, handler->get_socket_waitable(), &_events[_size])) {
            std::cout << "Muxer add error" << std::endl;
            return false;
        }
        handler->set_pio_offset(384 * _size);
        handler->set_pio_id(_size);
        ++_size;
        _changed = true;
        return true;
    }

    inline void delete_handler(TCPDelegatedDataChannel* handler) noexcept
    {
        for (auto it = _handlers.begin(); it != _handlers.end();) {
            if (*it == handler) { it = _handlers.erase(it); } else { ++it; }
        }
        if (zf_muxer_del(_muxer, handler->get_socket_waitable(), &_events[_size])) {
            std::cout << "Muxer add error" << std::endl;
            return;
        }
        _changed = true;
        handler->set_event_loop(nullptr);
        handler->set_pio_offset(0);
    }

    inline int run_iteration(uint64_t timeout_us) noexcept
    {
        evq_poll();
        int events_count = zf_muxer_wait(_muxer, _events, _capacity, timeout_us * 1000);
        if (events_count < 0) {
            std::cout << "Wait err" << std::endl;
            return -1;
        }
        do {
            _changed = false;
            for (int i = 0; i < events_count; ++i) {
                if (!perform_handler_events(&_events[i])) { break; }
            }
        } while (_changed);
        if (check_handlers()) {
            do_writes();
        }
        return _send_events_happen >= _send_events_limit;
    }

    inline void evq_poll() noexcept
    {
        ef_request_id ids[EF_VI_TRANSMIT_BATCH];
        ef_event evs[EF_VI_EVENT_POLL_MIN_EVS];
        int n_ev;
        int unbundle_ret;
        timespec tmp_ts = {0, 0};
        int id;
        int timestamp_id;

        n_ev = ef_eventq_poll(&_vi, evs, sizeof(evs) / sizeof(evs[0]));
        for (int i = 0; i < n_ev; ++i) {
            switch (EF_EVENT_TYPE(evs[i])) {
                case EF_EVENT_TYPE_TX:
                    unbundle_ret = ef_vi_transmit_unbundle(&_vi, &evs[i], ids);
                    for (int j = 0; j < unbundle_ret; ++j) { _handlers[ids[j]]->_pio_in_use = false; }
                    break;
                default:
                    fprintf(stderr, "ERROR: unexpected event \n");
                    return;
                    break;
            }
        }
    }

    inline bool perform_handler_events(epoll_event* event) noexcept
    {
        auto handler = (TCPDelegatedDataChannel*)event->data.ptr;

        if (event->events & EPOLLIN) {
            int ret = handler->read_event();
            event->events &= ~EPOLLIN;
            ((TCPDelegatedDataChannel*)event->data.ptr)->_has_data_to_send = true;
        }
        if ((event->events & EPOLLOUT) && ((TCPDelegatedDataChannel*)event->data.ptr)->_has_data_to_send && _send_events_happen < _send_events_limit) {
            handler->_has_write_event = true;
        }

        return true;
    }

    inline bool check_handlers() noexcept
    {
        for (auto h : _handlers) {
            if (!h->_has_write_event || !h->_has_data_to_send || h->_pio_in_use) { return false; }
        }
        return true;
    }

    inline void do_writes() noexcept
    {
        uint64_t start;
        uint64_t end;
        uint32_t aux;
        for (int i = 0; i < _size; ++i) { fill_msg_with_rand(_msgs[i]);  }
#ifdef USE_COPY_PIO
        for (auto h : _handlers) { h->create_header(0, false); }
        for (int i = 0; i < _size; ++i) { _handlers[i]->prepare_data_for_copy_pio(_msgs[i], _msg_len); }
        ef_vi_transmit_pio_warm(&_vi);
        start = __rdtscp(&aux);
        for (auto h : _handlers) { h->write_event_copy_pio(); }
        end = __rdtscp(&aux);
#else
        for (auto h : _handlers) { h->create_header(0, true); }
        ef_vi_transmit_pio_warm(&_vi);
        start = __rdtscp(&aux);
        for (int i = 0; i < _size; ++i) { _handlers[i]->write_event(_msgs[i], _msg_len); }
        end = __rdtscp(&aux);
#endif
        _stats.add(end - start);
        for (auto h : _handlers) {
            h->_has_write_event = false;
            h->_has_data_to_send = false;
            h->_pio_in_use = true;
            h->complete_send(_msg, _msg_len);
        }
        _send_events_happen += _handlers.size();
        _sended_total += _handlers.size() * _msg_len;
    }

  private:
    inline void init(const char* interface_name) noexcept
    {
        if (zf_init()) {
            std::cout << "zf_init err" << std::endl;
        }
        if (zf_attr_alloc(&_attr)) {
            std::cout << "zf_attr_alloc err" << std::endl;
        }
        if (zf_attr_set_from_str(_attr, "interface", interface_name)) {
            std::cout << "zf_attr_set_from_str err" << std::endl;
        }
        if (zf_stack_alloc(_attr, &_stack)) {
            std::cout << "zf_stack_alloc err" << std::endl;
        }
        if (zf_muxer_alloc(_stack, &_muxer)) {
            std::cout << "zf_muxer_alloc err" << std::endl;
        }
        init_ev_fi(interface_name);
    }

    inline void init_ev_fi(const char* interface_name) noexcept
    {
        std::cout << "Trying to init ev_fi with interface: '" << interface_name << "'" << std::endl;
        if (ef_driver_open(&_driver_handle)) {
            std::cout << "ef_driver_open err" << std::endl;
        }

        if (ef_pd_alloc_by_name(&_pd, _driver_handle, interface_name, EF_PD_DEFAULT)) {
            std::cout << "ef_pd_alloc_by_name err" << std::endl;
        }
        int flags = EF_VI_FLAGS_DEFAULT;
        if (ef_vi_alloc_from_pd(&_vi, _driver_handle, &_pd, _driver_handle, -1, 0, -1, nullptr, -1, static_cast<enum ef_vi_flags >(flags)) < 0) {
            std::cout << "ef_vi_alloc_from_pd err" << std::endl;
        }
        if (ef_pio_alloc(&_pio, _driver_handle, &_pd, -1, _driver_handle)) {
            std::cout << "ef_pio_alloc err" << std::endl;
        }
        if (ef_pio_link_vi(&_pio, _driver_handle, &_vi, _driver_handle)) {
            std::cout << "ef_pio_link_vi err" << std::endl;
        }
    }

    inline void deinit() noexcept
    {
        deinit_ev_fi();
        zf_muxer_free(_muxer);
        if (zf_stack_free(_stack)) {
            std::cout << "zf_stack_free err" << std::endl;
        }
        zf_attr_free(_attr);
        if (zf_deinit()) {
            std::cout << "zf_deinit err" << std::endl;
        }
    }

    inline void deinit_ev_fi() noexcept
    {
        if (ef_pio_unlink_vi(&_pio, _driver_handle, &_vi, _driver_handle)) {
            std::cout << "ef_pio_unlink_vi err" << std::endl;
        }
        if (ef_pio_free(&_pio, _driver_handle)) {
            std::cout << "ef_pio_free err" << std::endl;
        }
        if (ef_vi_free(&_vi, _driver_handle)) {
            std::cout << "ef_vi_free err" << std::endl;
        }
        if (ef_pd_free(&_pd, _driver_handle)) {
            std::cout << "ef_pd_free err" << std::endl;
        }
        if (ef_driver_close(_driver_handle)) {
            std::cout << "ef_driver_close err" << std::endl;
        }
    }

    inline void fill_msg_with_rand(char* data) noexcept
    {
        for (size_t i = 0; i < _msg_len; ++i) { data[i] = rand() % 90 + 33; }
    }

    static constexpr size_t _msg_len = 314;
    static constexpr int    _capacity = 10;

    std::vector<TCPDelegatedDataChannel*> _handlers;

    zf_attr*         _attr = nullptr;
    zf_stack*        _stack = nullptr;
    zf_muxer_set*    _muxer = nullptr;
    ef_pd            _pd;
    ef_pio           _pio;
    ef_driver_handle _driver_handle;
    ef_vi            _vi;

    struct epoll_event _events[_capacity];

    timespec** _all_timestamps;
    int _timestamps_counters[_capacity] = {0};

    const int _send_events_limit;
    int _size = 0;
    int _read_events_happen = 0;
    int _send_events_happen = 0;

    char _msgs[_capacity][_msg_len];
    char _msg[_msg_len];

    bool _changed = false;
};