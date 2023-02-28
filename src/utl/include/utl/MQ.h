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
  enum Type
  {
    REQ,
    PUB,
  };

  MQ(const std::string& serverAddr,
     Type socketType = Type::REQ,
     int threadCnt = 1);
  std::string request(const std::string& msg);

 private:
  const std::string& serverAddr_;
  int threadCnt_;
  zmq::context_t context_;
  zmq::socket_t socket_;
};

}  // namespace utl
