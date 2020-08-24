cmake_minimum_required(VERSION 3.15)
project(test_tcp_size)

set(CMAKE_CXX_STANDARD 17)

add_executable(test_tcp_size main.cpp tcp_server.cpp addr.h)

include_directories(/home/gleb/Downloads/OO/onload-7.1.0.265/src/lib/ciul/
        /home/gleb/Downloads/OO/onload-7.1.0.265/src/include
        #/home/gleb/Downloads/OO/onload-7.1.0.265/src
        )

target_link_libraries(test_tcp_size
        "/usr/lib/x86_64-linux-gnu/libciul1.a"
        "/usr/lib/x86_64-linux-gnu/libonload_zf_static.a")
