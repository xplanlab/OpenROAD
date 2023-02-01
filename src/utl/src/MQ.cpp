//
// Created by matts8023 on 2023/2/1.
//

#include <iostream>
#include <zmq.hpp>

#include "utl/MQ.h"

namespace utl {

MQ::MQ(const std::string& serverAddress, int threadCount)
    : serverAddr_(serverAddress), threadCnt_(threadCount)
{
  // initialize the zmq context with a single IO thread
  context_ = zmq::context_t(threadCnt_);

  // construct a REQ (request) socket and connect to interface
  socket_ = zmq::socket_t(context_, zmq::socket_type::req);
  socket_.connect(serverAddr_);
}

void MQ::sendMessage(const std::string& message)
{
  // send the request message
  const std::string data{message};
  socket_.send(zmq::buffer(data), zmq::send_flags::none);

  // wait for reply from server
  zmq::message_t reply{};
  socket_.recv(reply, zmq::recv_flags::none);
  std::cout << "Received: " << reply.to_string() << std::endl;
}

}
