//
// Created by matts8023 on 2023/2/13.
//

#ifndef _OPENROAD_XR_TYPE_H_
#define _OPENROAD_XR_TYPE_H_

#include <nlohmann/json.hpp>
#include <vector>

namespace xr {
using xrDim = std::vector<int>;
using xrNodeMIdx = std::vector<int>;
using xrNodeCoord = std::vector<int>;
using xrNodeDetail = std::vector<int>;
using xrNodeCost = std::vector<int>;
using xrReward = std::vector<unsigned long>;
using xrNode = struct
{
  xrNodeMIdx idx;
  xrNodeCoord coord;
  xrNodeDetail detail;
  xrNodeCost cost;
};
using xrNodes = std::vector<xrNode>;
using xrData = struct
{
  xrDim dim;
  xrNodes nodes;
  xrReward reward;
};

void to_json(nlohmann::json& j, const xrNode& node);
void to_json(nlohmann::json& j, const xrData& data);
}  // namespace xr

#endif  // _OPENROAD_XR_TYPE_H_
