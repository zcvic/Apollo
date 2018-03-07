/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
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

// @brief: lane_post_processing_subnode source file

#include "modules/perception/obstacle/onboard/lane_post_processing_subnode.h"

#include <Eigen/Dense>

#include <cfloat>

#include "opencv2/opencv.hpp"
#include "yaml-cpp/yaml.h"

#include "modules/common/log.h"
#include "modules/perception/common/perception_gflags.h"
#include "modules/perception/lib/base/time_util.h"
#include "modules/perception/lib/config_manager/config_manager.h"

#include "modules/perception/onboard/event_manager.h"
#include "modules/perception/onboard/shared_data_manager.h"
#include "modules/perception/onboard/types.h"

namespace apollo {
namespace perception {

using std::string;
using std::shared_ptr;
using apollo::common::Status;
using apollo::common::ErrorCode;

bool LanePostProcessingSubnode::InitInternal() {
  // init shared data
  if (!InitSharedData()) {
    AERROR << "failed to init shared data.";
    return false;
  }

  // init plugins
  if (!InitAlgorithmPlugin()) {
    AERROR << "failed to init algorithm plugins.";
    return false;
  }

  // init work_root
  if (!InitWorkRoot()) {
    AERROR << "failed to init work root.";
    return false;
  }

  AINFO << "init LanePostProcessing subnode successfully.";
  return true;
}

bool LanePostProcessingSubnode::InitSharedData() {
  if (shared_data_manager_ == nullptr) {
    AERROR << "shared data manager is a null pointer.";
    return false;
  }

  // init preprocess_data
  camera_object_data_ = dynamic_cast<CameraObjectData *>(
      shared_data_manager_->GetSharedData("CameraObjectData"));
  if (camera_object_data_ == nullptr) {
    AERROR << "failed to get shared data instance: CameraObjectData ";
    return false;
  }
  lane_shared_data_ = dynamic_cast<LaneSharedData *>(
      shared_data_manager_->GetSharedData("LaneSharedData"));
  if (lane_shared_data_ == nullptr) {
    AERROR << "failed to get shared data instance: LaneSharedData ";
    return false;
  }

  AINFO << "init shared data successfully, data: "
        << camera_object_data_->name() << " and " << lane_shared_data_->name();

  return true;
}

bool LanePostProcessingSubnode::InitAlgorithmPlugin() {
  // init lane post-processer
  lane_post_processor_.reset(
      BaseCameraLanePostProcessorRegisterer::GetInstanceByName(
          FLAGS_onboard_lane_post_processor));
  if (!lane_post_processor_) {
    AERROR << "failed to get instance: " << FLAGS_onboard_lane_post_processor;
    return false;
  }
  if (!lane_post_processor_->Init()) {
    AERROR << "failed to init lane post-processor: "
           << lane_post_processor_->name();
    return false;
  }

  AINFO << "init alg pulgins successfully\n"
        << " lane post-processer:     " << FLAGS_onboard_lane_post_processor;
  return true;
}

bool LanePostProcessingSubnode::InitWorkRoot() {
  ConfigManager *config_manager = ConfigManager::instance();
  if (config_manager == NULL) {
    AERROR << "failed to get ConfigManager instance.";
    return false;
  }

  if (!config_manager->Init()) {
    AERROR << "failed to init ConfigManager";
    return false;
  }
  // get work root dir
  work_root_dir_ = config_manager->WorkRoot();

  AINFO << "init config manager successfully, work_root: " << work_root_dir_;
  return true;
}

Status LanePostProcessingSubnode::ProcEvents() {
  // fusion output subnode only subcribe the fusion subnode
  CHECK_EQ(sub_meta_events_.size(), 1u) << "only subcribe one event.";
  const EventMeta &event_meta = sub_meta_events_[0];
  Event event;
  event_manager_->Subscribe(event_meta.event_id, &event);
  // PERF_FUNCTION();
  ++seq_num_;
  shared_ptr<SensorObjects> objs;
  if (!GetSharedData(event, &objs)) {
    AERROR << "Failed to get shared data. event:" << event.to_string();
    return Status(ErrorCode::PERCEPTION_ERROR, "Failed to proc events.");
  }

  cv::Mat lane_map = objs->camera_frame_supplement->lane_map;
  LaneObjectsPtr lane_instances(new LaneObjects());
  CameraLanePostProcessOptions options;
  options.timestamp = event.timestamp;

  lane_post_processor_->Process(lane_map, options, lane_instances);
  for (size_t i = 0; i < lane_instances->size(); ++i) {
    (*lane_instances)[i].timestamp = event.timestamp;
    (*lane_instances)[i].seq_num = seq_num_;
  }
  AINFO << "Before publish lane objects, objects num: "
        << lane_instances->size();

  PublishDataAndEvent(event.timestamp, lane_instances);

  AINFO << "Successfully finished lane post processing";
  return Status::OK();
}

bool LanePostProcessingSubnode::GetSharedData(const Event &event,
                                              shared_ptr<SensorObjects> *objs) {
  double timestamp = event.timestamp;
  string device_id = event.reserve;
  device_id_ = device_id;
  string data_key;
  if (!SubnodeHelper::ProduceSharedDataKey(timestamp, device_id, &data_key)) {
    AERROR << "failed to produce shared data key. EventID:" << event.event_id
           << " timestamp:" << timestamp << " device_id:" << device_id;
    return false;
  }

  if (!camera_object_data_->Get(data_key, objs)) {
    AERROR << "failed to get shared data. event:" << event.to_string();
    return false;
  }
  return true;
}

void LanePostProcessingSubnode::PublishDataAndEvent(
    double timestamp, const SharedDataPtr<LaneObjects> &lane_objects) {
  string key;
  if (!SubnodeHelper::ProduceSharedDataKey(timestamp, device_id_, &key)) {
    AERROR << "failed to produce shared key. time: "
           << GLOG_TIMESTAMP(timestamp) << ", device_id: " << device_id_;
    return;
  }

  if (!lane_shared_data_->Add(key, lane_objects)) {
    AWARN << "failed to add LaneSharedData. key: " << key
          << " num_detected_objects: " << lane_objects->size();
    return;
  }

  // pub events
  for (size_t idx = 0; idx < pub_meta_events_.size(); ++idx) {
    const EventMeta &event_meta = pub_meta_events_[idx];
    Event event;
    event.event_id = event_meta.event_id;
    event.timestamp = timestamp;
    event.reserve = device_id_;
    event_manager_->Publish(event);
  }
  AINFO << "succeed to publish data and event.";
}

REGISTER_SUBNODE(LanePostProcessingSubnode);

}  // namespace perception
}  // namespace apollo
