//
// Created by matts8023 on 2023/12/20.
//

#include "Custom.h"

#include "FlexGridGraph.h"

int Custom::queryNetOrderWithGraph(FlexDRWorker* drWorker,
                                   frDebugSettings* debugSettings,
                                   Logger* logger,
                                   vector<unsigned int>* outerNetIdxRemaining)
{
  openroad_api::net_ordering::Message msg;
  openroad_api::net_ordering::Request* req = msg.mutable_request();

  generateGraph(req, drWorker, outerNetIdxRemaining);

  req->mutable_nets()->CopyFrom(
      {outerNetIdxRemaining->begin(), outerNetIdxRemaining->end()});

  if (outerNetIdxRemaining->empty()) {
    req->set_is_done(true);
    logger->info(DRT, 982, "Sending done message...");
  }

  string reqStr = msg.SerializeAsString();
  string addr = "tcp://" + debugSettings->apiAddr;
  utl::MQ mq(addr, debugSettings->apiTimeout);
  string res = mq.request(reqStr);

  if (outerNetIdxRemaining->empty()) {
    return -1;
  } else {
    msg = openroad_api::net_ordering::Message();
    msg.ParseFromString(res);
    int selectedIndex = msg.response().net_index();
    logger->info(DRT, 983, "Selected net index: {}", selectedIndex);
    return selectedIndex;
  }
}

void Custom::generateGraph(openroad_api::net_ordering::Request* req,
                           FlexDRWorker* drWorker,
                           vector<unsigned int>* outerNetIdxRemaining)
{
  openroad_api::net_ordering::Graph* graph = req->mutable_graph();

  // 获取 gridGraph 的数据
  FlexGridGraph gridGraph_ = drWorker->getGridGraph();
  gridGraph_.dump(req, true);

  // 线网的最小矩形
  map<int, Rect> netRects;

  // 计算布线区域的体积
  int layerCount = gridGraph_.getLayerCount() + 1;
  frArea regionVolume = drWorker->getRouteBox().area() * layerCount;

  // 遍历所有线网，获取线网特征
  for (const auto& [outerIndex, net] : drWorker->getOuterNetMap()) {
    float pinNum = net->getPins().size();
    int accessPointNum = 0;

    int xlo = INT_MAX, xhi = INT_MIN, ylo = INT_MAX, yhi = INT_MIN,
        zlo = INT_MAX, zhi = INT_MIN;

    // 遍历线网的所有 pin
    for (auto& uPin : net->getPins()) {
      for (auto& uAP : uPin.get()->getAccessPatterns()) {
        accessPointNum++;

        // 计算该线网所有 pin 所围成的最小矩形
        drAccessPattern* ap = uAP.get();
        xlo = min(xlo, ap->getPoint().x());
        xhi = max(xhi, ap->getPoint().x());
        ylo = min(ylo, ap->getPoint().y());
        yhi = max(yhi, ap->getPoint().y());
        zlo = min(zlo, ap->getBeginLayerNum());
        zhi = max(zhi, ap->getBeginLayerNum());
      }
    }

    // 记录线网的最小矩形
    netRects[outerIndex] = Rect(xlo, ylo, xhi, yhi);

    // 该线网平均每个 pin 的 access point 数量
    float accessPointRatio = (float) accessPointNum / pinNum;

    // 计算该线网体积占布线区域体积的比例
    float regionVolumeRatio = (float) (xhi - xlo + 1) * (yhi - ylo + 1)
                              * (zhi - zlo + 1) / regionVolume;

    // 记录线网的特征属性
    openroad_api::net_ordering::NodeProperty* nodeProperty
        = graph->add_node_properties();
    nodeProperty->add_values(pinNum);
    nodeProperty->add_values(accessPointRatio);
    nodeProperty->add_values(regionVolumeRatio);
  }

  // 遍历所有线网，看哪两个线网有重叠
  for (int i = 0; i < netRects.size(); i++) {
    for (int j = i + 1; j < netRects.size(); j++) {
      if (netRects[i].intersects(netRects[j])) {
        openroad_api::net_ordering::EdgeConnection* edgeConnection
            = graph->add_edge_connections();
        edgeConnection->add_values(i);
        edgeConnection->add_values(j);
      }
    }
  }

  // 更新 is_routed 特征
  for (int i = 0; i < graph->node_properties_size(); i++) {
    if (outerNetIdxRemaining != nullptr
        && find(outerNetIdxRemaining->begin(), outerNetIdxRemaining->end(), i)
               == outerNetIdxRemaining->end()) {
      graph->mutable_node_properties(i)->add_values(1);
    } else {
      graph->mutable_node_properties(i)->add_values(0);
    }
  }

//    for (int i = 0; i < graph->node_properties_size(); i++) {
//      cout << i << ": ";
//      for (int j = 0; j < graph->node_properties(i).values_size(); j++) {
//        cout << graph->node_properties(i).values(j) << " ";
//      }
//      cout << endl;
//    }
}