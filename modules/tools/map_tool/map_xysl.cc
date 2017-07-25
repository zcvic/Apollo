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

#include <cstdio>
#include <iostream>
#include <fstream>
#include "gflags/gflags.h"

#define private public
#include "modules/map/proto/map_geometry.pb.h"
#include "modules/common/log.h"
#include "modules/map/hdmap/hdmap_common.h"
#include "modules/map/hdmap/hdmap.h"

DEFINE_string(map_file, "modules/map/data/base_map.txt", "the map file");
DEFINE_bool(xy_to_sl, false, "calculate xy to sl");
DEFINE_bool(sl_to_xy, false, "calculate sl to xy");
DEFINE_bool(xy_to_lane, false, "calculate xy to lane");
DEFINE_bool(lane_to_lane, false, "calculate lane to lane");
DEFINE_bool(lane_width, false, "calculate lane width");
DEFINE_string(print_map, "", "print the map");
DEFINE_string(print_map_bin, "", "print the map binary");
DEFINE_string(overlap, "", "get overlap information");
DEFINE_string(lane_info, "", "print lane info");
DEFINE_string(signal_info, "", "print signal info");
DEFINE_double(x, 0.0, "x");
DEFINE_double(y, 0.0, "y");
DEFINE_string(lane, "", "lane_id");
DEFINE_string(from_lane, "", "from_lane");
DEFINE_string(to_lane, "", "to_lane");
DEFINE_double(s, 0.0, "s");
DEFINE_double(l, 0.0, "l");

namespace apollo {
namespace tools {

class MapUtil {
 public:
  explicit MapUtil(const std::string &map_filename) : _map_client(nullptr) {
    _map_client.reset(new apollo::hdmap::HDMap());
    _map_client->load_map_from_file(map_filename);
  }

  static ::apollo::hdmap::Id create_id(const std::string &id) {
    ::apollo::hdmap::Id hdmap_id;
    hdmap_id.set_id(id);
    return hdmap_id;
  }

  const ::apollo::hdmap::OverlapInfo *get_overlap(
      const std::string &overlap_id) {
    ::apollo::hdmap::Id id;
    id.set_id(overlap_id);
    auto ret = _map_client->get_overlap_by_id(id);
    if (ret == nullptr) {
      fprintf(stderr, "failed to find overlap[%s]\n", overlap_id.c_str());
    }
    return ret.get();
  }

  const ::apollo::hdmap::SignalInfo *get_signal(const std::string &signal_id) {
    ::apollo::hdmap::Id id;
    id.set_id(signal_id);
    auto ret = _map_client->get_signal_by_id(id);
    if (ret == nullptr) {
      fprintf(stderr, "failed to find overlap[%s]\n", signal_id.c_str());
    }
    return ret.get();
  }

  const ::apollo::hdmap::LaneInfo *get_lane(const std::string &lane_id) {
    ::apollo::hdmap::Id id;
    id.set_id(lane_id);
    auto ret = _map_client->get_lane_by_id(id);
    if (ret == nullptr) {
      fprintf(stderr, "failed to find lane[%s]\n", lane_id.c_str());
    }
    return ret.get();
  }

  int point_to_sl(const ::apollo::hdmap::Point &point,
                  std::string *lane_id, double *s, double *l) {
    QUIT_IF(lane_id == nullptr, -1, ERROR, "arg lane id is null");
    QUIT_IF(s == nullptr, -2, ERROR, "arg s is null");
    QUIT_IF(l == nullptr, -3, ERROR, "arg l is null");
    ::apollo::hdmap::LaneInfoConstPtr lane = nullptr;
    int ret = _map_client->get_nearest_lane(point, &lane, s, l);
    QUIT_IF(ret != 0, -4, ERROR, "get_nearest_lane failed with ret[%d]", ret);
    QUIT_IF(lane == nullptr, -5, ERROR, "lane is null");
    *lane_id = lane->id().id();
    return 0;
  }

  int sl_to_point(const std::string &lane_id, const double s,
                  const double l, ::apollo::hdmap::Point *point,
                  double *heading) {
    QUIT_IF(point == nullptr, -1, ERROR, "arg point is null");
    QUIT_IF(heading == nullptr, -2, ERROR, "arg heading is null");
    ::apollo::common::math::Vec2d vec2d;
    // TODO(yifei) map api is not available
    int ret = -1;
    // int ret = _map_client.get_smooth_point_from_lane(
    //     create_id(lane_id), s, l, &vec2d, heading);
    QUIT_IF(ret != 0, -3, ERROR, "get_smooth_point_from_lane[%s] failed",
            lane_id.c_str());
    point->set_x(vec2d.x());
    point->set_y(vec2d.y());
    return 0;
  }

