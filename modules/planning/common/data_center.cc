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
 * @file:
 **/

#include "modules/planning/common/data_center.h"

#include <fstream>
#include <utility>

#include "google/protobuf/text_format.h"
#include "modules/common/log.h"
#include "modules/planning/common/planning_gflags.h"

namespace apollo {
namespace planning {

using apollo::common::Status;

DataCenter::DataCenter() {
  _master.reset(new MasterStateMachine());

  AINFO << "Data Center is ready!";

  if (map_.load_map_from_file(FLAGS_map_filename)) {
    AFATAL << "Failed to load map: " << FLAGS_map_filename;
  }
  AINFO << "map loaded, Map file: " << FLAGS_map_filename;
}

Frame *DataCenter::frame(const uint32_t sequence_num) const {
  std::unordered_map<uint32_t, std::unique_ptr<Frame>>::const_iterator it
      = _frames.find(sequence_num);
  if (it != _frames.end()) {
    return it->second.get();
  }
  return nullptr;
}

apollo::common::Status DataCenter::init_frame(const uint32_t sequence_num) {
  _frame.reset(new Frame(sequence_num));
  _frame->set_environment(_environment);
  return Status::OK();
}

Environment *DataCenter::mutable_environment() {
  return &_environment;
}

Frame *DataCenter::current_frame() const {
  return _frame.get();
}

void DataCenter::save_frame() {
  _sequence_queue.push_back(_frame->sequence_num());
  _frames[_frame->sequence_num()] = std::move(_frame);
  if (_sequence_queue.size() >
      static_cast<uint32_t>(FLAGS_max_history_result)) {
    _frames.erase(_sequence_queue.front());
    _sequence_queue.pop_front();
  }
}

const Frame *DataCenter::last_frame() const {
  if (_sequence_queue.empty()) {
    return nullptr;
  }
  uint32_t sequence_num = _sequence_queue.back();
  return _frames.find(sequence_num)->second.get();
}

MasterStateMachine *DataCenter::mutable_master() const {
  return _master.get();
}

}  // namespace planning
}  // namespace apollo
