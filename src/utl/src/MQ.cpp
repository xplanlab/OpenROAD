//
// Created by zarkin404 on 2023/2/1.
//

#include "utl/MQ.h"

#include <iostream>
#include <zmq.hpp>

namespace utl {

MQ::MQ(const std::string& serverAddress,
       int timeout,
       int maxRetry,
       SocketType socketType,
       int threadCount)
    : serverAddr_(serverAddress),
      timeout_(timeout),
      maxRetry_(maxRetry),
      socketType_(socketType),
      threadCnt_(threadCount)
{
  MQ::init();
}

void MQ::init()
{
  // initialize the zmq context with a single IO thread
  context_ = zmq::context_t(threadCnt_);

  switch (socketType_) {
    case SocketType::REQ:
      socket_ = zmq::socket_t(context_, zmq::socket_type::req);
      socket_.set(zmq::sockopt::linger, 0);
      socket_.set(zmq::sockopt::rcvtimeo, timeout_);
      socket_.connect(serverAddr_);
      break;

    case SocketType::PUB:
      socket_ = zmq::socket_t(context_, zmq::socket_type::pub);
      socket_.set(zmq::sockopt::linger, 0);
      socket_.set(zmq::sockopt::rcvtimeo, timeout_);
      socket_.bind(serverAddr_);
      break;

    default:
      break;
  }
}

void MQ::destroy()
{
  socket_.close();
  context_.close();
}

std::string MQ::request(const std::string& msg)
{
  int retry = 0;
  while (true) {
    socket_.send(zmq::buffer(msg), zmq::send_flags::dontwait);
    zmq::message_t reply;
    zmq::recv_result_t result = socket_.recv(reply);

    if (result && result.value() > 0) {
      return reply.to_string();
    } else {
      retry++;
      destroy();
      init();

      if (maxRetry_ == -1) {
        std::cout << "No reply received from server, infinitely retrying "
                  << retry << " time(s)..." << std::endl;
      } else if (maxRetry_ > 0) {
        if (retry > maxRetry_) {
          std::cout << "No reply received from server, giving up." << std::endl;
          assert(false);
        } else {
          std::cout << "No reply received from server, retrying " << retry
                    << " time(s..." << std::endl;
        }
      } else {
        std::cout << "Invalid maxRetry_ " << maxRetry_ << " ." << std::endl;
        assert(false);
      }
    }
  }
}

}  // namespace utl