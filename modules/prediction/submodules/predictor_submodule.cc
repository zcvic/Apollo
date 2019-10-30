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

#include "modules/prediction/submodules/predictor_submodule.h"

#include <utility>

#include "modules/common/adapters/adapter_gflags.h"
#include "modules/common/adapters/proto/adapter_config.pb.h"
#include "modules/common/time/time.h"
#include "modules/prediction/common/prediction_system_gflags.h"
#include "modules/prediction/predictor/predictor_manager.h"

namespace apollo {
namespace prediction {

PredictorSubmodule::~PredictorSubmodule() {}

std::string PredictorSubmodule::Name() const {
  return FLAGS_evaluator_submodule_name;
}

bool PredictorSubmodule::Init() {
  if (!MessageProcess::InitEvaluators()) {
    return false;
  }
  predictor_writer_ =
      node_->CreateWriter<PredictionObstacles>(FLAGS_prediction_topic);
  return true;
}

bool PredictorSubmodule::Proc(
    const std::shared_ptr<EvaluatorOutput>& evaluator_output,
    const std::shared_ptr<ADCTrajectoryContainer>& adc_trajectory_container) {
  // TODO(kechxu) implement
  return true;
}

}  // namespace prediction
}  // namespace apollo
