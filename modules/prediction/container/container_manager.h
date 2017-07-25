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
 * @brief Use container manager to manage all containers
 */

#ifndef MODULES_PREDICTION_CONTAINER_CONTAINER_MANAGER_H_
#define MODULES_PREDICTION_CONTAINER_CONTAINER_MANAGER_H_

#include <unordered_map>
#include <string>
#include <memory>

#include "modules/prediction/container/container.h"
#include "modules/common/macro.h"

/**
 * @namespace apollo::prediction
 * @brief apollo::prediction
 */
namespace apollo {
namespace prediction {

class ContainerManager {
 public:
  /**
   * @brief Destructor
   */ 
  virtual ~ContainerManager();

  /**
   * @brief Register all containers
   */
  void RegisterContainers();

  /**
   * @brief Get mutable container
   * @param Name of the container
   * @return Pointer to the container given the name
   */
  Container* mutable_container(const std::string& name);

 private:
  std::unique_ptr<Container> CreateContainer(const std::string& name);

  void RegisterContainer(const std::string& name);

 private:
  std::unordered_map<std::string, std::unique_ptr<Container>> containers_;

  DECLARE_SINGLETON(ContainerManager)
};

}  // namespace prediction
}  // namespace apollo

#endif  // MODULES_PREDICTION_CONTAINER_CONTAINER_MANAGER_H_
