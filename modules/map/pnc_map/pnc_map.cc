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

/**
 * @file: pnc_map.cc
 **/

#include "modules/map/pnc_map/pnc_map.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "google/protobuf/text_format.h"

#include "modules/map/proto/map_id.pb.h"

#include "modules/common/log.h"
#include "modules/map/hdmap/hdmap_util.h"
#include "modules/map/pnc_map/path.h"

namespace apollo {
namespace hdmap {

using apollo::routing::RoutingResponse;

namespace {

// Minimum error in lane segmentation.
const double kSegmentationEpsilon = 0.2;

// Minimum distance to remove duplicated points.
const double kDuplicatedPointsEpsilon = 1e-7;

// Maximum lateral error used in trajectory approximation.
const double kTrajectoryApproximationMaxError = 2.0;

void RemoveDuplicates(std::vector<common::math::Vec2d> *points) {
  CHECK_NOTNULL(points);
  int count = 0;
  const double limit = kDuplicatedPointsEpsilon * kDuplicatedPointsEpsilon;
  for (size_t i = 0; i < points->size(); ++i) {
    if (count == 0 ||
        (*points)[i].DistanceSquareTo((*points)[count - 1]) > limit) {
      (*points)[count++] = (*points)[i];
    }
  }
  points->resize(count);
}

void RemoveDuplicates(std::vector<MapPathPoint> *points) {
  CHECK_NOTNULL(points);
  int count = 0;
  const double limit = kDuplicatedPointsEpsilon * kDuplicatedPointsEpsilon;
  for (size_t i = 0; i < points->size(); ++i) {
    if (count == 0 ||
        (*points)[i].DistanceSquareTo((*points)[count - 1]) > limit) {
      (*points)[count++] = (*points)[i];
    } else {
      (*points)[count - 1].add_lane_waypoints((*points)[i].lane_waypoints());
    }
  }
  points->resize(count);
}

}  // namespace

PncMap::PncMap(const std::string &map_file) {
  CHECK(!hdmap_.LoadMapFromFile(map_file)) << "Failed to load map file:"
                                           << map_file;
  AINFO << "map loaded, Map file: " << map_file;
}

bool PncMap::ValidateRouting(const RoutingResponse &routing) const {
  const int num_road = routing.road_size();
  if (num_road == 0) {
    AERROR << "Route is empty.";
    return false;
  }
  return true;
}

bool PncMap::GetNearestPointFromRouting(const RoutingResponse &routing,
                                        const common::PointENU &point,
                                        LaneWaypoint *waypoint) const {
  const double kMaxDistance = 20.0;  // meters.
  std::vector<LaneInfoConstPtr> lanes;
  const int status = hdmap_.GetLanes(point, kMaxDistance, &lanes);
  if (status < 0) {
    AERROR << "failed to get lane from point " << point.DebugString();
    return false;
  }
  // get all lanes from routing
  std::unordered_set<std::string> routing_lane_ids;
  for (const auto &road_segment : routing.road()) {
    for (const auto &passage : road_segment.passage()) {
      for (const auto &segment : passage.segment()) {
        routing_lane_ids.insert(segment.id());
      }
    }
  }
  // get nearest_wayponints for current position
  double min_distance = std::numeric_limits<double>::infinity();
  std::vector<LaneWaypoint> nearest_waypoints;
  for (const auto lane : lanes) {
    if (!routing_lane_ids.count(lane->id().id())) {
      continue;
    }
    double distance = 0.0;
    common::PointENU map_point =
        lane->GetNearestPoint({point.x(), point.y()}, &distance);
    if (distance < min_distance) {
      min_distance = distance;
      double s = 0.0;
      double l = 0.0;
      if (!lane->GetProjection({map_point.x(), map_point.y()}, &s, &l)) {
        AERROR << "Failed to get projection for map_point "
               << map_point.DebugString();
        return false;
      }
      nearest_waypoints.emplace_back(lane, s);
    }
  }
  if (nearest_waypoints.empty()) {
    AERROR << "Failed to find point on routing. Point:" << point.DebugString();
    return false;
  }
  *waypoint = nearest_waypoints.back();
  return true;
}

bool PncMap::GetLaneSegmentsFromRouting(
    const RoutingResponse &routing, const common::PointENU &point,
    const double backward_length, const double forward_length,
    std::vector<LaneSegments> *const route_segments) const {
  if (route_segments == nullptr) {
    AERROR << "the provided proute_segments is null";
    return false;
  }
  if (backward_length < 0.0 || forward_length < 0.0 ||
      backward_length + forward_length <= 0.0) {
    AERROR << "backward_length[" << backward_length << "] or forward_length["
           << forward_length << "] are invalid";
    return false;
  }
  if (!ValidateRouting(routing)) {
    AERROR << "The provided routing result is invalid";
    return false;
  }
  route_segments->clear();
  LaneWaypoint start_waypoint;
  if (!GetNearestPointFromRouting(routing, point, &start_waypoint)) {
    AERROR << "Failed to find nearest proint: " << point.ShortDebugString()
           << " routing";
  }
  double min_overlap_distance = std::numeric_limits<double>::infinity();
  double proj_s = 0.0;
  double accumulate_s = 0.0;
  LaneSegments connected_lanes;
  LaneSegments cropped_lanes;
  for (const auto &road_segment : routing.road()) {
    for (const auto &passage : road_segment.passage()) {
      for (const auto &lane_segment : passage.segment()) {
        const double length = lane_segment.end_s() - lane_segment.start_s();
        auto lane = hdmap_.GetLaneById(MakeMapId(lane_segment.id()));
        if (!lane) {
          AERROR << "Failed to fine lane " << lane_segment.id();
          return false;
        }
        connected_lanes.emplace_back(lane, lane_segment.start_s(),
                                     lane_segment.end_s());
        if (lane_segment.id() == start_waypoint.lane->id().id()) {
          double overlap_distance = 0.0;
          if (start_waypoint.s < lane_segment.start_s()) {
            overlap_distance = lane_segment.start_s() - start_waypoint.s;
          } else if (start_waypoint.s > lane_segment.end_s()) {
            overlap_distance = start_waypoint.s - lane_segment.end_s();
          }
          if (overlap_distance < min_overlap_distance) {
            min_overlap_distance = overlap_distance;
            proj_s =
                accumulate_s +
                std::max(0.0, std::min(length, start_waypoint.s -
                                                   lane_segment.start_s()));
          }
        }
        accumulate_s += length;
      }
    }
  }
  if (min_overlap_distance < std::numeric_limits<double>::infinity()) {
    LaneSegments truncated_segments;
    if (TruncateLaneSegments(connected_lanes, proj_s - backward_length,
                             proj_s + forward_length, &truncated_segments)) {
      route_segments->emplace_back(std::move(truncated_segments));
      return true;
    } else {
      AERROR << "Failed to truncate lane segments";
      return false;
    }
  } else {
    AERROR << "Failed to get lanes from routing";
    return false;
  }
}

bool PncMap::TruncateLaneSegments(
    const LaneSegments &segments, double start_s, double end_s,
    LaneSegments *const truncated_segments) const {
  if (segments.empty()) {
    AERROR << "The input segments is empty";
    return false;
  }
  if (truncated_segments == nullptr) {
    AERROR << "the output truncated segments buffer is null";
    return false;
  }
  if (start_s >= end_s) {
    AERROR << "start_s(" << start_s << " >= end_s(" << end_s << ")";
    return false;
  }
  const double kRouteEpsilon = 1e-3;
  // Extend the trajectory towards the start of the trajectory.
  if (start_s < 0) {
    const auto &first_segment = segments[0];
    auto lane = first_segment.lane;
    double s = first_segment.start_s;
    double extend_s = -start_s;
    std::vector<LaneSegment> extended_lane_segments;
    while (extend_s > kRouteEpsilon) {
      if (s <= kRouteEpsilon) {
        if (lane->lane().predecessor_id_size() == 0) {
          break;
        }
        const auto &next_lane_id = lane->lane().predecessor_id(0);
        lane = hdmap_.GetLaneById(next_lane_id);
        if (lane == nullptr) {
          break;
        }
        s = lane->total_length();
      } else {
        const double length = std::min(s, extend_s);
        extended_lane_segments.emplace_back(lane, s - length, s);
        extend_s -= length;
        s -= length;
      }
    }
    truncated_segments->insert(truncated_segments->end(),
                               extended_lane_segments.rbegin(),
                               extended_lane_segments.rend());
  }
  double router_s = 0;
  for (const auto &lane_segment : segments) {
    const double adjusted_start_s = std::max(
        start_s - router_s + lane_segment.start_s, lane_segment.start_s);
    const double adjusted_end_s =
        std::min(end_s - router_s + lane_segment.start_s, lane_segment.end_s);
    if (adjusted_start_s < adjusted_end_s) {
      truncated_segments->emplace_back(lane_segment.lane, adjusted_start_s,
                                       adjusted_end_s);
    }
    router_s += (lane_segment.end_s - lane_segment.start_s);
    if (router_s > end_s) {
      break;
    }
  }
  // Extend the trajectory towards the end of the trajectory.
  if (router_s < end_s) {
    const auto &last_segment = segments.back();
    std::string last_lane_id = last_segment.lane->id().id();
    double last_s = last_segment.end_s;
    while (router_s < end_s - kRouteEpsilon) {
      const auto lane = hdmap_.GetLaneById(MakeMapId(last_lane_id));
      if (lane == nullptr) {
        break;
      }
      if (last_s >= lane->total_length() - kRouteEpsilon) {
        if (lane->lane().successor_id_size() == 0) {
          break;
        }
        last_lane_id = lane->lane().successor_id(0).id();
        last_s = 0.0;
      } else {
        const double length =
            std::min(end_s - router_s, lane->total_length() - last_s);
        truncated_segments->emplace_back(lane, last_s, last_s + length);
        router_s += length;
        last_s += length;
      }
    }
  }
  return true;
}

void PncMap::AppendLaneToPoints(LaneInfoConstPtr lane, const double start_s,
                                const double end_s,
                                std::vector<MapPathPoint> *const points) {
  if (points == nullptr || start_s >= end_s) {
    return;
  }
  double accumulate_s = 0.0;
  for (size_t i = 0; i < lane->points().size(); ++i) {
    if (accumulate_s >= start_s && accumulate_s <= end_s) {
      points->emplace_back(lane->points()[i], lane->headings()[i],
                           LaneWaypoint(lane, accumulate_s));
    }
    if (i < lane->segments().size()) {
      const auto &segment = lane->segments()[i];
      const double next_accumulate_s = accumulate_s + segment.length();
      if (start_s > accumulate_s && start_s < next_accumulate_s) {
        points->emplace_back(
            segment.start() +
                segment.unit_direction() * (start_s - accumulate_s),
            lane->headings()[i], LaneWaypoint(lane, start_s));
      }
      if (end_s > accumulate_s && end_s < next_accumulate_s) {
        points->emplace_back(
            segment.start() + segment.unit_direction() * (end_s - accumulate_s),
            lane->headings()[i], LaneWaypoint(lane, end_s));
      }
      accumulate_s = next_accumulate_s;
    }
    if (accumulate_s > end_s) {
      break;
    }
  }
}

bool PncMap::CreatePathsFromRouting(const RoutingResponse &routing,
                                    std::vector<Path> *paths) const {
  for (const auto &road : routing.road()) {
    for (const auto &passage_region : road.passage()) {
      // Each passage region in a road forms a path
      if (!AddPathFromPassageRegion(passage_region, paths)) {
        return false;
      }
    }
  }
  return true;
}

bool PncMap::AddPathFromPassageRegion(const routing::Passage &passage_region,
                                      std::vector<Path> *paths) const {
  LaneSegments segments;
  for (const auto &segment : passage_region.segment()) {
    auto lane_ptr = hdmap_.GetLaneById(MakeMapId(segment.id()));
    if (!lane_ptr) {
      AERROR << "Failed to find lane: " << segment.id();
      return false;
    }
    segments.emplace_back(lane_ptr, segment.start_s(), segment.end_s());
  }
  Path path;
  if (!CreatePathFromLaneSegments(segments, &path)) {
    return false;
  }
  paths->push_back(path);

  return true;
}

bool PncMap::CreatePathFromLaneSegments(const LaneSegments &segments,
                                        Path *const path) {
  std::vector<MapPathPoint> points;
  for (const auto &segment : segments) {
    AppendLaneToPoints(segment.lane, segment.start_s, segment.end_s, &points);
  }
  RemoveDuplicates(&points);

  if (points.size() < 2) {
    AWARN << "Cannot create path from " << points.size()
          << " points. Expecting more than 2.";
    return false;
  }

  *path = Path(points, segments, kTrajectoryApproximationMaxError);
  return true;
}

const HDMap &PncMap::HDMap() const { return hdmap_; }

}  // namespace hdmap
}  // namespace apollo
