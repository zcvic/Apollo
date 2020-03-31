/******************************************************************************
 * Copyright 2020 The Apollo Authors. All Rights Reserved.
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
 **/

#include "modules/planning/scenarios/learning_model/test_learning_model_scenario.h"

#include <algorithm>
#include <iterator>

#include "cyber/common/file.h"
#include "cyber/common/log.h"

namespace apollo {
namespace planning {
namespace scenario {

TestLearningModelScenario::TestLearningModelScenario(
    const ScenarioConfig& scenario_config,
    const ScenarioContext* context)
  : Scenario(scenario_config, context), device_(torch::kCPU) {
  const auto& config = scenario_config.test_learning_model_config();
  AINFO << "Loading learning model:" << config.model_file();
  if (apollo::cyber::common::PathExists(config.model_file())) {
    try {
      model_ = torch::jit::load(config.model_file(), device_);
    }
    catch (const c10::Error& e) {
      AERROR << "error loading the model:" << config.model_file();
      is_init_ = false;
      return;
    }
  }

  input_feature_num_ = config.input_feature_num();
  is_init_ = true;
}

bool TestLearningModelScenario::ExtractFeatures(Frame* frame,
    std::vector<torch::jit::IValue> *input_features) {
  if (!is_init_) {
    AWARN << "scenario is not initialzed successfully.";
    return false;
  }

  // TODO(all): generate learning features.
  // TODO(all): adapt to new input feature shapes
  std::vector<torch::jit::IValue> tuple;
  tuple.push_back(torch::zeros({2, 3, 224, 224}));
  tuple.push_back(torch::zeros({2, 14}));
  // assumption: future learning model use one dimension input features.
  input_features->push_back(torch::ivalue::Tuple::create(tuple));
  return true;
}

bool TestLearningModelScenario::InferenceModel(
    const std::vector<torch::jit::IValue> &input_features,
    Frame* frame) {
  if (!is_init_) {
    AWARN << "scenario is not initialzed successfully.";
    return false;
  }
  auto torch_output = model_.forward(input_features);
  ADEBUG << torch_output;

  return true;
}

Scenario::ScenarioStatus TestLearningModelScenario::Process(
    const common::TrajectoryPoint& planning_init_point,
    Frame* frame) {
  std::vector<torch::jit::IValue> input_features;
  ExtractFeatures(frame, &input_features);
  InferenceModel(input_features, frame);

  return STATUS_DONE;
}
std::unique_ptr<Stage> TestLearningModelScenario::CreateStage(
    const ScenarioConfig::StageConfig& stage_config) {
  return nullptr;
}
}  // namespace scenario
}  // namespace planning
}  // namespace apollo
