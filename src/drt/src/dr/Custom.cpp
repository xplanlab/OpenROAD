//
// Created by matts8023 on 2023/12/20.
//

#include "Custom.h"

#include "FlexGridGraph.h"

int Custom::queryNetOrderWithGraph(FlexDRWorker* drWorker,
                                   frDebugSettings* debugSettings,
                                   Logger* logger,
                                   vector<unsigned int>* unroutedOuterIds)
{
  openroad_api::net_ordering::Message msg;
  openroad_api::net_ordering::Request* req = msg.mutable_request();

  generateGraph(req, drWorker, unroutedOuterIds);

  req->mutable_nets()->CopyFrom(
      {unroutedOuterIds->begin(), unroutedOuterIds->end()});

  if (unroutedOuterIds->empty()) {
    req->set_is_done(true);
    logger->info(DRT, 982, "Sending done message...");
  }

  string reqStr = msg.SerializeAsString();
  string addr = "tcp://" + debugSettings->apiAddr;
  utl::MQ mq(addr, debugSettings->apiTimeout);
  string res = mq.request(reqStr);

  if (unroutedOuterIds->empty()) {
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
                           vector<unsigned int>* unroutedOuterIds)
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
  for (const auto& [outerIndex, net] : drWorker->getOuterIdToNet()) {
    // 因为在启用 rerouteQueue 时，outerIdToNet 中存储的线网数据可能有问题
    // 所有这里直接通过 net_name 去 drWorker->getNets() 中查找线网的最新状态

    auto netName = net->getFrNet()->getName();
    const auto& drWorkerNets = drWorker->getNets();

    auto it = std::find_if(
        drWorkerNets.begin(), drWorkerNets.end(), [&netName](const auto& net) {
          return net->getFrNet()->getName() == netName;
        });

    auto targetNet = net;
    if (it != drWorkerNets.end()) {
      targetNet = it->get();
    }

    float pinNum = targetNet->getPins().size();
    int accessPointNum = 0;

    int xlo = INT_MAX, xhi = INT_MIN, ylo = INT_MAX, yhi = INT_MIN,
        zlo = INT_MAX, zhi = INT_MIN;

    // 遍历线网的所有 pin
    for (auto& pin : targetNet->getPins()) {
      for (auto& ap : pin->getAccessPatterns()) {
        accessPointNum++;

        // 计算该线网所有 pin 所围成的最小矩形
        auto pt = ap->getPoint();
        xlo = min(xlo, pt.x());
        xhi = max(xhi, pt.x());
        ylo = min(ylo, pt.y());
        yhi = max(yhi, pt.y());
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

    // is_routed 特征
    int isRouted = 0;
    if (unroutedOuterIds != nullptr
        && find(unroutedOuterIds->begin(), unroutedOuterIds->end(), outerIndex)
               == unroutedOuterIds->end()) {
      // 仅当该线网 ID 不在待布网络列表 unroutedOuterIds
      // 中时，才认为它已经被布过
      isRouted = 1;
    }
    nodeProperty->add_values(isRouted);
  }

  // 遍历所有线网，看哪两个线网有重叠
  // 注意：由于对 drWorker->getOuterNetMap() 使用了 range-based for
  // loop，所以此时 outerId 是从 0 开始从小到大排序的， 又由于
  // graph->add_node_properties 是从下标 0
  // 开始插入的数组，graph->add_edge_connections 也是， 从而
  // graph->edge_connections 可通过 netRects[outerIndex] 对应回
  // graph->node_properties 的顺序上
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

  //  for (int i = 0; i < graph->node_properties_size(); i++) {
  //    cout << i << ": ";
  //    for (int j = 0; j < graph->node_properties(i).values_size(); j++) {
  //      cout << graph->node_properties(i).values(j) << " ";
  //    }
  //    cout << endl;
  //  }
  //
  //  for (int i = 0; i < graph->edge_connections_size(); i++) {
  //    cout << i << ": ";
  //    for (int j = 0; j < graph->edge_connections(i).values_size(); j++) {
  //      cout << graph->edge_connections(i).values(j) << " ";
  //    }
  //    cout << endl;
  //  }
}

int Custom::queryNetOrderWithGrid(FlexDRWorker* drWorker,
                                  frDebugSettings* debugSettings,
                                  Logger* logger,
                                  vector<unsigned int>* unroutedOuterIds,
                                  vector<unsigned int>* routedOuterIds)
{
  openroad_api::net_ordering::Message msg;
  openroad_api::net_ordering::Request* req = msg.mutable_request();

  // 获取 gridGraph 的数据
  FlexGridGraph gridGraph_ = drWorker->getGridGraph();
  gridGraph_.dump(req, true);

  req->mutable_nets()->CopyFrom(
      {unroutedOuterIds->begin(), unroutedOuterIds->end()});

  req->mutable_routed_nets()->CopyFrom(
      {routedOuterIds->begin(), routedOuterIds->end()});

  if (unroutedOuterIds->empty()) {
    req->set_is_done(true);
  }

  string reqStr = msg.SerializeAsString();
  std::string addr = "tcp://" + debugSettings->apiAddr;
  utl::MQ mq(addr, debugSettings->apiTimeout);

  int selectedNetIdx;

  if (unroutedOuterIds->empty()) {
    logger->info(DRT, 994, "Sending done message...");
    mq.request(reqStr);
    return -1;
  } else {
    while (true) {
      logger->info(DRT, 997, "Requesting net order...");
      string res = mq.request(reqStr);
      msg = openroad_api::net_ordering::Message();
      msg.ParseFromString(res);

      selectedNetIdx = msg.response().net_index();
      if (selectedNetIdx >= 0 && selectedNetIdx < drWorker->getNets().size()) {
        logger->info(DRT, 995, "Selected net index {}.", selectedNetIdx);
        break;
      } else if (selectedNetIdx == -1) {
        logger->info(DRT, 986, "Outer thinks it has finished ordering.");

        for (auto netId : *unroutedOuterIds) {
          logger->info(
              DRT,
              984,
              "Outer net index {} remains, name {}.",
              netId,
              drWorker->getOuterIdToNet()[netId]->getFrNet()->getName());
        }

        return -1;
      } else {
        logger->info(DRT, 993, "Invalid net index {}.", selectedNetIdx);
      }
    }

    return selectedNetIdx;
  }
}