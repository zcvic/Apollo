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
#include "modules/perception/traffic_light/onboard/output_subnode.h"

#include <cv_bridge/cv_bridge.h>
#include <gflags/gflags.h>
#include "modules/perception/lib/base/time_util.h"
#include "modules/perception/lib/base/timer.h"
#include "modules/perception/onboard/event_manager.h"
#include "modules/perception/onboard/shared_data_manager.h"
#include "modules/perception/onboard/subnode_helper.h"
#include "modules/perception/proto/traffic_light_detection.pb.h"
#include "modules/perception/traffic_light/base/utils.h"
#include "modules/perception/traffic_light/onboard/preprocessor_subnode.h"
#include "modules/perception/traffic_light/onboard/proc_data.h"

DEFINE_bool(enable_fill_lights_outside_image, false,
            "fill the lights(returned by HD-Map, while not in image) with "
            "DEFAULT_UNKNOWN_COLOR");

namespace apollo {
namespace perception {
namespace traffic_light {

using SubnodeHelper;

StatusCode TLOutputSubnode::proc_events() {
  Event event;
  const EventMeta &event_meta = sub_meta_events_[0];
  if (!event_manager_->subscribe(event_meta.event_id, &event)) {
    AERROR << "Failed to subscribe event: " << event_meta.event_id;
    return FAIL;
  }

  PERF_FUNCTION();
  Timer timer;
  timer.Start();
  if (!proc_upstream_data(event)) {
    AERROR << "TLOutputSubnode failed to proc_upstream_data. "
           << "event:" << event.to_string();
    return FAIL;
  }
  timer.End("TLOutputSubnode::proc_events");
  return SUCC;
}

bool TLOutputSubnode::init_internal() {
  if (!init_shared_data()) {
    AERROR << "TLOutputSubnode init failed. init_shared_data failed";
    return false;
  }
  if (!init_output_stream()) {
    AERROR << "TLOutputSubnode init failed. init_output_stream failed.";
    return false;
  }

  return true;
}

bool TLOutputSubnode::init_shared_data() {
  CHECK_NOTNULL(shared_data_manager_);

  const std::string proc_data_name("TLProcData");
  _proc_data = dynamic_cast<TLProcData *>(
      shared_data_manager_->GetSharedData(proc_data_name));
  if (_proc_data == nullptr) {
    AERROR << "Failed to get shared data instance " << proc_data_name;
    return false;
  }
  AINFO << "TLProcData init shared data.";
  return true;
}

bool TLOutputSubnode::init_output_stream() {
  // expect _reserve format:
  //      "traffic_light_output_stream : sink_type=m&sink_name=x;
  std::map<std::string, std::string> reserve_field_map;
  if (!SubnodeHelper::ParseReserveField(reserve_, &reserve_field_map)) {
    AERROR << "TLOutputSubnode failed to parse reserve string: " << reserve_;
    return false;
  }

  auto iter = reserve_field_map.find("traffic_light_output_stream");
  if (iter == reserve_field_map.end()) {
    AERROR << "TLOutputSubnode no output stream conf, need "
           << "key:traffic_light_output_stream. reserve:" << reserve_;
    return false;
  }

  _output_stream.reset(new StreamOutput());
  if (_output_stream == nullptr ||
      !_output_stream->register_publisher<std_msgs::String>(iter->second)) {
    AERROR << "TLOutputSubnode init publisher failed. "
           << "param:" << iter->second;
    return false;
  }
  return true;
}

bool TLOutputSubnode::proc_upstream_data(const Event &event) {
  Timer timer;
  timer.Start();
  std::string key;
  if (!SubnodeHelper::ProduceSharedDataKey(event.timestamp, event.reserve,
                                           &key)) {
    AERROR << "TLOutputSubnode Failed to produce shared data key. "
           << "event:" << event.to_string();
    return false;
  }

  std::shared_ptr<ImageLights> image_lights;
  if (!_proc_data->Get(key, &image_lights)) {
    AERROR << "TLOutputSubnode get data failed. key:" << key;
    return false;
  }

  // transform & publish message.
  boost::shared_ptr<std_msgs::String> msg(new std_msgs::String);
  if (!transform_message(event, image_lights, &msg)) {
    AERROR << "TLOutputSubnode transfrom message failed. "
           << "event:" << event.to_string();
    return false;
  }
  if (!_output_stream->publish<std_msgs::String>(msg)) {
    AERROR << "TLOutputSubnode publish message failed. "
           << "event:" << event.to_string();
    return false;
  }

  timer.End("TLOutputSubnode::proc_upstream_data");
  return true;
}

bool TLOutputSubnode::transform_message(
    const Event &event, const std::shared_ptr<ImageLights> &image_lights,
    boost::shared_ptr<std_msgs::String> *msg) const {
  Timer timer;
  timer.Start();
  const auto &lights = image_lights->lights;

  apollo::perception::TrafficLightDetection result;
  apollo::common::Header *header = result.mutable_header();
  header->set_timestamp_sec(ros::Time::now().toSec());
  uint64_t timestamp = TimestampDouble2Int64(image_lights->image->ts());
  switch (image_lights->image->camera_id()) {
    default:
    case CameraId::LONG_FOCUS:
      timestamp += 222;
      break;
    case CameraId::SHORT_FOCUS:
      timestamp += 111;
      break;
    case CameraId::NARROW_FOCUS:
      timestamp += 444;
      break;
    case CameraId::WIDE_FOCUS:
      timestamp += 333;
      break;
  }

  header->set_camera_timestamp(timestamp);
  ros::MetaInfo info;
  info.camera_timestamp = timestamp;
  info.lidar_timestamp = 0;
  ros::MetaStats::instance()->record_publish(
      info, "/perception/traffic_light_status");
  // add traffic light result
  for (size_t i = 0; i < lights->size(); i++) {
    apollo::perception::TrafficLight *light_result = result.add_traffic_light();
    light_result->set_id(lights->at(i)->info.id().id());
    light_result->set_confidence(lights->at(i)->status.confidence);
    light_result->set_color(lights->at(i)->status.color);
  }

  // set contain_lights
  result.set_contain_lights(image_lights->num_signals > 0);

  // add traffic light debug info
  apollo::perception::TrafficLightDebug *light_debug =
      result.mutable_traffic_light_debug();

  // set signal number
  AINFO << "TLOutputSubnode num_signals: " << image_lights->num_signals
        << ", camera_id: " << kCameraIdToStr.at(image_lights->camera_id)
        << ", is_pose_valid: " << image_lights->is_pose_valid
        << ", ts: " << GLOG_TIMESTAMP(image_lights->timestamp);
  light_debug->set_signal_num(image_lights->num_signals);

  // Crop ROI
  if (lights->size() > 0 && lights->at(0)->region.debug_roi.size() > 0) {
    auto &crop_roi = lights->at(0)->region.debug_roi[0];
    auto tl_cropbox = light_debug->mutable_cropbox();
    tl_cropbox->set_x(crop_roi.x);
    tl_cropbox->set_y(crop_roi.y);
    tl_cropbox->set_width(crop_roi.width);
    tl_cropbox->set_height(crop_roi.height);
  }

  // Rectified ROI
  for (size_t i = 0; i < lights->size(); ++i) {
    auto &rectified_roi = lights->at(i)->region.rectified_roi;
    auto tl_rectified_box = light_debug->add_box();
    tl_rectified_box->set_x(rectified_roi.x);
    tl_rectified_box->set_y(rectified_roi.y);
    tl_rectified_box->set_width(rectified_roi.width);
    tl_rectified_box->set_height(rectified_roi.height);
    tl_rectified_box->set_color(lights->at(i)->status.color);
    tl_rectified_box->set_selected(true);
  }

  // Projection ROI
  for (size_t i = 0; i < lights->size(); ++i) {
    auto &projection_roi = lights->at(i)->region.projection_roi;
    auto tl_projection_box = light_debug->add_box();
    tl_projection_box->set_x(projection_roi.x);
    tl_projection_box->set_y(projection_roi.y);
    tl_projection_box->set_width(projection_roi.width);
    tl_projection_box->set_height(projection_roi.height);
  }

  // debug ROI (candidate detection boxes)
  if (lights->size() > 0 && lights->at(0)->region.debug_roi.size() > 0) {
    for (size_t i = 1; i < lights->at(0)->region.debug_roi.size(); ++i) {
      auto &debug_roi = lights->at(0)->region.debug_roi[i];
      auto tl_debug_box = light_debug->add_box();
      tl_debug_box->set_x(debug_roi.x);
      tl_debug_box->set_y(debug_roi.y);
      tl_debug_box->set_width(debug_roi.width);
      tl_debug_box->set_height(debug_roi.height);
    }
  }

  light_debug->set_ts_diff_pos(image_lights->diff_image_pose_ts);
  light_debug->set_ts_diff_sys(image_lights->diff_image_sys_ts);
  light_debug->set_valid_pos(image_lights->is_pose_valid);
  light_debug->set_project_error(image_lights->offset);

  if (lights->size() > 0) {
    double distance = stopline_distance(image_lights->pose.pose(),
                                        lights->at(0)->info.stop_line());
    light_debug->set_distance_to_stop_line(distance);
  }

  // AINFO << "TLOutputSubnode tl_debug_info: "
  //         << light_debug->ShortDebugString();

  if (FLAGS_enable_fill_lights_outside_image) {
    // We can fill the light with DEFAULT_UNKNOWN_COLOR, even if the light is
    // outside the image.
    auto color = DEFAULT_UNKNOWN_COLOR;
    const auto &lights_outside_image = image_lights->lights_outside_image;
    if (!lights_outside_image->empty()) {
      if (lights->empty()) {
        // map return lights && all lights outside the images.
        ADEBUG
            << "Output will not fill lights, because all lights outside image.";
      } else {
        color = lights->at(0)->status.color;
        for (size_t i = 0; i < lights_outside_image->size(); i++) {
          apollo::perception::TrafficLight *light_result =
              result.add_traffic_light();
          light_result->set_id(lights_outside_image->at(i)->info.id().id());
          light_result->set_confidence(
              lights_outside_image->at(i)->status.confidence);
          light_result->set_color(color);
        }
      }
    }
  }

  if (!result.SerializeToString(&((*msg)->data))) {
    AERROR << "Output_result serialize to String failed.";
    return false;
  }
  // auto process_time = TimeUtil::GetCurrentTime() - event.local_timestamp;
  auto process_time =
      TimeUtil::GetCurrentTime() - image_lights->preprocess_receive_timestamp;
  AINFO << "TLOutputSubnode transform_message "
        << " ts:" << GLOG_TIMESTAMP(event.timestamp)
        << " device:" << image_lights->image->camera_id_str() << " consuming "
        << process_time * 1000 << " ms."
        << " number of lights:" << lights->size()
        << " lights:" << result.ShortDebugString();

  timer.End("TLOutputSubnode::transform_message");
  return true;
}

REGISTER_SUBNODE(TLOutputSubnode);

}  // namespace traffic_light
}  // namespace perception
}  // namespace apollo
