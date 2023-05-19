//
// Created by zarkin404 on 2023/2/1.
//

#pragma once

#include <zmq.hpp>

namespace utl {

class MQ
{
 public:
  enum SocketType
  {
    REQ,
    PUB,
  };

  MQ(const std::string& serverAddr,
     int timeout = 30000,
     int maxRetry = -1,
     SocketType socketType = SocketType::REQ,
     int threadCnt = 1);
  void init();
  void destroy();
  std::string request(const std::string& msg);

 private:
  const std::string& serverAddr_;
  const int timeout_;
  const int maxRetry_;
  SocketType socketType_;
  int threadCnt_;
  zmq::context_t context_;
  zmq::socket_t socket_;
};

}  // namespace utl
