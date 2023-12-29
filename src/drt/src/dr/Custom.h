//
// Created by matts8023 on 2023/12/20.
//

#ifndef OPENROAD_CUSTOM_H
#define OPENROAD_CUSTOM_H

#include "dr/FlexDR.h"

using namespace std;
using namespace fr;

class Custom
{
public:
  Custom();
  ~Custom();

  static int queryNetOrderWithGraph(FlexDRWorker* drWorker,
                                     frDebugSettings* debugSettings,
                                     Logger* logger,
                                     vector<unsigned int>* outerNetIdxRemaining);

  static void generateGraph(openroad_api::net_ordering::Request* req,
                            FlexDRWorker* drWorker,
                            vector<unsigned int>* outerNetIdxRemaining = nullptr);
};

#endif  // OPENROAD_CUSTOM_H
