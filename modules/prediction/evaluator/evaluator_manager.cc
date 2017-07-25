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

#include "modules/prediction/evaluator/evaluator_manager.h"

#include "modules/prediction/evaluator/evaluator_factory.h"
#include "modules/prediction/container/container_manager.h"
#include "modules/prediction/container/obstacles/obstacles_container.h"
#include "modules/common/log.h"

namespace apollo {
namespace prediction {

using ::apollo::perception::PerceptionObstacles;
using ::apollo::perception::PerceptionObstacle;

EvaluatorManager::EvaluatorManager() {}

Evaluator* EvaluatorManager::GetEvaluator(
    const ObstacleConf::EvaluatorType& type) {
  return EvaluatorFactory::instance()->CreateEvaluator(type).get();
}

void EvaluatorManager::Run(
    const ::apollo::perception::PerceptionObstacles& perception_obstacles) {
  ObstaclesContainer *container = dynamic_cast<ObstaclesContainer*>(
      ContainerManager::instance()->mutable_container("ObstaclesContainer"));
  CHECK_NOTNULL(container);

  for (const auto& perception_obstacle :
      perception_obstacles.perception_obstacle()) {
    int id = perception_obstacle.id();
    switch (perception_obstacle.type()) {
      case PerceptionObstacle::VEHICLE: {
        Evaluator *evaluator = GetEvaluator(ObstacleConf::MLP_EVALUATOR);
        CHECK_NOTNULL(evaluator);
        Obstacle *obstacle = container->GetObstacle(id);
        CHECK_NOTNULL(obstacle);
        evaluator->Evaluate(obstacle);
        break;
      }
      default: {
        break;
      }
    }
  }
}

}  // namespace prediction
}  // namespace apollo
