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
 * @file
 */

#ifndef MODULES_DREAMVIEW_BACKEND_MAP_MAP_SERVICE_H_
#define MODULES_DREAMVIEW_BACKEND_MAP_MAP_SERVICE_H_

#include <string>
#include <vector>
#include "modules/map/hdmap/hdmap.h"
#include "third_party/json/json.hpp"

/**
 * @namespace apollo::dreamview
 * @brief apollo::dreamview
 */
namespace apollo {
namespace dreamview {

struct MapElementIds {
  std::vector<std::string> lane;
  std::vector<std::string> crosswalk;
  std::vector<std::string> junction;
  std::vector<std::string> signal;
  std::vector<std::string> stop_sign;
  std::vector<std::string> yield;
  std::vector<std::string> overlap;

  MapElementIds();
  explicit MapElementIds(const nlohmann::json &json_object);

  void LogDebugInfo() const {
    AINFO << "Lanes: " << lane.size();
    AINFO << "Crosswalks: " << crosswalk.size();
    AINFO << "Junctions: " << junction.size();
    AINFO << "Signals: " << signal.size();
    AINFO << "StopSigns: " << stop_sign.size();
    AINFO << "YieldSigns: " << yield.size();
    AINFO << "Overlaps: " << overlap.size();
  }

  size_t Hash() const;
  nlohmann::json Json() const;
};

class MapService {
 public:
  explicit MapService(const std::string &map_filename);
  MapElementIds CollectMapElements(const apollo::hdmap::Point &point,
                                   double raidus) const;

  // The returned value is of a ::apollo::hdmap::Map proto. This
  // makes it easy to convert to a JSON object and to send to the
  // javascript clients.
  ::apollo::hdmap::Map RetrieveMapElements(const MapElementIds &ids) const;

  const ::apollo::hdmap::HDMap &hdmap() {
    return hdmap_;
  }

 private:
  ::apollo::hdmap::HDMap hdmap_;
};

}  // namespace dreamview
}  // namespace apollo

#endif  // MODULES_DREAMVIEW_BACKEND_MAP_MAP_SERVICE_H_