  int lane_projection(const ::apollo::common::math::Vec2d &vec2d,
                      const std::string &lane_id,
                      double *s, double *l) {
    QUIT_IF(s == nullptr, -1, ERROR, "arg s is nullptr");
    const ::apollo::hdmap::LaneInfo *lane =
        _map_client->get_lane_by_id(create_id(lane_id)).get();
    QUIT_IF(lane == nullptr, -2, ERROR, "get_lane_by_id[%s] failed",
            lane_id.c_str());
    // TODO(yifei) map api is not available
    bool ret = false;
    // bool ret = lane->get_projection(vec2d, s, l);
    QUIT_IF(!ret, -3, ERROR,
            "lane[%s] get projection for point[%f, %f] failed",
            lane_id.c_str(),
            vec2d.x(), vec2d.y());
    return 0;
  }
  const ::apollo::hdmap::HDMap *get_map_client() const {
    return _map_client.get();
  }

 private:
  std::unique_ptr<apollo::hdmap::HDMap> _map_client;
};

}  // namespace tools
}  // namespace apollo

std::ostream &operator<<(
    std::ostream &os,
    const ::google::protobuf::RepeatedPtrField<::apollo::hdmap::Id> &ids) {
  for (int i = 0; i < ids.size(); ++i) {
    os << ids.Get(i).id();
    if (i != ids.size() - 1) {
      os << ", ";
    }
  }
  return os;
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  std::string map_file = FLAGS_map_file;
  ::apollo::tools::MapUtil map_util(FLAGS_map_file);

  if (FLAGS_xy_to_sl) {
    double x = FLAGS_x;
    double y = FLAGS_y;
    ::apollo::hdmap::Point point;
    point.set_x(x);
    point.set_y(y);
    point.set_z(0);
    std::string lane_id;
    double s = 0.0;
    double l = 0.0;
    map_util.point_to_sl(point, &lane_id, &s, &l);
    double heading = 0.0;
    map_util.sl_to_point(lane_id, s, l, &point, &heading);
    printf("lane_id[%s], s[%f], l[%f], heading[%f]\n",
           lane_id.c_str(), s, l, heading);
  }
  if (FLAGS_sl_to_xy) {
    ::apollo::hdmap::Point point;
    double heading = 0.0;
    map_util.sl_to_point(FLAGS_lane, FLAGS_s, FLAGS_l, &point, &heading);
    printf("x[%f] y[%f], heading[%f]\n", point.x(), point.y(), heading);
  }
  if (FLAGS_xy_to_lane) {
    ::apollo::common::math::Vec2d vec2d(FLAGS_x, FLAGS_y);
    double s = 0.0;
    double l = 0.0;
    int ret = map_util.lane_projection(vec2d, FLAGS_lane, &s, &l);
    if (ret != 0) {
      printf("lane_projection for x[%f], y[%f], lane_id[%s] failed\n",
             FLAGS_x, FLAGS_y, FLAGS_lane.c_str());
      return -1;
    }
    printf("lane[%s] s[%f], l[%f]\n", FLAGS_lane.c_str(), s, l);
  }
  if (FLAGS_lane_to_lane) {
    ::apollo::hdmap::Point point;
    double heading = 0.0;
    map_util.sl_to_point(FLAGS_from_lane, FLAGS_s, 0.0, &point, &heading);
    double target_s = 0.0;
    double target_l = 0.0;
    ::apollo::common::math::Vec2d vec2d(point.x(), point.y());
    int ret = map_util.lane_projection(vec2d, FLAGS_to_lane,
                                       &target_s, &target_l);
    if (ret != 0) {
      printf("lane_projection for lane[%s], s[%f] to lane_id[%s] failed\n",
             FLAGS_from_lane.c_str(), FLAGS_s, FLAGS_to_lane.c_str());
      return -1;
    }
    printf("lane[%s] s[%f], l[%f]\n", FLAGS_to_lane.c_str(),
           target_s, target_l);
  }
  if (FLAGS_lane_width) {
    auto lane_ptr = map_util.get_lane(FLAGS_lane);
    double left_width = 0.0;
    double right_width = 0.0;
    lane_ptr->get_width(FLAGS_s, &left_width, &right_width);
    printf("lane[%s] s[%f]: left_width[%f], right_width[%f], total_width[%f]\n",
           FLAGS_lane.c_str(), FLAGS_s, left_width, right_width,
           left_width + right_width);
  }
  if (!FLAGS_lane_info.empty()) {
    const auto *lane_ptr = map_util.get_lane(FLAGS_lane_info);
    const auto &lane = lane_ptr->lane();

    ::apollo::hdmap::Point start_point;
    double start_heading = 0.0;
    map_util.sl_to_point(FLAGS_lane_info, 0, 0, &start_point, &start_heading);

    ::apollo::hdmap::Point end_point;
    double end_heading = 0.0;
    map_util.sl_to_point(FLAGS_lane_info, lane_ptr->total_length(), 0,
                         &end_point, &end_heading);

    std::cout << "lane[" << FLAGS_lane_info
              << "] length[" << lane_ptr->total_length()
              << "] type[" << Lane_LaneType_Name(lane.type())
              << "] turn[" << Lane_LaneTurn_Name(lane.turn())
              << "] left_boundary["
              << LaneBoundary_Type_Name(lane.left_boundary().type())
              << "] right_boundary["
              << LaneBoundary_Type_Name(lane.right_boundary().type())
              << "] speed_limit[" << lane.speed_limit()
              << "] predecessor[" << lane.predecessor_id()
              << "] successor[" << lane.successor_id()
              << "] left_forward[" << lane.left_neighbor_forward_lane_id()
              << "] right_forward[" << lane.right_neighbor_forward_lane_id()
              << "] left_reverse[" << lane.left_neighbor_reverse_lane_id()
              << "] right_reverse[" << lane.right_neighbor_reverse_lane_id()
              << "] overlap[" << lane.overlap_id()
              << "] crosswalk[" << lane.crosswalk_id()
              << "] yield_sign[" << lane.yield_sign_id()
              // TODO(yifei)
              // << "] stop_sign num:[" << lane_ptr->stop_signs().size()
              << "]"
              << " start point(x,y,heading):" << start_point.x() << ","
              << start_point.y() << "," << start_heading
              << " end point(x,y,heading):" << end_point.x() << ","
              << end_point.y() << "," << end_heading
              << std::endl;
  }
  if (!FLAGS_overlap.empty()) {
    const auto *overlap_ptr = map_util.get_overlap(FLAGS_overlap);
    if (overlap_ptr != nullptr) {
      std::cout << "overlap[" << overlap_ptr->id().id()
                << "] info[" << overlap_ptr->overlap().DebugString()
                << "]" << std::endl;
    }
  }
  if (!FLAGS_signal_info.empty()) {
    const auto *signal_ptr = map_util.get_signal(FLAGS_signal_info);
    if (signal_ptr) {
      std::cout << "signal[" << FLAGS_signal_info << "] info["
                << signal_ptr->signal().DebugString() << "]" << std::endl;
    }
  }
  if (!FLAGS_print_map.empty()) {
    // const auto *map_client = map_util.get_map_client();
    std::ofstream ofs(FLAGS_print_map);
    // TODO(yifei) map api is not available
    // ofs << map_client->_map.DebugString();
    ofs.close();
  }
  if (!FLAGS_print_map_bin.empty()) {
    // const auto *map_client = map_util.get_map_client();
    std::ofstream ofs(FLAGS_print_map_bin);
    std::string map_str;
    // TODO(yifei) map api is not available
    // map_client->_map.SerializeToString(&map_str);
    // ofs << map_str;
    ofs.close();
  }
  if (!FLAGS_sl_to_xy
      && !FLAGS_xy_to_sl
      && !FLAGS_xy_to_lane
      && !FLAGS_lane_to_lane
      && !FLAGS_lane_width
      && FLAGS_print_map.empty()
      && FLAGS_print_map_bin.empty()
      && FLAGS_lane_info.empty()
      && FLAGS_signal_info.empty()
      && FLAGS_overlap.empty()) {
    std::cout << "usage: --map_file" << std::endl;
    std::cout << "usage: --print_map" << std::endl;
    std::cout << "usage: --print_map_bin" << std::endl;
    std::cout << "usage: --xy_to_sl --x --y" << std::endl;
    std::cout << "usage: --sl_to_xy --lane --s --l" << std::endl;
    std::cout << "usage: --xy_to_lane --x --y --lane" << std::endl;
    std::cout << "usage: --lane_to_lane --from_lane --s --to_lane" << std::endl;
    std::cout << "usage: --lane_width --lane --s" << std::endl;
    std::cout << "usage: --lane_info" << std::endl;
    std::cout << "usage: --signal_info" << std::endl;
    std::cout << "usage: --overlap" << std::endl;
  }
  return 0;
}
