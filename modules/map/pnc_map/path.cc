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

#include "modules/map/pnc_map/path.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <unordered_map>

#include "modules/common/math/line_segment2d.h"
#include "modules/common/math/math_utils.h"
#include "modules/common/math/polygon2d.h"
#include "modules/common/math/vec2d.h"

namespace apollo {
namespace hdmap {

using apollo::common::math::LineSegment2d;
using apollo::common::math::Polygon2d;
using apollo::common::math::Vec2d;
using apollo::common::math::Box2d;
using apollo::common::math::kMathEpsilon;
using apollo::common::math::Sqr;
using std::placeholders::_1;

namespace {

const double kSampleDistance = 0.25;

bool find_lane_segment(const PathPoint& p1, const PathPoint& p2,
                       LaneSegment* const lane_segment) {
  for (const auto& wp1 : p1.lane_waypoints()) {
    for (const auto& wp2 : p2.lane_waypoints()) {
      if (wp1.lane->id().id() == wp2.lane->id().id() && wp1.s < wp2.s) {
        *lane_segment = LaneSegment(wp1.lane, wp1.s, wp2.s);
        return true;
      }
    }
  }
  return false;
}

}  // namespace

std::string LaneWaypoint::debug_string() const {
  if (lane == nullptr) {
    return "(lane is null)";
  }
  std::ostringstream sout;
  sout << "id = " << lane->id().id() << "  s = " << s;
  sout.flush();
  return sout.str();
}

std::string LaneSegment::debug_string() const {
  if (lane == nullptr) {
    return "(lane is null)";
  }
  std::ostringstream sout;
  sout << "id = " << lane->id().id() << "  start_s = " << start_s
       << "  end_s = " << end_s;
  sout.flush();
  return sout.str();
}

std::string PathPoint::debug_string() const {
  std::ostringstream sout;
  sout << "x = " << x_ << "  y = " << y_ << "  heading = " << _heading
       << "  lwp = {";
  bool first_lwp = true;
  for (const auto& lwp : _lane_waypoints) {
    sout << "(" << lwp.debug_string() << ")";
    if (!first_lwp) {
      sout << ", ";
      first_lwp = false;
    }
  }
  sout << "}";
  sout.flush();
  return sout.str();
}

std::string Path::debug_string() const {
  std::ostringstream sout;
  sout << "num_points = " << _num_points << "  points = {";
  bool first_point = true;
  for (const auto& point : _path_points) {
    sout << "(" << point.debug_string() << ")";
    if (!first_point) {
      sout << ", ";
      first_point = false;
    }
  }
  sout << "}  num_lane_segments = " << _lane_segments.size()
       << "  lane_segments = {";
  bool first_segment = true;
  for (const auto& segment : _lane_segments) {
    sout << "(" << segment.debug_string() << ")";
    if (!first_segment) {
      sout << ", ";
      first_segment = false;
    }
  }
  sout << "}";
  sout.flush();
  return sout.str();
}

std::string PathOverlap::debug_string() const {
  std::ostringstream sout;
  sout << object_id << " " << start_s << " " << end_s;
  sout.flush();
  return sout.str();
}

Path::Path(std::vector<PathPoint> path_points)
    : _path_points(std::move(path_points)) {
  init();
}

Path::Path(std::vector<PathPoint> path_points,
           std::vector<LaneSegment> lane_segments)
    : _path_points(std::move(path_points)),
      _lane_segments(std::move(lane_segments)) {
  init();
}

Path::Path(std::vector<PathPoint> path_points,
           std::vector<LaneSegment> lane_segments,
           const double max_approximation_error)
    : _path_points(std::move(path_points)),
      _lane_segments(std::move(lane_segments)) {
  init();
  if (max_approximation_error > 0.0) {
    _use_path_approximation = true;
    _approximation = PathApproximation(*this, max_approximation_error);
  }
}

void Path::init() {
  init_points();
  init_lane_segments();
  init_point_index();
  init_width();
  //  init_overlaps();
}

void Path::init_points() {
  _num_points = static_cast<int>(_path_points.size());
  CHECK_GE(_num_points, 2);

  _accumulated_s.clear();
  _accumulated_s.reserve(_num_points);
  _segments.clear();
  _segments.reserve(_num_points);
  _unit_directions.clear();
  _unit_directions.reserve(_num_points);
  double s = 0.0;
  for (int i = 0; i < _num_points; ++i) {
    _accumulated_s.push_back(s);
    Vec2d heading;
    if (i + 1 >= _num_points) {
      heading = _path_points[i] - _path_points[i - 1];
    } else {
      _segments.emplace_back(_path_points[i], _path_points[i + 1]);
      heading = _path_points[i + 1] - _path_points[i];
      // TODO(lianglia_apollo):
      // use heading.length when all adjacent lanes are guarantee to be
      // connected.
      LaneSegment lane_segment;
      if (find_lane_segment(_path_points[i], _path_points[i + 1],
                            &lane_segment)) {
        s += lane_segment.end_s - lane_segment.start_s;
      } else {
        s += heading.Length();
      }
    }
    heading.Normalize();
    _unit_directions.push_back(heading);
  }
  _length = s;
  _num_sample_points = static_cast<int>(_length / kSampleDistance) + 1;
  _num_segments = _num_points - 1;

  CHECK_EQ(_accumulated_s.size(), _num_points);
  CHECK_EQ(_unit_directions.size(), _num_points);
  CHECK_EQ(_segments.size(), _num_segments);
}

void Path::init_lane_segments() {
  if (_lane_segments.empty()) {
    _lane_segments.reserve(_num_points);
    for (int i = 0; i + 1 < _num_points; ++i) {
      LaneSegment lane_segment;
      if (find_lane_segment(_path_points[i], _path_points[i + 1],
                            &lane_segment)) {
        _lane_segments.push_back(lane_segment);
      }
    }
  }

  _lane_segments_to_next_point.clear();
  _lane_segments_to_next_point.reserve(_num_points);
  for (int i = 0; i + 1 < _num_points; ++i) {
    LaneSegment lane_segment;
    if (find_lane_segment(_path_points[i], _path_points[i + 1],
                          &lane_segment)) {
      _lane_segments_to_next_point.push_back(lane_segment);
    } else {
      _lane_segments_to_next_point.push_back(LaneSegment());
    }
  }
  CHECK_EQ(_lane_segments_to_next_point.size(), _num_segments);
}

void Path::init_width() {
  _left_width.clear();
  _left_width.reserve(_num_sample_points);
  _right_width.clear();
  _right_width.reserve(_num_sample_points);

  double s = 0;
  for (int i = 0; i < _num_sample_points; ++i) {
    const PathPoint point = get_smooth_point(s);
    if (point.lane_waypoints().empty()) {
      _left_width.push_back(0.0);
      _right_width.push_back(0.0);
    } else {
      const LaneWaypoint waypoint = point.lane_waypoints()[0];
      CHECK_NOTNULL(waypoint.lane);
      double left_width = 0.0;
      double right_width = 0.0;
      waypoint.lane->get_width(waypoint.s, &left_width, &right_width);
      _left_width.push_back(left_width);
      _right_width.push_back(right_width);
    }
    s += kSampleDistance;
  }
  CHECK_EQ(_left_width.size(), _num_sample_points);
  CHECK_EQ(_right_width.size(), _num_sample_points);
}

void Path::init_point_index() {
  _last_point_index.clear();
  _last_point_index.reserve(_num_sample_points);
  double s = 0.0;
  int last_index = 0;
  for (int i = 0; i < _num_sample_points; ++i) {
    while (last_index + 1 < _num_points &&
           _accumulated_s[last_index + 1] <= s) {
      ++last_index;
    }
    _last_point_index.push_back(last_index);
    s += kSampleDistance;
  }
  CHECK_EQ(_last_point_index.size(), _num_sample_points);
}

/*
void Path::get_all_overlaps(
    GetOverlapFromLaneFunc get_overlaps_from_lane,
    std::vector<PathOverlap>* const overlaps) const {
  if (overlaps == nullptr) {
    return;
  }
  overlaps->clear();
  std::unordered_map<std::string, std::vector<std::pair<double, double>>>
      overlaps_by_id;
  double s = 0.0;
  for (const auto& lane_segment : _lane_segments) {
    if (lane_segment.lane == nullptr) {
      continue;
    }
    for (const OverlapInfo* overlap :
         get_overlaps_from_lane(*lane_segment.lane)) {
      const auto* overlap_info =
          overlap->get_object_overlap_info(lane_segment.lane->id());
      if (overlap_info == nullptr) {
        continue;
      }
      const auto& lane_overlap_info = overlap_info->lane_overlap_info();
      if (lane_overlap_info.start_s() < lane_segment.end_s &&
          lane_overlap_info.end_s() > lane_segment.start_s) {
        const double ref_s = s - lane_segment.start_s;
        const double adjusted_start_s =
            std::max(lane_overlap_info.start_s(), lane_segment.start_s) + ref_s;
        const double adjusted_end_s =
            std::min(lane_overlap_info.end_s(), lane_segment.end_s) + ref_s;
        for (const auto& object : overlap->overlap().object()) {
          if (object.id().id() != lane_segment.lane->id().id()) {
            overlaps_by_id[object.id().id()].emplace_back(adjusted_start_s,
                                                          adjusted_end_s);
          }
        }
      }
    }
    s += lane_segment.end_s - lane_segment.start_s;
  }
  for (auto& overlaps_one_object : overlaps_by_id) {
    const std::string& object_id = overlaps_one_object.first;
    auto& segments = overlaps_one_object.second;
    std::sort(segments.begin(), segments.end());

    const double kMinOverlapDistanceGap = 1.5;  // in meters.
    for (const auto& segment : segments) {
      if (!overlaps->empty() && overlaps->back().object_id == object_id &&
          segment.first - overlaps->back().end_s <= kMinOverlapDistanceGap) {
        overlaps->back().end_s =
            std::max(overlaps->back().end_s, segment.second);
      } else {
        overlaps->emplace_back(object_id, segment.first, segment.second);
      }
    }
  }
  std::sort(
      overlaps->begin(), overlaps->end(),
      [](const PathOverlap& overlap1, const PathOverlap& overlap2) {
        return overlap1.start_s < overlap2.start_s;
      });
}
*/

void Path::init_overlaps() {
  // TODO: implement this function. (Liangliang)
  /*
  get_all_overlaps(std::bind(&LaneInfo::cross_lanes, _1), &_lane_overlaps);
  get_all_overlaps(std::bind(&LaneInfo::signals, _1), &_signal_overlaps);
  get_all_overlaps(std::bind(&LaneInfo::yield_signs, _1),
                   &_yield_sign_overlaps);
  get_all_overlaps(std::bind(&LaneInfo::stop_signs, _1), &_stop_sign_overlaps);
  get_all_overlaps(std::bind(&LaneInfo::crosswalks, _1), &_crosswalk_overlaps);
  get_all_overlaps(std::bind(&LaneInfo::parking_spaces, _1),
                   &_parking_space_overlaps);
  get_all_overlaps(std::bind(&LaneInfo::junctions, _1), &_junction_overlaps);
  get_all_overlaps(std::bind(&LaneInfo::speed_bumps, _1),
                   &_speed_bump_overlaps);
  */
}

PathPoint Path::get_smooth_point(const InterpolatedIndex& index) const {
  CHECK_GE(index.id, 0);
  CHECK_LT(index.id, _num_points);

  const PathPoint& ref_point = _path_points[index.id];
  if (std::abs(index.offset) > kMathEpsilon) {
    const Vec2d delta = _unit_directions[index.id] * index.offset;
    PathPoint point({ref_point.x() + delta.x(), ref_point.y() + delta.y()},
                    ref_point.heading());
    if (index.id < _num_segments) {
      const LaneSegment& lane_segment = _lane_segments_to_next_point[index.id];
      if (lane_segment.lane != nullptr) {
        point.add_lane_waypoint(LaneWaypoint(
            lane_segment.lane, lane_segment.start_s + index.offset));
      }
    }
    return point;
  } else {
    return ref_point;
  }
}

PathPoint Path::get_smooth_point(double s) const {
  return get_smooth_point(get_index_from_s(s));
}

double Path::get_s_from_index(const InterpolatedIndex& index) const {
  if (index.id < 0) {
    return 0.0;
  }
  if (index.id >= _num_points) {
    return _length;
  }
  return _accumulated_s[index.id] + index.offset;
}

InterpolatedIndex Path::get_index_from_s(double s) const {
  if (s <= 0.0) {
    return {0, 0.0};
  }
  CHECK_GT(_num_points, 0);
  if (s >= _length) {
    return {_num_points - 1, 0.0};
  }
  const int sample_id = static_cast<int>(s / kSampleDistance);
  if (sample_id >= _num_sample_points) {
    return {_num_points - 1, 0.0};
  }
  const int next_sample_id = sample_id + 1;
  int low = _last_point_index[sample_id];
  int high = (next_sample_id < _num_sample_points
                  ? std::min(_num_points, _last_point_index[next_sample_id] + 1)
                  : _num_points);
  while (low + 1 < high) {
    const int mid = (low + high) / 2;
    if (_accumulated_s[mid] <= s) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return {low, s - _accumulated_s[low]};
}

bool Path::get_nearest_point(const Vec2d& point, double* accumulate_s,
                             double* lateral) const {
  double distance = 0.0;
  return get_nearest_point(point, accumulate_s, lateral, &distance);
}

bool Path::get_nearest_point(const Vec2d& point, double* accumulate_s,
                             double* lateral, double* min_distance) const {
  if (!get_projection(point, accumulate_s, lateral, min_distance)) {
    return false;
  }
  if (*accumulate_s < 0.0) {
    *accumulate_s = 0.0;
    *min_distance = point.DistanceTo(_path_points[0]);
  } else if (*accumulate_s > _length) {
    *accumulate_s = _length;
    *min_distance = point.DistanceTo(_path_points.back());
  }
  return true;
}

bool Path::get_projection(const apollo::common::math::Vec2d& point,
                          double* accumulate_s, double* lateral) const {
  double distance = 0.0;
  return get_projection(point, accumulate_s, lateral, &distance);
}

bool Path::get_projection(const Vec2d& point, double* accumulate_s,
                          double* lateral, double* min_distance) const {
  if (_segments.empty()) {
    return false;
  }
  if (accumulate_s == nullptr || lateral == nullptr ||
      min_distance == nullptr) {
    return false;
  }
  if (_use_path_approximation) {
    return _approximation.get_projection(*this, point, accumulate_s, lateral,
                                         min_distance);
  }
  CHECK_GE(_num_points, 2);
  *min_distance = std::numeric_limits<double>::infinity();

  for (int i = 0; i < _num_segments; ++i) {
    const auto& segment = _segments[i];
    const double distance = segment.DistanceTo(point);
    if (distance < *min_distance) {
      const double proj = segment.ProjectOntoUnit(point);
      if (proj < 0.0 && i > 0) {
        continue;
      }
      if (proj > segment.length() && i + 1 < _num_segments) {
        const auto& next_segment = _segments[i + 1];
        if ((point - next_segment.start())
                .InnerProd(next_segment.unit_direction()) >= 0.0) {
          continue;
        }
      }
      *min_distance = distance;
      if (i + 1 >= _num_segments) {
        *accumulate_s = _accumulated_s[i] + proj;
      } else {
        *accumulate_s = _accumulated_s[i] + std::min(proj, segment.length());
      }
      const double prod = segment.ProductOntoUnit(point);
      if ((i == 0 && proj < 0.0) ||
          (i + 1 == _num_segments && proj > segment.length())) {
        *lateral = prod;
      } else {
        *lateral = (prod > 0.0 ? distance : -distance);
      }
    }
  }
  return true;
}

bool Path::get_heading_along_path(const Vec2d& point, double* heading) const {
  if (heading == nullptr) {
    return false;
  }
  double s = 0;
  double l = 0;
  if (get_projection(point, &s, &l)) {
    *heading = get_smooth_point(s).heading();
    return true;
  }
  return false;
}

double Path::get_left_width(const double s) const {
  return get_sample(_left_width, s);
}

double Path::get_right_width(const double s) const {
  return get_sample(_right_width, s);
}

bool Path::get_width(const double s, double* left_width,
                     double* right_width) const {
  CHECK_NOTNULL(left_width);
  CHECK_NOTNULL(right_width);

  if (s < 0.0 || s > _length) {
    return false;
  }
  *left_width = get_sample(_left_width, s);
  *right_width = get_sample(_right_width, s);
  return true;
}

double Path::get_sample(const std::vector<double>& samples,
                        const double s) const {
  if (samples.empty()) {
    return 0.0;
  }
  if (s <= 0.0) {
    return samples[0];
  }
  const int idx = static_cast<int>(s / kSampleDistance);
  if (idx >= _num_sample_points - 1) {
    return samples.back();
  }
  const double ratio = (s - idx * kSampleDistance) / kSampleDistance;
  return samples[idx] * (1.0 - ratio) + samples[idx + 1] * ratio;
}

bool Path::is_on_path(const Vec2d& point) const {
  double accumulate_s = 0.0;
  double lateral = 0.0;
  if (!get_projection(point, &accumulate_s, &lateral)) {
    return false;
  }
  double left_width = 0.0;
  double right_width = 0.0;
  if (!get_width(accumulate_s, &left_width, &right_width)) {
    return false;
  }
  if (lateral < left_width && lateral > -right_width) {
    return true;
  }
  return false;
}

bool Path::is_on_path(const Box2d& box) const {
  std::vector<Vec2d> corners;
  box.GetAllCorners(&corners);
  for (const auto& corner : corners) {
    if (!is_on_path(corner)) {
      return false;
    }
  }
  return true;
}

bool Path::overlap_with(const apollo::common::math::Box2d& box,
                        double width) const {
  if (_use_path_approximation) {
    return _approximation.overlap_with(*this, box, width);
  }
  const Vec2d center = box.center();
  const double radius_sqr = Sqr(box.diagonal() / 2.0 + width) + kMathEpsilon;
  for (const auto& segment : _segments) {
    if (segment.DistanceSquareTo(center) > radius_sqr) {
      continue;
    }
    if (box.DistanceTo(segment) <= width + kMathEpsilon) {
      return true;
    }
  }
  return false;
}

double PathApproximation::compute_max_error(const Path& path, const int s,
                                            const int t) {
  if (s + 1 >= t) {
    return 0.0;
  }
  const auto& points = path.path_points();
  const LineSegment2d segment(points[s], points[t]);
  double max_distance_sqr = 0.0;
  for (int i = s + 1; i < t; ++i) {
    max_distance_sqr =
        std::max(max_distance_sqr, segment.DistanceSquareTo(points[i]));
  }
  return sqrt(max_distance_sqr);
}

bool PathApproximation::is_within_max_error(const Path& path, const int s,
                                            const int t) {
  if (s + 1 >= t) {
    return true;
  }
  const auto& points = path.path_points();
  const LineSegment2d segment(points[s], points[t]);
  for (int i = s + 1; i < t; ++i) {
    if (segment.DistanceSquareTo(points[i]) > _max_sqr_error) {
      return false;
    }
  }
  return true;
}

void PathApproximation::init(const Path& path) {
  init_dilute(path);
  init_projections(path);
}

void PathApproximation::init_dilute(const Path& path) {
  const int num_original_points = path.num_points();
  _original_ids.clear();
  int last_idx = 0;
  while (last_idx < num_original_points - 1) {
    _original_ids.push_back(last_idx);
    int next_idx = last_idx + 1;
    int delta = 2;
    for (; last_idx + delta < num_original_points; delta *= 2) {
      if (!is_within_max_error(path, last_idx, last_idx + delta)) {
        break;
      }
      next_idx = last_idx + delta;
    }
    for (; delta > 0; delta /= 2) {
      if (next_idx + delta < num_original_points &&
          is_within_max_error(path, last_idx, next_idx + delta)) {
        next_idx += delta;
      }
    }
    last_idx = next_idx;
  }
  _original_ids.push_back(last_idx);
  _num_points = static_cast<int>(_original_ids.size());
  if (_num_points == 0) {
    return;
  }

  _segments.clear();
  _segments.reserve(_num_points - 1);
  for (int i = 0; i < _num_points - 1; ++i) {
    _segments.emplace_back(path.path_points()[_original_ids[i]],
                           path.path_points()[_original_ids[i + 1]]);
  }
  _max_error_per_segment.clear();
  _max_error_per_segment.reserve(_num_points - 1);
  for (int i = 0; i < _num_points - 1; ++i) {
    _max_error_per_segment.push_back(
        compute_max_error(path, _original_ids[i], _original_ids[i + 1]));
  }
}

void PathApproximation::init_projections(const Path& path) {
  if (_num_points == 0) {
    return;
  }
  _projections.clear();
  _projections.reserve(_segments.size() + 1);
  double s = 0.0;
  _projections.push_back(0);
  for (const auto& segment : _segments) {
    s += segment.length();
    _projections.push_back(s);
  }
  const auto& original_points = path.path_points();
  const int num_original_points = original_points.size();
  _original_projections.clear();
  _original_projections.reserve(num_original_points);
  for (size_t i = 0; i < _projections.size(); ++i) {
    _original_projections.push_back(_projections[i]);
    if (i + 1 < _projections.size()) {
      const auto& segment = _segments[i];
      for (int idx = _original_ids[i] + 1; idx < _original_ids[i + 1]; ++idx) {
        const double proj = segment.ProjectOntoUnit(original_points[idx]);
        _original_projections.push_back(
            _projections[i] + std::max(0.0, std::min(proj, segment.length())));
      }
    }
  }

  // max_p_to_left[i] = max(p[0], p[1], ... p[i]).
  _max_original_projections_to_left.resize(num_original_points);
  double last_projection = -std::numeric_limits<double>::infinity();
  for (int i = 0; i < num_original_points; ++i) {
    last_projection = std::max(last_projection, _original_projections[i]);
    _max_original_projections_to_left[i] = last_projection;
  }
  for (int i = 0; i + 1 < num_original_points; ++i) {
    CHECK_LE(_max_original_projections_to_left[i],
             _max_original_projections_to_left[i + 1] + kMathEpsilon);
  }

  // min_p_to_right[i] = min(p[i], p[i + 1], ... p[size - 1]).
  _min_original_projections_to_right.resize(_original_projections.size());
  last_projection = std::numeric_limits<double>::infinity();
  for (int i = num_original_points - 1; i >= 0; --i) {
    last_projection = std::min(last_projection, _original_projections[i]);
    _min_original_projections_to_right[i] = last_projection;
  }
  for (int i = 0; i + 1 < num_original_points; ++i) {
    CHECK_LE(_min_original_projections_to_right[i],
             _min_original_projections_to_right[i + 1] + kMathEpsilon);
  }

  // Sample max_p_to_left by sample_distance.
  _max_projection = _projections.back();
  _num_projection_samples =
      static_cast<int>(_max_projection / kSampleDistance) + 1;
  _sampled_max_original_projections_to_left.clear();
  _sampled_max_original_projections_to_left.reserve(_num_projection_samples);
  double proj = 0.0;
  int last_index = 0;
  for (int i = 0; i < _num_projection_samples; ++i) {
    while (last_index + 1 < num_original_points &&
           _max_original_projections_to_left[last_index + 1] < proj) {
      ++last_index;
    }
    _sampled_max_original_projections_to_left.push_back(last_index);
    proj += kSampleDistance;
  }
  CHECK_EQ(_sampled_max_original_projections_to_left.size(),
           _num_projection_samples);
}

bool PathApproximation::get_projection(const Path& path,
                                       const apollo::common::math::Vec2d& point,
                                       double* accumulate_s, double* lateral,
                                       double* min_distance) const {
  if (_num_points == 0) {
    return false;
  }
  if (accumulate_s == nullptr || lateral == nullptr ||
      min_distance == nullptr) {
    return false;
  }
  double min_distance_sqr = std::numeric_limits<double>::infinity();
  int estimate_nearest_segment_idx = -1;
  std::vector<double> distance_sqr_to_segments;
  distance_sqr_to_segments.reserve(_segments.size());
  for (size_t i = 0; i < _segments.size(); ++i) {
    const double distance_sqr = _segments[i].DistanceSquareTo(point);
    distance_sqr_to_segments.push_back(distance_sqr);
    if (distance_sqr < min_distance_sqr) {
      min_distance_sqr = distance_sqr;
      estimate_nearest_segment_idx = i;
    }
  }
  if (estimate_nearest_segment_idx < 0) {
    return false;
  }
  const auto& original_segments = path.segments();
  const int num_original_segments = static_cast<int>(original_segments.size());
  const auto& original_accumulated_s = path.accumulated_s();
  double min_distance_sqr_with_error =
      Sqr(sqrt(min_distance_sqr) +
          _max_error_per_segment[estimate_nearest_segment_idx] + _max_error);
  *min_distance = std::numeric_limits<double>::infinity();
  int nearest_segment_idx = -1;
  for (size_t i = 0; i < _segments.size(); ++i) {
    if (distance_sqr_to_segments[i] >= min_distance_sqr_with_error) {
      continue;
    }
    int first_segment_idx = _original_ids[i];
    int last_segment_idx = _original_ids[i + 1] - 1;
    double max_original_projection = std::numeric_limits<double>::infinity();
    if (first_segment_idx < last_segment_idx) {
      const auto& segment = _segments[i];
      const double projection = segment.ProjectOntoUnit(point);
      const double prod_sqr = Sqr(segment.ProductOntoUnit(point));
      if (prod_sqr >= min_distance_sqr_with_error) {
        continue;
      }
      const double scan_distance = sqrt(min_distance_sqr_with_error - prod_sqr);
      const double min_projection = projection - scan_distance;
      max_original_projection = _projections[i] + projection + scan_distance;
      if (min_projection > 0.0) {
        const double limit = _projections[i] + min_projection;
        const int sample_index =
            std::max(0, static_cast<int>(limit / kSampleDistance));
        if (sample_index >= _num_projection_samples) {
          first_segment_idx = last_segment_idx;
        } else {
          first_segment_idx =
              std::max(first_segment_idx,
                       _sampled_max_original_projections_to_left[sample_index]);
          if (first_segment_idx >= last_segment_idx) {
            first_segment_idx = last_segment_idx;
          } else {
            while (first_segment_idx < last_segment_idx &&
                   _max_original_projections_to_left[first_segment_idx + 1] <
                       limit) {
              ++first_segment_idx;
            }
          }
        }
      }
    }
    bool min_distance_updated = false;
    bool is_within_end_point = false;
    for (int idx = first_segment_idx; idx <= last_segment_idx; ++idx) {
      if (_min_original_projections_to_right[idx] > max_original_projection) {
        break;
      }
      const auto& original_segment = original_segments[idx];
      const double x0 = point.x() - original_segment.start().x();
      const double y0 = point.y() - original_segment.start().y();
      const double ux = original_segment.unit_direction().x();
      const double uy = original_segment.unit_direction().y();
      double proj = x0 * ux + y0 * uy;
      double distance = 0.0;
      if (proj < 0.0) {
        if (is_within_end_point) {
          continue;
        }
        is_within_end_point = true;
        distance = hypot(x0, y0);
      } else if (proj <= original_segment.length()) {
        is_within_end_point = true;
        distance = std::abs(x0 * uy - y0 * ux);
      } else {
        is_within_end_point = false;
        if (idx != last_segment_idx) {
          continue;
        }
        distance = original_segment.end().DistanceTo(point);
      }
      if (distance < *min_distance) {
        min_distance_updated = true;
        *min_distance = distance;
        nearest_segment_idx = idx;
      }
    }
    if (min_distance_updated) {
      min_distance_sqr_with_error = Sqr(*min_distance + _max_error);
    }
  }
  if (nearest_segment_idx >= 0) {
    const auto& segment = original_segments[nearest_segment_idx];
    double proj = segment.ProjectOntoUnit(point);
    const double prod = segment.ProductOntoUnit(point);
    if (nearest_segment_idx > 0) {
      proj = std::max(0.0, proj);
    }
    if (nearest_segment_idx + 1 < num_original_segments) {
      proj = std::min(segment.length(), proj);
    }
    *accumulate_s = original_accumulated_s[nearest_segment_idx] + proj;
    if ((nearest_segment_idx == 0 && proj < 0.0) ||
        (nearest_segment_idx + 1 == num_original_segments &&
         proj > segment.length())) {
      *lateral = prod;
    } else {
      *lateral = (prod > 0 ? (*min_distance) : -(*min_distance));
    }
    return true;
  }
  return false;
}

bool PathApproximation::overlap_with(const Path& path, const Box2d& box,
                                     double width) const {
  if (_num_points == 0) {
    return false;
  }
  const Vec2d center = box.center();
  const double radius = box.diagonal() / 2.0 + width;
  const double radius_sqr = Sqr(radius);
  const auto& original_segments = path.segments();
  for (size_t i = 0; i < _segments.size(); ++i) {
    const LineSegment2d& segment = _segments[i];
    const double max_error = _max_error_per_segment[i];
    const double radius_sqr_with_error = Sqr(radius + max_error);
    if (segment.DistanceSquareTo(center) > radius_sqr_with_error) {
      continue;
    }
    int first_segment_idx = _original_ids[i];
    int last_segment_idx = _original_ids[i + 1] - 1;
    double max_original_projection = std::numeric_limits<double>::infinity();
    if (first_segment_idx < last_segment_idx) {
      const auto& segment = _segments[i];
      const double projection = segment.ProjectOntoUnit(center);
      const double prod_sqr = Sqr(segment.ProductOntoUnit(center));
      if (prod_sqr >= radius_sqr_with_error) {
        continue;
      }
      const double scan_distance = sqrt(radius_sqr_with_error - prod_sqr);
      const double min_projection = projection - scan_distance;
      max_original_projection = _projections[i] + projection + scan_distance;
      if (min_projection > 0.0) {
        const double limit = _projections[i] + min_projection;
        const int sample_index =
            std::max(0, static_cast<int>(limit / kSampleDistance));
        if (sample_index >= _num_projection_samples) {
          first_segment_idx = last_segment_idx;
        } else {
          first_segment_idx =
              std::max(first_segment_idx,
                       _sampled_max_original_projections_to_left[sample_index]);
          if (first_segment_idx >= last_segment_idx) {
            first_segment_idx = last_segment_idx;
          } else {
            while (first_segment_idx < last_segment_idx &&
                   _max_original_projections_to_left[first_segment_idx + 1] <
                       limit) {
              ++first_segment_idx;
            }
          }
        }
      }
    }
    for (int idx = first_segment_idx; idx <= last_segment_idx; ++idx) {
      if (_min_original_projections_to_right[idx] > max_original_projection) {
        break;
      }
      const auto& original_segment = original_segments[idx];
      if (original_segment.DistanceSquareTo(center) > radius_sqr) {
        continue;
      }
      if (box.DistanceTo(original_segment) <= width) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace hdmap
}  // namespace apollo
