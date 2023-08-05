#pragma once

#include <TrodesNetwork/Util.h>

#include <msgpack.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <zmq.hpp>
#include <msgpack.hpp>

template<typename T>
class ZmqSourceSubscriber {
public:
    ZmqSourceSubscriber(std::string endpoint)
    : ctx_(1),
      socket_(ctx_, zmq::socket_type::sub)
    {
        socket_.connect(endpoint.c_str());
        socket_.set(zmq::sockopt::subscribe, "");
    }

    ~ZmqSourceSubscriber() {
    }

    T receive() {
        zmq::message_t message;
        auto rv = socket_.recv(message);
        if (!rv.has_value()) {
            // failed to receive
        }
        return trodes::network::util::unpack<T>(message.to_string());
    }

public:
    zmq::context_t ctx_;
    zmq::socket_t socket_;
private:

};

