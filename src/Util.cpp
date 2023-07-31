#include <TrodesNetwork/Connection.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <zmq.hpp>

namespace trodes {
namespace network {
namespace util {


/* UNIX epoch timestamp */
int64_t get_timestamp() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
}

std::string get_connection_string(std::string address, int port) {
    return address + ":" + std::to_string(port);
}

std::string get_wildcard_string(std::string address) {
    return address + ":*";
}

std::string sock_endpoint(zmq::socket_t* socket) {
    // can't use this until cppzmq v4.7.0
    // return socket->get(zmq::sockopt::last_endpoint);

    // potentially buffer overflow
    char buffer[1024];
    size_t sz = sizeof(buffer);

    socket->getsockopt(ZMQ_LAST_ENDPOINT, buffer, &sz);

    // safe-ish because NULL-terminated
    std::string rv(buffer);
    return rv;
}

std::string get_endpoint_retry_forever(std::string address, int port, std::string name) {
    Connection c(address, port);
    std::string endpoint;

    while ((endpoint = c.get_endpoint(name)) == "") {
        std::cout << "Endpoint `" + name + "` is not available on the network yet. Retrying in 500ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return endpoint;
}

}
}
}
