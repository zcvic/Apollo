/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/dreamview/backend/map/map_service.h"

#include <algorithm>

#include "modules/common/util/points_downsampler.h"
#include "modules/common/util/string_util.h"

namespace apollo {
namespace dreamview {

using apollo::common::PointENU;
using apollo::common::util::DownsampleByAngle;
using apollo::hdmap::Map;
using apollo::hdmap::Id;
using apollo::hdmap::LaneInfoConstPtr;
using apollo::hdmap::CrosswalkInfoConstPtr;
using apollo::hdmap::JunctionInfoConstPtr;
using apollo::hdmap::SignalInfoConstPtr;
using apollo::hdmap::StopSignInfoConstPtr;
using apollo::hdmap::YieldSignInfoConstPtr;
using apollo::hdmap::Path;
using apollo::hdmap::MapPathPoint;
using apollo::routing::RoutingResponse;
using apollo::routing::RoutingRequest;

namespace {

template <typename MapElementInfoConstPtr>
void ExtractIds(const std::vector<MapElementInfoConstPtr> &items,
                std::vector<std::string> *ids) {
  ids->reserve(items.size());
  for (const auto &item : items) {
    ids->push_back(item->id().id());
  }
  // The output is sorted so that the calculated hash will be
  // invariant to the order of elements.
  std::sort(ids->begin(), ids->end());
}

template <typename MapElementInfoConstPtr>
void ExtractOverlapIds(const std::vector<MapElementInfoConstPtr> &items,
                       std::vector<std::string> *ids) {
  for (const auto &item : items) {
    for (auto &overlap_id : item->signal().overlap_id()) {
      ids->push_back(overlap_id.id());
    }
  }
  // The output is sorted so that the calculated hash will be
  // invariant to the order of elements.
  std::sort(ids->begin(), ids->end());
}

void ExtractStringVectorFromJson(const nlohmann::json &json_object,
                                 const std::string &key,
                                 std::vector<std::string> *result) {
  auto iter = json_object.find(key);
  if (iter != json_object.end()) {
    result->reserve(iter->size());
    for (size_t i = 0; i < iter->size(); ++i) {
      result->push_back((*iter)[i]);
    }
  }
}

}  // namespace

MapElementIds::MapElementIds(const nlohmann::json &json_object)
    : MapElementIds() {
  ExtractStringVectorFromJson(json_object, "lane", &lane);
  ExtractStringVectorFromJson(json_object, "crosswalk", &crosswalk);
  ExtractStringVectorFromJson(json_object, "junction", &junction);
  ExtractStringVectorFromJson(json_object, "signal", &signal);
  ExtractStringVectorFromJson(json_object, "stopSign", &stop_sign);
  ExtractStringVectorFromJson(json_object, "yield", &yield);
  ExtractStringVectorFromJson(json_object, "overlap", &overlap);
}

size_t MapElementIds::Hash() const {
  static std::hash<std::string> hash_function;
  const std::string text = apollo::common::util::StrCat(
      apollo::common::util::PrintIter(lane, ""),
      apollo::common::util::PrintIter(crosswalk, ""),
      apollo::common::util::PrintIter(junction, ""),
      apollo::common::util::PrintIter(signal, ""),
      apollo::common::util::PrintIter(stop_sign, ""),
      apollo::common::util::PrintIter(yield, ""),
      apollo::common::util::PrintIter(overlap, ""));
  return hash_function(text);
}

nlohmann::json MapElementIds::Json() const {
  nlohmann::json result;
  result["lane"] = lane;
  result["crosswalk"] = crosswalk;
  result["junction"] = junction;
  result["signal"] = signal;
  result["stopSign"] = stop_sign;
  result["yield"] = yield;
  result["overlap"] = overlap;
  return result;
}

MapService::MapService(const std::string map_filename)
    : MapService(map_filename, map_filename) {}

MapService::MapService(const std::string &base_map_filename,
                       const std::string &sim_map_filename)
    : pnc_map_(base_map_filename) {
  CHECK(sim_map_.LoadMapFromFile(sim_map_filename) == 0)
      << "Failed to load sim_map from " << sim_map_filename;
}

MapElementIds MapService::CollectMapElementIds(const PointENU &point,
                                               double radius) const {
  MapElementIds result;

  std::vector<LaneInfoConstPtr> lanes;
  if (sim_map_.GetLanes(point, radius, &lanes) != 0) {
    AERROR << "Fail to get lanes from sim_map.";
  }
  ExtractIds(lanes, &result.lane);

  std::vector<CrosswalkInfoConstPtr> crosswalks;
  if (sim_map_.GetCrosswalks(point, radius, &crosswalks) != 0) {
    AERROR << "Fail to get crosswalks from sim_map.";
  }
  ExtractIds(crosswalks, &result.crosswalk);

  std::vector<JunctionInfoConstPtr> junctions;
  if (sim_map_.GetJunctions(point, radius, &junctions) != 0) {
    AERROR << "Fail to get junctions from sim_map.";
  }
  ExtractIds(junctions, &result.junction);

  std::vector<SignalInfoConstPtr> signals;
  if (sim_map_.GetSignals(point, radius, &signals) != 0) {
    AERROR << "Failed to get signals from sim_map.";
  }

  ExtractIds(signals, &result.signal);
  ExtractOverlapIds(signals, &result.overlap);

  std::vector<StopSignInfoConstPtr> stop_signs;
  if (sim_map_.GetStopSigns(point, radius, &stop_signs) != 0) {
    AERROR << "Failed to get stop signs from sim_map.";
  }
  ExtractIds(stop_signs, &result.stop_sign);

  std::vector<YieldSignInfoConstPtr> yield_signs;
  if (sim_map_.GetYieldSigns(point, radius, &yield_signs) != 0) {
    AERROR << "Failed to get yield signs from sim_map.";
  }
  ExtractIds(yield_signs, &result.yield);

  return result;
}

Map MapService::RetrieveMapElements(const MapElementIds &ids) const {
  Map result;
  Id map_id;

  for (const auto &id : ids.lane) {
    map_id.set_id(id);
    auto element = sim_map_.GetLaneById(map_id);
    if (element) {
      *result.add_lane() = element->lane();
    }
  }

  for (const auto &id : ids.crosswalk) {
    map_id.set_id(id);
    auto element = sim_map_.GetCrosswalkById(map_id);
    if (element) {
      *result.add_crosswalk() = element->crosswalk();
    }
  }

  for (const auto &id : ids.junction) {
    map_id.set_id(id);
    auto element = sim_map_.GetJunctionById(map_id);
    if (element) {
      *result.add_junction() = element->junction();
    }
  }

  for (const auto &id : ids.signal) {
    map_id.set_id(id);
    auto element = sim_map_.GetSignalById(map_id);
    if (element) {
      *result.add_signal() = element->signal();
    }
  }

  for (const auto &id : ids.stop_sign) {
    map_id.set_id(id);
    auto element = sim_map_.GetStopSignById(map_id);
    if (element) {
      *result.add_stop_sign() = element->stop_sign();
    }
  }

  for (const auto &id : ids.yield) {
    map_id.set_id(id);
    auto element = sim_map_.GetYieldSignById(map_id);
    if (element) {
      *result.add_yield() = element->yield_sign();
    }
  }

  for (const auto &id : ids.overlap) {
    map_id.set_id(id);
    auto element = sim_map_.GetOverlapById(map_id);
    if (element) {
      *result.add_overlap() = element->overlap();
    }
  }

  return result;
}

bool MapService::GetNearestLane(const double x, const double y,
                                LaneInfoConstPtr *nearest_lane,
                                double *nearest_s, double *nearest_l) const {
  PointENU point;
  point.set_x(x);
  point.set_y(y);
  if (BaseMap().GetNearestLane(point, nearest_lane, nearest_s, nearest_l) < 0) {
    AERROR << "Failed to get nearest lane!";
    return false;
  }
  return true;
}

bool MapService::GetPointsFromRouting(const RoutingResponse &routing,
                                      std::vector<MapPathPoint> *points) const {
  Path path;
  if (!pnc_map_.CreatePathFromRouting(routing, &path)) {
    AERROR << "Unable to get points from routing!";
    return false;
  }

  constexpr double angle_threshold = 0.1;  // threshold is about 5.72 degree.
  std::vector<int> sampled_indices =
      DownsampleByAngle(path.path_points(), angle_threshold);
  for (int index : sampled_indices) {
    points->push_back(path.path_points()[index]);
  }

  return true;
}

bool MapService::GetPoseWithRegardToLane(const double x, const double y,
                                         double *theta, double *s) const {
  double l;
  LaneInfoConstPtr nearest_lane;
  if (!GetNearestLane(x, y, &nearest_lane, s, &l)) {
    return false;
  }

  *theta = nearest_lane->Heading(*s);
  return true;
}

bool MapService::ConstructLaneWayPoint(
    const double x, const double y,
    RoutingRequest::LaneWaypoint *laneWayPoint) const {
  double s, l;
  LaneInfoConstPtr lane;
  if (!GetNearestLane(x, y, &lane, &s, &l)) {
    return false;
  }

  laneWayPoint->set_id(lane->id().id());
  laneWayPoint->set_s(s);
  auto *pose = laneWayPoint->mutable_pose();
  pose->set_x(x);
  pose->set_y(y);

  return true;
}

bool MapService::GetStartPoint(apollo::common::PointENU *start_point) const {
  // Start from origin to find a lane from the map.
  double s, l;
  LaneInfoConstPtr lane;
  if (!GetNearestLane(0.0, 0.0, &lane, &s, &l)) {
    return false;
  }

  *start_point = lane->GetSmoothPoint(0.0);
  return true;
}

}  // namespace dreamview
}  // namespace apollo
