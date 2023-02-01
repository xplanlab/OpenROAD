//
// Created by matts8023 on 2023/2/1.
//

#ifndef _OPENROAD_MQ_H_
#define _OPENROAD_MQ_H_

#endif  // _OPENROAD_MQ_H_

#include <zmq.hpp>

namespace utl {

class MQ
{
 public:
  MQ(const std::string& serverAddr, int threadCnt = 1);
  void sendMessage(const std::string& msg);

 private:
  const std::string& serverAddr_;
  int threadCnt_;
  zmq::context_t context_;
  zmq::socket_t socket_;
};

}  // namespace utl
