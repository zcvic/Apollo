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

#include "modules/monitor/software/safety_manager.h"

#include "modules/common/adapters/adapter_manager.h"
#include "modules/common/kv_db/kv_db.h"
#include "modules/common/log.h"
#include "modules/common/util/map_util.h"
#include "modules/dreamview/backend/common/dreamview_gflags.h"
#include "modules/monitor/common/monitor_manager.h"

DEFINE_double(safety_mode_seconds_before_estop, 10.0,
              "Interval before sending estop after we found critical errors.");

namespace apollo {
namespace monitor {

using apollo::canbus::Chassis;
using apollo::common::KVDB;
using apollo::common::adapter::AdapterManager;
using apollo::common::util::ContainsKey;
using apollo::common::util::GetProtoFromFile;
using apollo::common::util::FindOrNull;

SafetyManager::SafetyManager() {
  CHECK(GetProtoFromFile(FLAGS_hmi_config_filename, &hmi_config_))
      << "Unable to parse HMI config file " << FLAGS_hmi_config_filename;
}

void SafetyManager::CheckSafety(const double current_time) {
  auto *system_status = MonitorManager::GetStatus();
  // Everything looks good or has been handled properly.
  if (!ShouldTriggerSafeMode()) {
    system_status->clear_passenger_msg();
    system_status->clear_safety_mode_trigger_time();
    return;
  }

  // Newly entered safety mode.
  if (!system_status->has_safety_mode_trigger_time()) {
    static const std::string kWarningMessageOnSafetyMode =
        "Please disengage! Please disengage! Please disengage!";
    system_status->set_passenger_msg(kWarningMessageOnSafetyMode);
    system_status->set_safety_mode_trigger_time(current_time);
    return;
  }

  const int estop_count_down = static_cast<int>(
      system_status->safety_mode_trigger_time() +
      FLAGS_safety_mode_seconds_before_estop - current_time);
  if (estop_count_down > 0) {
    // Send counting down.
    system_status->set_passenger_msg(std::to_string(estop_count_down));
  } else {
    // Trigger EStop.
    system_status->set_passenger_msg("Emergency stop triggered.");

    // TODO(all): Prefer a static "Planning::SendEStop()" helper function for
    // all use cases.
    apollo::planning::ADCTrajectory estop_trajectory;
    auto *estop = estop_trajectory.mutable_estop();
    estop->set_is_estop(true);
    estop->set_reason("No proper action was taken for safety mode.");

    AdapterManager::FillPlanningHeader("Monitor", &estop_trajectory);
    AdapterManager::PublishPlanning(estop_trajectory);
  }
}

bool SafetyManager::ShouldTriggerSafeMode() {
  // We only check safety mode in self driving mode.
  auto* adapter = AdapterManager::GetChassis();
  adapter->Observe();
  if (adapter->Empty()) {
    return false;
  }

  const auto driving_mode = adapter->GetLatestObserved().driving_mode();
  if (driving_mode != Chassis::COMPLETE_AUTO_DRIVE) {
    return false;
  }

  static const std::string kApolloModeKey = "apollo:dreamview:mode";
  if (!KVDB::Has(kApolloModeKey)) {
    AERROR << "Cannot get apollo mode";
    return true;
  }

  const std::string mode_name = KVDB::Get(kApolloModeKey);
  const apollo::dreamview::Mode *mode_conf =
      FindOrNull(hmi_config_.modes(), mode_name);
  if (mode_conf == nullptr) {
    AERROR << "Cannot find configuration for apollo mode: " << mode_name;
    return true;
  }

  const auto &hardware_status = MonitorManager::GetStatus()->hardware();
  for (const auto &hardware : mode_conf->live_hardware()) {
    const auto *status = FindOrNull(hardware_status, hardware);
    if (status == nullptr) {
      AERROR << "Cannot get status of hardware: " << hardware;
      return true;
    }
    if (status->summary() == Summary::ERROR ||
        status->summary() == Summary::FATAL) {
      AERROR << "Hardware " << hardware << " triggers safety mode: "
             << status->msg();
      return true;
    }
  }

  const auto& modules_status = MonitorManager::GetStatus()->modules();
  for (const auto &module : mode_conf->live_modules()) {
    const auto *status = FindOrNull(modules_status, module);
    if (status == nullptr) {
      AERROR << "Cannot get status of module: " << module;
      return true;
    }
    if (status->summary() == Summary::ERROR ||
        status->summary() == Summary::FATAL) {
      AERROR << "Module " << module << " triggers safety mode: "
             << status->msg();
      return true;
    }
  }

  // TODO(xiaoxq): We may also need to trigger safety mode if GPS becomes very
  // unstable.
  return false;
}

}  // namespace monitor
}  // namespace apollo
