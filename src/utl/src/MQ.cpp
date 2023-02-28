//
// Created by matts8023 on 2023/2/1.
//

#include <iostream>
#include <zmq.hpp>

#include "utl/MQ.h"

namespace utl {

MQ::MQ(const std::string& serverAddress, Type socketType, int threadCount)
    : serverAddr_(serverAddress), threadCnt_(threadCount)
{
  // initialize the zmq context with a single IO thread
  context_ = zmq::context_t(threadCnt_);

  switch (socketType) {
    case Type::REQ:
      // construct a REQ (request) socket and connect to interface
      socket_ = zmq::socket_t(context_, zmq::socket_type::req);
      socket_.connect(serverAddr_);
      break;

    case Type::PUB:
      // construct a PUB (publisher) socket and connect to interface
      socket_ = zmq::socket_t(context_, zmq::socket_type::pub);
      socket_.bind(serverAddr_);
      break;

    default:
      break;
  }
}

std::string MQ::request(const std::string& msg)
{
  socket_.send(zmq::buffer(msg));
  zmq::message_t reply{};
  socket_.recv(reply);
  return reply.to_string();
}

}  // namespace utl
