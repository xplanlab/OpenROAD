/* Authors: Lutong Wang and Bangqi Xu */
/*
 * Copyright (c) 2019, The Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dr/FlexGridGraph.h"

#include <fstream>
#include <iostream>
#include <map>

#include "dr/FlexDR.h"

using namespace std;
using namespace fr;

void FlexGridGraph::initGrids(
    const map<frCoord, map<frLayerNum, frTrackPattern*>>& xMap,
    const map<frCoord, map<frLayerNum, frTrackPattern*>>& yMap,
    const map<frLayerNum, dbTechLayerDir>& zMap,
    bool followGuide)
{
  // initialize coord vectors
  xCoords_.clear();
  yCoords_.clear();
  zCoords_.clear();
  zHeights_.clear();
  layerRouteDirections_.clear();
  xCoords_.reserve(xMap.size());
  for (auto& [k, v] : xMap) {
    xCoords_.push_back(k);
  }
  yCoords_.reserve(yMap.size());
  for (auto& [k, v] : yMap) {
    yCoords_.push_back(k);
  }
  frCoord zHeight = 0;
  // vector<frCoord> via2viaMinLenTmp(4, 0);
  zCoords_.reserve(zMap.size());
  for (auto& [k, v] : zMap) {
    zCoords_.push_back(k);
    zHeight += getTech()->getLayer(k)->getPitch() * VIACOST;
    zHeights_.push_back(zHeight);
    layerRouteDirections_.push_back(v);
  }
  // initialize all grids
  frMIdx xDim, yDim, zDim;
  getDim(xDim, yDim, zDim);
  const int capacity = xDim * yDim * zDim;

  nodes_.clear();
  nodes_.resize(capacity, Node());
  // new
  prevDirs_.clear();
  srcs_.clear();
  dsts_.clear();

  prevDirs_.resize(capacity * 3, 0);
  srcs_.resize(capacity, 0);
  dsts_.resize(capacity, 0);
  guides_.clear();
  if (followGuide) {
    guides_.resize(capacity, 0);
  } else {
    guides_.resize(capacity, 1);
  }
}

bool FlexGridGraph::outOfDieVia(frMIdx x,
                                frMIdx y,
                                frMIdx z,
                                const Rect& dieBox)
{
  frLayerNum lNum = getLayerNum(z) + 1;
  if (lNum > getTech()->getTopLayerNum())
    return false;
  frViaDef* via = getTech()->getLayer(lNum)->getDefaultViaDef();
  Rect viaBox(via->getLayer1ShapeBox());
  viaBox.merge(via->getLayer2ShapeBox());
  viaBox.moveDelta(xCoords_[x], yCoords_[y]);
  return !dieBox.contains(viaBox);
}
bool FlexGridGraph::hasOutOfDieViol(frMIdx x, frMIdx y, frMIdx z)
{
  const frLayerNum lNum = getLayerNum(z);
  if (!getTech()->getLayer(lNum)->isUnidirectional()) {
    return false;
  }
  Rect testBoxUp;
  if (lNum + 1 <= getTech()->getTopLayerNum()) {
    frViaDef* via = getTech()->getLayer(lNum + 1)->getDefaultViaDef();
    if (via) {
      testBoxUp = via->getLayer1ShapeBox();
      testBoxUp.merge(via->getLayer2ShapeBox());
      testBoxUp.moveDelta(xCoords_[x], yCoords_[y]);
    } else {
      // artificial value to indicate no via in test below
      dieBox_.bloat(1, testBoxUp);
    }
  }
  Rect testBoxDown;
  if (lNum - 1 >= getTech()->getBottomLayerNum()) {
    frViaDef* via = getTech()->getLayer(lNum - 1)->getDefaultViaDef();
    if (via) {
      testBoxDown = via->getLayer1ShapeBox();
      testBoxDown.merge(via->getLayer2ShapeBox());
      testBoxDown.moveDelta(xCoords_[x], yCoords_[y]);
    } else {
      // artificial value to indicate no via in test below
      dieBox_.bloat(1, testBoxDown);
    }
  }
  if (getTech()->getLayer(lNum)->isVertical()) {
    return (testBoxUp.xMax() > dieBox_.xMax()
            || testBoxUp.xMin() < dieBox_.xMin())
           && (testBoxDown.xMax() > dieBox_.xMax()
               || testBoxDown.xMin() < dieBox_.xMin());
  }
  // layer is horizontal
  return (testBoxUp.yMax() > dieBox_.yMax()
          || testBoxUp.yMin() < dieBox_.yMin())
         && (testBoxDown.yMax() > dieBox_.yMax()
             || testBoxDown.yMin() < dieBox_.yMin());
}

bool FlexGridGraph::isWorkerBorder(frMIdx v, bool isVert)
{
  if (isVert)
    return xCoords_[v] == drWorker_->getRouteBox().xMin()
           || xCoords_[v] == drWorker_->getRouteBox().xMax();
  return yCoords_[v] == drWorker_->getRouteBox().yMin()
         || yCoords_[v] == drWorker_->getRouteBox().yMax();
}
bool FlexGridGraph::hasAlignedUpDefTrack(
    frLayerNum layerNum,
    const map<frLayerNum, frTrackPattern*>& xSubMap,
    const map<frLayerNum, frTrackPattern*>& ySubMap) const
{
  for (frLayerNum lNum = layerNum + 2;
       lNum < (int) getTech()->getLayers().size();
       lNum += 2) {
    auto it = xSubMap.find(lNum);
    if (it != xSubMap.end()) {  // has x track in lNum
      if (it->second)           // has track pattern, i.e., the track is default
        return true;
    }
    it = ySubMap.find(lNum);
    if (it != ySubMap.end()) {  // has y track in lNum
      if (it->second)           // has track pattern, i.e., the track is default
        return true;
    }
  }
  return false;
}

void FlexGridGraph::initEdges(
    const frDesign* design,
    map<frCoord, map<frLayerNum, frTrackPattern*>>& xMap,
    map<frCoord, map<frLayerNum, frTrackPattern*>>& yMap,
    const map<frLayerNum, dbTechLayerDir>& zMap,
    const Rect& bbox,
    bool initDR)
{
  frMIdx xDim, yDim, zDim;
  getDim(xDim, yDim, zDim);
  // initialize grid graph
  frMIdx xIdx = 0, yIdx = 0, zIdx = 0;
  dieBox_ = design->getTopBlock()->getDieBox();

  // 按层初始化 nodes_ 的边，注意，尽管 zMap 不包含 Cut Layer，但层号却计入了 Cut Layer，所以后面的层号 ±2 都是在指首选方向不同的相邻层
  for (const auto& [layerNum, dir] : zMap) {
    frLayerNum nonPrefLayerNum;
    const auto layer = getTech()->getLayer(layerNum);
    // 将相邻层作为非首选层
    if (layerNum + 2 <= getTech()->getTopLayerNum()) {
      nonPrefLayerNum = layerNum + 2;
    } else if (layerNum - 2 >= getTech()->getBottomLayerNum()) {
      nonPrefLayerNum = layerNum - 2;
    } else {
      nonPrefLayerNum = layerNum;
    }
    yIdx = 0;

    // ySubMap 为在对应坐标上有边的层其 track pattern 所构成的 map（"72770": { "2": { "numTracks_": 51, "trackSpacing_": 380, "layerNum_": 6 ... } }）
    for (auto& [yCoord, ySubMap] : yMap) {
      auto yIt = ySubMap.find(layerNum);  // 看看当前层在这个坐标上有没有边，iterator 为 <int layerNum, frTrackPattern* tp>
      auto yIt2 = ySubMap.find(layerNum + 2); // 看看相邻上层上有没有边（当当前层方向是 V，那么相邻层的 H 方向如果有边，就可以 Via）
      auto yIt3 = ySubMap.find(nonPrefLayerNum);  // 非首选层上有没有边
      bool yFound = (yIt != ySubMap.end());
      bool yFound2 = (yIt2 != ySubMap.end());
      bool yFound3 = (yIt3 != ySubMap.end());
      xIdx = 0;
      for (auto& [xCoord, xSubMap] : xMap) {
        auto xIt = xSubMap.find(layerNum);
        auto xIt2 = xSubMap.find(layerNum + 2);
        auto xIt3 = xSubMap.find(nonPrefLayerNum);
        bool xFound = (xIt != xSubMap.end());
        bool xFound2 = (xIt2 != xSubMap.end());
        bool xFound3 = (xIt3 != xSubMap.end());
        // add cost to out-of-die edge
        bool isOutOfDieVia = outOfDieVia(xIdx, yIdx, zIdx, dieBox_);
        // add edge for preferred direction
        if (dir == dbTechLayerDir::HORIZONTAL && yFound) {  // yCoord 在当前层有边且同向
          if (layerNum >= BOTTOM_ROUTING_LAYER
              && layerNum <= TOP_ROUTING_LAYER) {
            if ((!isOutOfDieVia || !hasOutOfDieViol(xIdx, yIdx, zIdx))
                && (layer->getLef58RightWayOnGridOnlyConstraint() == nullptr
                    || yIt->second != nullptr)) { // yIt->second 为空时说明没有 track pattern
              addEdge(xIdx, yIdx, zIdx, frDirEnum::E, bbox, initDR);  // 为 xIdx, yIdx, zIdx 这个点添加 E 方向的边
              if (yIt->second == nullptr || isWorkerBorder(yIdx, false)) {
                setGridCostE(xIdx, yIdx, zIdx); // 如果该点在 Clip 的边上，则标志边有 cost
              }
            }
          }
          // via to upper layer
          if (xFound2) {  // 如果该点在相邻上层有边，那就意味着有过孔
            if (!isOutOfDieVia) {
              addEdge(xIdx, yIdx, zIdx, frDirEnum::U, bbox, initDR);
              bool condition
                  = (yIt->second == nullptr || xIt2->second == nullptr);
              if (condition) {
                setGridCostU(xIdx, yIdx, zIdx);
              }
            }
          }
        } else if (dir == dbTechLayerDir::VERTICAL && xFound) {
          if (layerNum >= BOTTOM_ROUTING_LAYER
              && layerNum <= TOP_ROUTING_LAYER) {
            if ((!isOutOfDieVia || !hasOutOfDieViol(xIdx, yIdx, zIdx))
                && (layer->getLef58RightWayOnGridOnlyConstraint() == nullptr
                    || xIt->second != nullptr)) {
              addEdge(xIdx, yIdx, zIdx, frDirEnum::N, bbox, initDR);
              if (xIt->second == nullptr || isWorkerBorder(xIdx, true)) {
                setGridCostN(xIdx, yIdx, zIdx);
              }
            }
          }
          // via to upper layer
          if (yFound2) {
            if (!isOutOfDieVia) {
              addEdge(xIdx, yIdx, zIdx, frDirEnum::U, bbox, initDR);
              bool condition
                  = (yIt2->second == nullptr || xIt->second == nullptr);
              if (condition) {
                setGridCostU(xIdx, yIdx, zIdx);
              }
            }
          }
        }
        // get non pref track layer --> use upper layer pref dir track if
        // possible
        if (USENONPREFTRACKS && !layer->isUnidirectional()) {
          // add edge for non-preferred direction
          // vertical non-pref track
          if (dir == dbTechLayerDir::HORIZONTAL && xFound3) {
            if (layerNum >= BOTTOM_ROUTING_LAYER
                && layerNum <= TOP_ROUTING_LAYER) {
              addEdge(xIdx, yIdx, zIdx, frDirEnum::N, bbox, initDR);
              setGridCostN(xIdx, yIdx, zIdx);
            }
            // horizontal non-pref track
          } else if (dir == dbTechLayerDir::VERTICAL && yFound3) {
            if (layerNum >= BOTTOM_ROUTING_LAYER
                && layerNum <= TOP_ROUTING_LAYER) {
              addEdge(xIdx, yIdx, zIdx, frDirEnum::E, bbox, initDR);
              setGridCostE(xIdx, yIdx, zIdx);
            }
          }
        }
        ++xIdx;
      }
      ++yIdx;
    }
    ++zIdx;
  }
  // this creates via edges over each ap until reaching a default track or a
  // layer with normal routing; in this case it creates jogs connections to the
  // neighboring tracks
  for (const Point3D& apPt : drWorker_->getSpecialAccessAPs()) {
    for (int i = 0; i < 2; i++) {  // down and up
      bool up = (bool) i;
      int inc = up ? 1 : -1;
      frMIdx startZ = getMazeZIdx(apPt.z());
      frLayerNum nextLNum = getLayerNum(startZ) + 2 * inc;
      if (!up)
        startZ--;
      frMIdx xIdx = getMazeXIdx(apPt.x());
      frMIdx yIdx = getMazeYIdx(apPt.y());
      // create the edges
      for (int zIdx = startZ; zIdx >= 0 && zIdx < (int) zCoords_.size() - 1;
           zIdx += inc, nextLNum += inc * 2) {
        addEdge(xIdx, yIdx, zIdx, frDirEnum::U, bbox, initDR);
        frLayer* nextLayer = getTech()->getLayer(nextLNum);
        const bool restrictedRouting = nextLayer->isUnidirectional()
                                       || nextLNum < BOTTOM_ROUTING_LAYER
                                       || nextLNum > TOP_ROUTING_LAYER;
        if (!restrictedRouting || nextLayer->isVertical()) {
          auto& xSubMap = xMap[apPt.x()];
          auto xTrack = xSubMap.find(nextLNum);
          if (xTrack != xSubMap.end() && xTrack->second != nullptr)
            break;
        }
        if (!restrictedRouting || nextLayer->isHorizontal()) {
          auto& ySubMap = yMap[apPt.y()];
          auto yTrack = ySubMap.find(nextLNum);
          if (yTrack != ySubMap.end() && yTrack->second != nullptr)
            break;
        }
        // didnt find default track, then create tracks if possible
        if (!restrictedRouting && nextLNum >= VIA_ACCESS_LAYERNUM) {
          dbTechLayerDir prefDir
              = design->getTech()->getLayer(nextLNum)->getDir();
          xMap[apPt.x()][nextLNum] = nullptr;  // to keep coherence
          yMap[apPt.y()][nextLNum] = nullptr;
          frMIdx nextZ = up ? zIdx + 1 : zIdx;
          // This is a value to make sure the edges we are adding will
          // reach a track on the layer of interest.  It is simpler to
          // be conservative than trying to figure out how many edges
          // to add to hit it precisely.  I intend to obviate the need
          // for this whole approach next.  Note that addEdge checks
          // for bounds so I don't.
          const int max_offset = 20;
          if (prefDir == dbTechLayerDir::HORIZONTAL) {
            for (int offset = 0; offset < max_offset; ++offset) {
              addEdge(xIdx, yIdx + offset, nextZ, frDirEnum::N, bbox, initDR);
              addEdge(xIdx, yIdx - offset, nextZ, frDirEnum::S, bbox, initDR);
            }
          } else {
            for (int offset = 0; offset < max_offset; ++offset) {
              addEdge(xIdx + offset, yIdx, nextZ, frDirEnum::E, bbox, initDR);
              addEdge(xIdx - offset, yIdx, nextZ, frDirEnum::W, bbox, initDR);
            }
          }
          break;
        }
      }
    }
  }
}

// initialization: update grid graph topology, does not assign edge cost
void FlexGridGraph::init(const frDesign* design,
                         const Rect& routeBBox,
                         const Rect& extBBox,
                         map<frCoord, map<frLayerNum, frTrackPattern*>>& xMap,
                         map<frCoord, map<frLayerNum, frTrackPattern*>>& yMap,
                         bool initDR,
                         bool followGuide)
{
  auto* via_data = getDRWorker()->getViaData();
  halfViaEncArea_ = &via_data->halfViaEncArea;

  // get tracks intersecting with the Maze bbox
  map<frLayerNum, dbTechLayerDir> zMap;
  initTracks(design, xMap, yMap, zMap, extBBox); // 根据 DEF 中 TRACK 的定义，将相同方向的层的轨道坐标存入 xMap 和 yMap 这两个 map 中
  initGrids(xMap, yMap, zMap, followGuide);  // buildGridGraph 还有初始化 Track 交织形成的交点集合 nodes_
  initEdges(
      design, xMap, yMap, zMap, routeBBox, initDR);  // add edges and edgeCost
}

// initialization helpers
// get all tracks intersecting with the Maze bbox, left/bottom are inclusive
void FlexGridGraph::initTracks(
    const frDesign* design,
    map<frCoord, map<frLayerNum, frTrackPattern*>>& xMap,
    map<frCoord, map<frLayerNum, frTrackPattern*>>& yMap,
    map<frLayerNum, dbTechLayerDir>& zMap,
    const Rect& bbox)
{
  for (auto& layer : getTech()->getLayers()) {
    if (layer->getType() != dbTechLayerType::ROUTING) {
      continue;
    }
    frLayerNum currLayerNum = layer->getLayerNum();
    dbTechLayerDir currPrefRouteDir = layer->getDir();
    for (auto& tp : design->getTopBlock()->getTrackPatterns(currLayerNum)) {
      // allow wrongway if global variable and design rule allow
      bool flag
          = (USENONPREFTRACKS && !layer->isUnidirectional())
                ? (tp->isHorizontal()
                   && currPrefRouteDir == dbTechLayerDir::VERTICAL)
                      || (!tp->isHorizontal()
                          && currPrefRouteDir == dbTechLayerDir::HORIZONTAL)
                : true;
      if (flag) {
        int trackNum = ((tp->isHorizontal() ? bbox.xMin() : bbox.yMin())
                        - tp->getStartCoord())
                       / (int) tp->getTrackSpacing();
        if (trackNum < 0) {
          trackNum = 0;
        }
        if (trackNum * (int) tp->getTrackSpacing() + tp->getStartCoord()
            < (tp->isHorizontal() ? bbox.xMin() : bbox.yMin())) {
          ++trackNum;
        }
        for (; trackNum < (int) tp->getNumTracks()
               && trackNum * (int) tp->getTrackSpacing() + tp->getStartCoord()
                      < (tp->isHorizontal() ? bbox.xMax() : bbox.yMax());
             ++trackNum) {
          frCoord trackLoc
              = trackNum * tp->getTrackSpacing() + tp->getStartCoord();
          // 注意：同一 coord 上可能有不同同向的层的轨道在上面
          if (tp->isHorizontal()) {
            xMap[trackLoc][currLayerNum] = tp.get();
          } else {
            yMap[trackLoc][currLayerNum] = tp.get();
          }
        }
      }
    }
    zMap[currLayerNum] = currPrefRouteDir;
  }
}

void FlexGridGraph::resetStatus()
{
  resetSrc();
  resetDst();
  resetPrevNodeDir();
}

void FlexGridGraph::resetSrc()
{
  srcs_.assign(srcs_.size(), 0);
}

void FlexGridGraph::resetDst()
{
  dsts_.assign(dsts_.size(), 0);
}

void FlexGridGraph::resetPrevNodeDir()
{
  prevDirs_.assign(prevDirs_.size(), 0);
}

// print the grid graph with edge and vertex for debug purpose
void FlexGridGraph::print() const
{
  ofstream mazeLog(OUT_MAZE_FILE.c_str());
  if (mazeLog.is_open()) {
    // print edges
    Rect gridBBox;
    getBBox(gridBBox);
    mazeLog << "printing Maze grid (" << gridBBox.xMin() << ", "
            << gridBBox.yMin() << ") -- (" << gridBBox.xMax() << ", "
            << gridBBox.yMax() << ")\n";
    frMIdx xDim, yDim, zDim;
    getDim(xDim, yDim, zDim);

    if (xDim == 0 || yDim == 0 || zDim == 0) {
      cout << "Error: dimension == 0\n";
      return;
    } else {
      cout << "extBBox (xDim, yDim, zDim) = (" << xDim << ", " << yDim << ", "
           << zDim << ")\n";
    }

    Point p;
    for (frMIdx xIdx = 0; xIdx < xDim; ++xIdx) {
      for (frMIdx yIdx = 0; yIdx < yDim; ++yIdx) {
        for (frMIdx zIdx = 0; zIdx < zDim; ++zIdx) {
          if (hasEdge(xIdx, yIdx, zIdx, frDirEnum::N)) {
            if (yIdx + 1 >= yDim) {
              cout << "Error: no edge (" << xIdx << ", " << yIdx << ", " << zIdx
                   << ", N) " << yDim << endl;
              continue;
            }
            mazeLog << "Edge: " << getPoint(p, xIdx, yIdx).x() << " "
                    << getPoint(p, xIdx, yIdx).y() << " " << zIdx << " "
                    << getPoint(p, xIdx, yIdx + 1).x() << " "
                    << getPoint(p, xIdx, yIdx + 1).y() << " " << zIdx << "\n";
          }
          if (hasEdge(xIdx, yIdx, zIdx, frDirEnum::E)) {
            if (xIdx + 1 >= xDim) {
              cout << "Error: no edge (" << xIdx << ", " << yIdx << ", " << zIdx
                   << ", E) " << xDim << endl;
              continue;
            }
            mazeLog << "Edge: " << getPoint(p, xIdx, yIdx).x() << " "
                    << getPoint(p, xIdx, yIdx).y() << " " << zIdx << " "
                    << getPoint(p, xIdx + 1, yIdx).x() << " "
                    << getPoint(p, xIdx + 1, yIdx).y() << " " << zIdx << "\n";
          }
          if (hasEdge(xIdx, yIdx, zIdx, frDirEnum::U)) {
            if (zIdx + 1 >= zDim) {
              cout << "Error: no edge (" << xIdx << ", " << yIdx << ", " << zIdx
                   << ", U) " << zDim << endl;
              continue;
            }
            mazeLog << "Edge: " << getPoint(p, xIdx, yIdx).x() << " "
                    << getPoint(p, xIdx, yIdx).y() << " " << zIdx << " "
                    << getPoint(p, xIdx, yIdx).x() << " "
                    << getPoint(p, xIdx, yIdx).y() << " " << zIdx + 1 << "\n";
          }
        }
      }
    }
  } else {
    cout << "Error: Fail to open maze log\n";
  }
}

void FlexGridGraph::setUsedPointsForDump(
    openroad_api::net_ordering::Request* req,
    FlexMazeIdx beginMazeIdx,
    FlexMazeIdx endMazeIdx)
{
  if (beginMazeIdx.x() == endMazeIdx.x()) {
    // 垂直走线
    int x = beginMazeIdx.x();
    int z = beginMazeIdx.z();
    for (int y = beginMazeIdx.y(); y <= endMazeIdx.y(); y++) {
      int idx = getIdx(x, y, z);
      openroad_api::net_ordering::Node* node = req->mutable_nodes(idx);
      node->set_is_used(true);
    }
  } else if (beginMazeIdx.y() == endMazeIdx.y()) {
    // 水平走线
    int y = beginMazeIdx.y();
    int z = beginMazeIdx.z();
    for (int x = beginMazeIdx.x(); x <= endMazeIdx.x(); x++) {
      int idx = getIdx(x, y, z);
      openroad_api::net_ordering::Node* node = req->mutable_nodes(idx);
      node->set_is_used(true);
    }
  } else if (beginMazeIdx.x() == endMazeIdx.x()
             && beginMazeIdx.y() == endMazeIdx.y()) {
    // 跨层走线
    int x = beginMazeIdx.x();
    int y = beginMazeIdx.y();
    for (int z = beginMazeIdx.z(); z <= endMazeIdx.x(); z++) {
      int idx = getIdx(x, y, z);
      openroad_api::net_ordering::Node* node = req->mutable_nodes(idx);
      node->set_is_used(true);
    }
  }
}

void FlexGridGraph::dump(openroad_api::net_ordering::Request* req)
{
  int xDim, yDim, zDim;
  getDim(xDim, yDim, zDim);
  req->set_dim_x(xDim);
  req->set_dim_y(yDim);
  req->set_dim_z(zDim);

  for (int z = 0; z < zDim; z++) {
//    auto lNum = getLayerNum(z);
//    auto layer = getTech()->getLayer(lNum);

    for (int y = 0; y < yDim; y++) {
      for (int x = 0; x < xDim; x++) {
        openroad_api::net_ordering::Node* node = req->add_nodes();

        node->set_maze_x(x);
        node->set_maze_y(y);
        node->set_maze_z(z);

        Point pt;
        getPoint(pt, x, y);
        node->set_point_x(pt.x());
        node->set_point_y(pt.y());
        node->set_point_z(getZHeight(z));

        // 先设置部分值
        bool isBlockage = isAllBlocked(x, y, z);
        node->set_type(isBlockage ? openroad_api::net_ordering::BLOCKAGE
                                 : openroad_api::net_ordering::NORMAL);
        node->set_is_used(isBlockage);

//        frDirEnum frDirEnumAllCustom[] = {frDirEnum::E,
//                                    frDirEnum::S,
//                                    frDirEnum::W,
//                                    frDirEnum::N,
//                                    frDirEnum::U,
//                                    frDirEnum::D};
//        for (const auto dir : frDirEnumAllCustom) {
//          int cost = -1;
//          if (hasEdge(x, y, z, dir)) {
//            // TODO 参照 src/drt/src/dr/FlexGridGraph_maze.cpp:L436，那里有区分 NDR，因不清楚作用，所以暂时忽略
//            cost = getCosts(x, y, z, dir, layer);
//          }
//          costs.push_back(cost);
//        }
      }
    }
  }

  // 统计 violation
  auto gcWorker_ = drWorker_->getGCWorker();

  gcWorker_->resetTargetNet();
  gcWorker_->setEnableSurgicalFix(true);
  gcWorker_->main();
  gcWorker_->end();

  int numMarkers = 0;
  for (auto& uMarker : gcWorker_->getMarkers()) {
    auto& marker = *uMarker;
    if (drWorker_->getDrcBox().intersects(marker.getBBox())) {
      numMarkers++;
    }
  }

  unsigned long violation = numMarkers;
  unsigned long wireLength = 0;
  unsigned long via = 0;

  // 获取当前 drWorker 所有 nets 的信息
  for (auto& net : drWorker_->getNets()) {
    int pinIdx = -1;

    for (auto& pin : net->getPins()) {
      pinIdx++;

      // 获取当前 net 在 outerNetMap 中的索引
      int netIdx;
      auto outerNetMap = drWorker_->getOuterNetMap();
      for (auto it = outerNetMap.begin(); it != outerNetMap.end(); ++it) {
        if (it->second == net.get()) {
          netIdx = it->first;
          break;
        }
      }

      for (auto& ap : pin->getAccessPatterns()) {
        FlexMazeIdx mi = ap->getMazeIdx();
        int idx = getIdx(mi.x(), mi.y(), mi.z());
        openroad_api::net_ordering::Node* node = req->mutable_nodes(idx);
        node->set_type(openroad_api::net_ordering::ACCESS);
        node->set_net(netIdx);
        node->set_pin(pinIdx);
      }
    }

    for (auto& connFig : net->getRouteConnFigs()) {
      if (connFig->typeId() == drcPathSeg) {
        auto pathSeg = static_cast<drPathSeg*>(connFig.get());

        // 记录已使用的点
        FlexMazeIdx beginMazeIdx, endMazeIdx;
        std::tie(beginMazeIdx, endMazeIdx) = pathSeg->getMazeIdx();
        setUsedPointsForDump(req, beginMazeIdx, endMazeIdx);

        // 记录线长
        auto [bp, ep] = pathSeg->getPoints();
        frCoord pathSegLen = Point::manhattanDistance(ep, bp);
        wireLength += pathSegLen;
      } else if (connFig->typeId() == drcVia) {
        auto viaSeg = static_cast<drVia*>(connFig.get());

        // 记录已使用的点
        FlexMazeIdx beginMazeIdx, endMazeIdx;
        std::tie(beginMazeIdx, endMazeIdx) = viaSeg->getMazeIdx();
        setUsedPointsForDump(req, beginMazeIdx, endMazeIdx);

        // 记录过孔数量
        via++;
      }
    }

    req->set_reward_violation(violation);
    req->set_reward_wire_length(wireLength);
    req->set_reward_via(via);
  }

  drWorker_->getLogger()->info(DRT,
                998,
                "Current violation {}, wireLength {}, via {}.",
                violation,
                wireLength,
                via);
}
