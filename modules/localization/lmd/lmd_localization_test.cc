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

#include "modules/localization/lmd/lmd_localization.h"

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ros/include/ros/ros.h"

#include "modules/common/adapters/adapter_manager.h"
#include "modules/common/log.h"
#include "modules/common/util/util.h"
#include "modules/localization/common/localization_gflags.h"

using apollo::common::adapter::AdapterConfig;
using apollo::common::adapter::AdapterManager;
using apollo::common::adapter::AdapterManagerConfig;

namespace apollo {
namespace localization {

class LMDLocalizationTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    FLAGS_v = 3;
    lmd_localizatoin_.reset(new LMDLocalization());

    // Setup AdapterManager.
    AdapterManagerConfig config;
    config.set_is_ros(false);
    {
      auto *sub_config = config.add_config();
      sub_config->set_mode(AdapterConfig::PUBLISH_ONLY);
      sub_config->set_type(AdapterConfig::LOCALIZATION);
    }
    AdapterManager::Init(config);
  }

 protected:
  template <class T>
  void load_data(const std::string &filename, T *data) {
    CHECK(common::util::GetProtoFromFile(filename, data))
        << "Failed to open file " << filename;
  }

  std::unique_ptr<LMDLocalization> lmd_localizatoin_;
};

}  // namespace localization
}  // namespace apollo
