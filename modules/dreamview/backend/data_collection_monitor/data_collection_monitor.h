/******************************************************************************
 * Copyright 2019 The Apollo Authors. All Rights Reserved.
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
 * @file
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "boost/thread/shared_mutex.hpp"
#include "third_party/json/json.hpp"

#include "cyber/cyber.h"
#include "modules/canbus/proto/chassis.pb.h"
#include "modules/dreamview/proto/data_collection_table.pb.h"

/**
 * @namespace apollo::dreamview
 * @brief apollo::dreamview
 */
namespace apollo {
namespace dreamview {

/**
 * @class DataCollectionMonitor
 * @brief A module that monitor data collection progress for calibration
 * purpose.
 */
class DataCollectionMonitor {
 public:
  /**
   * @brief Constructor of DataCollectionMonitor.
   */
  DataCollectionMonitor();
  ~DataCollectionMonitor();

  bool IsEnabled() const { return enabled_; }

  /**
   * @brief start monitoring collection progress
   */
  void Start();

  /**
   * @brief stop monitoring collection progress
   */
  void Stop();

  /**
   * @brief return collection progress of categories and overall as json
   */
  nlohmann::json GetProgressString();

 private:
  void InitReaders();
  void LoadConfiguration(const std::string& data_collection_config_path);
  void OnChassis(const std::shared_ptr<apollo::canbus::Chassis>& chassis);
  void UpdateProgressInJson();

  std::unique_ptr<cyber::Node> node_;

  // Whether the calibration monitor is enabled.
  bool enabled_ = false;

  // The table defines data collection requirements for calibration
  DataCollectionTable data_collection_table_;

  // Number of frames that has been collected for each category
  std::unordered_map<std::string, size_t> category_frame_count_;

  // Total number of frames that has been collected
  size_t current_frame_count_ = 0.0;

  // Store overall and each category progress in percentage
  nlohmann::json current_progress_json_;

  // Mutex to protect concurrent access to current_progress_json_.
  // NOTE: Use boost until we have std version of rwlock support.
  boost::shared_mutex mutex_;
};

}  // namespace dreamview
}  // namespace apollo
