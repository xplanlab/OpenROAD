//
// Created by matts8023 on 2023/2/2.
//

#include "xrType.h"

namespace xr {
void to_json(nlohmann::json& j, const xrNode& node)
{
  j = nlohmann::json::array({node.idx, node.coord, node.detail, node.cost});
}

void to_json(nlohmann::json& j, const xrData& data)
{
  j = nlohmann::json::array({data.dim, data.nodes});
}
}  // namespace xr
