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

#include "modules/dreamview/backend/hmi/hmi_worker.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

#include "cyber/proto/dag_conf.pb.h"
#include "modules/dreamview/proto/scenario.pb.h"
#include "modules/monitor/proto/system_status.pb.h"

#include "cyber/common/file.h"
#include "modules/common/adapters/adapter_gflags.h"
#include "modules/common/configs/config_gflags.h"
#include "modules/common/kv_db/kv_db.h"
#include "modules/common/util/future.h"
#include "modules/common/util/map_util.h"
#include "modules/common/util/message_util.h"
#include "modules/dreamview/backend/common/dreamview_gflags.h"
#include "modules/dreamview/backend/fuel_monitor/data_collection_monitor.h"
#include "modules/dreamview/backend/fuel_monitor/fuel_monitor_gflags.h"
#include "modules/dreamview/backend/fuel_monitor/fuel_monitor_manager.h"
#include "modules/dreamview/backend/fuel_monitor/preprocess_monitor.h"
#include "modules/dreamview/backend/hmi/vehicle_manager.h"

DEFINE_string(hmi_modes_config_path, "/apollo/modules/dreamview/conf/hmi_modes",
              "HMI modes config path.");

DEFINE_string(maps_data_path, "/apollo/modules/map/data", "Maps data path.");

DEFINE_string(vehicles_config_path, "/apollo/modules/calibration/data",
              "Vehicles config path.");

DEFINE_double(status_publish_interval, 5, "HMI Status publish interval.");

DEFINE_string(current_mode_db_key, "/apollo/hmi/status:current_mode",
              "Key to store hmi_status.current_mode in KV DB.");

DEFINE_string(default_hmi_mode, "Mkz Standard Debug",
              "Default HMI Mode when there is no cache.");

namespace apollo {
namespace dreamview {
namespace {

using apollo::audio::AudioEvent;
using apollo::canbus::Chassis;
using apollo::common::DriveEvent;
using apollo::common::KVDB;
using apollo::control::DrivingAction;
using apollo::cyber::Clock;
using apollo::cyber::Node;
using apollo::cyber::proto::DagConfig;
using apollo::dreamview::SimTicket;
using apollo::dreamview::UserAdsGroup;
using apollo::localization::LocalizationEstimate;
using apollo::monitor::ComponentStatus;
using apollo::monitor::SystemStatus;
using google::protobuf::Map;
using RLock = boost::shared_lock<boost::shared_mutex>;
using WLock = boost::unique_lock<boost::shared_mutex>;
using Json = nlohmann::json;

constexpr char kNavigationModeName[] = "Navigation";

// Convert a string to be title-like. E.g.: "hello_world" -> "Hello World".
std::string TitleCase(std::string_view origin) {
  std::vector<std::string> parts = absl::StrSplit(origin, '_');
  for (auto& part : parts) {
    if (!part.empty()) {
      // Upper case the first char.
      part[0] = static_cast<char>(toupper(part[0]));
    }
  }

  return absl::StrJoin(parts, " ");
}

// List subdirs and return a dict of {subdir_title: subdir_path}.
Map<std::string, std::string> ListDirAsDict(const std::string& dir) {
  Map<std::string, std::string> result;
  const auto subdirs = cyber::common::ListSubPaths(dir);
  for (const auto& subdir : subdirs) {
    const auto subdir_title = TitleCase(subdir);
    const auto subdir_path = absl::StrCat(dir, "/", subdir);
    result.insert({subdir_title, subdir_path});
  }
  return result;
}

// List files by pattern and return a dict of {file_title: file_path}.
Map<std::string, std::string> ListFilesAsDict(std::string_view dir,
                                              std::string_view extension) {
  Map<std::string, std::string> result;
  const std::string pattern = absl::StrCat(dir, "/*", extension);
  for (const std::string& file_path : cyber::common::Glob(pattern)) {
    // Remove the extension and convert to title case as the file title.
    const std::string filename = cyber::common::GetFileName(file_path);
    const std::string file_title =
        TitleCase(filename.substr(0, filename.length() - extension.length()));
    result.insert({file_title, file_path});
  }
  return result;
}

template <class FlagType, class ValueType>
void SetGlobalFlag(std::string_view flag_name, const ValueType& value,
                   FlagType* flag) {
  constexpr char kGlobalFlagfile[] =
      "/apollo/modules/common/data/global_flagfile.txt";
  if (*flag != value) {
    *flag = value;
    // Overwrite global flagfile.
    std::ofstream fout(kGlobalFlagfile, std::ios_base::app);
    ACHECK(fout) << "Fail to open global flagfile " << kGlobalFlagfile;
    fout << "\n--" << flag_name << "=" << value << std::endl;
  }
}

void System(std::string_view cmd) {
  const int ret = std::system(cmd.data());
  if (ret == 0) {
    AINFO << "SUCCESS: " << cmd;
  } else {
    AERROR << "FAILED(" << ret << "): " << cmd;
  }
}

}  // namespace

HMIWorker::HMIWorker(const std::shared_ptr<Node>& node)
    : config_(LoadConfig()), node_(node) {
  InitStatus();
}

void HMIWorker::Start() {
  InitReadersAndWriters();
  RegisterStatusUpdateHandler(
      [this](const bool status_changed, HMIStatus* status) {
        apollo::common::util::FillHeader("HMI", status);
        status_writer_->Write(*status);
        status->clear_header();
      });
  ResetComponentStatusTimer();
  thread_future_ = cyber::Async(&HMIWorker::StatusUpdateThreadLoop, this);
}

void HMIWorker::Stop() {
  stop_ = true;
  if (thread_future_.valid()) {
    thread_future_.get();
  }
}

HMIConfig HMIWorker::LoadConfig() {
  HMIConfig config;
  // Get available modes, maps and vehicles by listing data directory.
  *config.mutable_modes() =
      ListFilesAsDict(FLAGS_hmi_modes_config_path, ".pb.txt");
  ACHECK(!config.modes().empty())
      << "No modes config loaded from " << FLAGS_hmi_modes_config_path;

  *config.mutable_maps() = ListDirAsDict(FLAGS_maps_data_path);
  *config.mutable_vehicles() = ListDirAsDict(FLAGS_vehicles_config_path);
  AINFO << "Loaded HMI config: " << config.DebugString();
  return config;
}

HMIMode HMIWorker::LoadMode(const std::string& mode_config_path) {
  HMIMode mode;
  ACHECK(cyber::common::GetProtoFromFile(mode_config_path, &mode))
      << "Unable to parse HMIMode from file " << mode_config_path;
  // Translate cyber_modules to regular modules.
  for (const auto& iter : mode.cyber_modules()) {
    const std::string& module_name = iter.first;
    const CyberModule& cyber_module = iter.second;
    // Each cyber module should have at least one dag file.
    ACHECK(!cyber_module.dag_files().empty())
        << "None dag file is provided for " << module_name << " module in "
        << mode_config_path;

    Module& module = LookupOrInsert(mode.mutable_modules(), module_name, {});
    module.set_required_for_safety(cyber_module.required_for_safety());

    // Construct start_command:
    //     nohup mainboard -p <process_group> -d <dag> ... &
    module.set_start_command("nohup mainboard");
    const auto& process_group = cyber_module.process_group();
    if (!process_group.empty()) {
      absl::StrAppend(module.mutable_start_command(), " -p ", process_group);
    }
    for (const std::string& dag : cyber_module.dag_files()) {
      absl::StrAppend(module.mutable_start_command(), " -d ", dag);
    }
    absl::StrAppend(module.mutable_start_command(), " &");

    // Construct stop_command: pkill -f '<dag[0]>'
    const std::string& first_dag = cyber_module.dag_files(0);
    module.set_stop_command(absl::StrCat("pkill -f \"", first_dag, "\""));
    // Construct process_monitor_config.
    module.mutable_process_monitor_config()->add_command_keywords("mainboard");
    module.mutable_process_monitor_config()->add_command_keywords(first_dag);
  }
  mode.clear_cyber_modules();
  AINFO << "Loaded HMI mode: " << mode.DebugString();
  return mode;
}

void HMIWorker::InitStatus() {
  static constexpr char kDockerImageEnv[] = "DOCKER_IMG";
  status_.set_docker_image(cyber::common::GetEnv(kDockerImageEnv));
  status_.set_utm_zone_id(FLAGS_local_utm_zone_id);

  // Populate modes and current_mode.
  const auto& modes = config_.modes();
  for (const auto& iter : modes) {
    const std::string& mode = iter.first;
    status_.add_modes(mode);
    if (mode == FLAGS_vehicle_calibration_mode) {
      FuelMonitorManager::Instance()->RegisterFuelMonitor(
          mode, std::make_unique<DataCollectionMonitor>());
      FuelMonitorManager::Instance()->RegisterFuelMonitor(
          mode, std::make_unique<PreprocessMonitor>());
    } else if (mode == FLAGS_lidar_calibration_mode) {
      FuelMonitorManager::Instance()->RegisterFuelMonitor(
          mode, std::make_unique<PreprocessMonitor>("lidar_to_gnss"));
    } else if (mode == FLAGS_camera_calibration_mode) {
      FuelMonitorManager::Instance()->RegisterFuelMonitor(
          mode, std::make_unique<PreprocessMonitor>("camera_to_lidar"));
    }
  }

  // Populate maps and current_map.
  for (const auto& map_entry : config_.maps()) {
    status_.add_maps(map_entry.first);

    // If current FLAG_map_dir is available, set it as current_map.
    if (map_entry.second == FLAGS_map_dir) {
      status_.set_current_map(map_entry.first);
    }
  }

  // Populate vehicles and current_vehicle.
  for (const auto& vehicle : config_.vehicles()) {
    status_.add_vehicles(vehicle.first);
  }

  // Initial HMIMode by priority:
  //   1. NavigationMode if --use_navigation_mode is specified explicitly.
  //   2. CachedMode if it's stored in KV database.
  //   3. default_hmi_mode if it is available.
  //   4. Pick the first available mode.
  const std::string cached_mode =
      KVDB::Get(FLAGS_current_mode_db_key).value_or("");
  if (FLAGS_use_navigation_mode && ContainsKey(modes, kNavigationModeName)) {
    ChangeMode(kNavigationModeName);
  } else if (ContainsKey(modes, cached_mode)) {
    ChangeMode(cached_mode);
  } else if (ContainsKey(modes, FLAGS_default_hmi_mode)) {
    ChangeMode(FLAGS_default_hmi_mode);
  } else {
    ChangeMode(modes.begin()->first);
  }
}

void HMIWorker::InitReadersAndWriters() {
  status_writer_ = node_->CreateWriter<HMIStatus>(FLAGS_hmi_status_topic);
  pad_writer_ = node_->CreateWriter<control::PadMessage>(FLAGS_pad_topic);
  audio_event_writer_ =
      node_->CreateWriter<AudioEvent>(FLAGS_audio_event_topic);
  drive_event_writer_ =
      node_->CreateWriter<DriveEvent>(FLAGS_drive_event_topic);

  node_->CreateReader<SystemStatus>(
      FLAGS_system_status_topic,
      [this](const std::shared_ptr<SystemStatus>& system_status) {
        this->ResetComponentStatusTimer();

        WLock wlock(status_mutex_);

        const bool is_realtime_msg =
            FLAGS_use_sim_time
                ? system_status->is_realtime_in_simulation()
                : Clock::NowInSeconds() -
                          system_status->header().timestamp_sec() <
                      FLAGS_system_status_lifetime_seconds;
        // Update modules running status from realtime SystemStatus.
        if (is_realtime_msg) {
          for (auto& iter : *status_.mutable_modules()) {
            auto* status = FindOrNull(system_status->hmi_modules(), iter.first);
            iter.second =
                status != nullptr && status->status() == ComponentStatus::OK;
          }
        }
        // Update monitored components status.
        for (auto& iter : *status_.mutable_monitored_components()) {
          auto* status = FindOrNull(system_status->components(), iter.first);
          if (status != nullptr) {
            iter.second = status->summary();
          } else {
            iter.second.set_status(ComponentStatus::UNKNOWN);
            iter.second.set_message("Status not reported by Monitor.");
          }
        }

        // Update other components status.
        for (auto& iter : *status_.mutable_other_components()) {
          auto* status =
              FindOrNull(system_status->other_components(), iter.first);
          if (status != nullptr) {
            iter.second.CopyFrom(*status);
          } else {
            iter.second.set_status(ComponentStatus::UNKNOWN);
            iter.second.set_message("Status not reported by Monitor.");
          }
        }

        // Check if the status is changed.
        const size_t new_fingerprint =
            apollo::common::util::MessageFingerprint(status_);
        if (last_status_fingerprint_ != new_fingerprint) {
          status_changed_ = true;
          last_status_fingerprint_ = new_fingerprint;
        }
      });

  localization_reader_ =
      node_->CreateReader<LocalizationEstimate>(FLAGS_localization_topic);
  // Received Chassis, trigger action if there is high beam signal.
  chassis_reader_ = node_->CreateReader<Chassis>(
      FLAGS_chassis_topic, [this](const std::shared_ptr<Chassis>& chassis) {
        if (Clock::NowInSeconds() - chassis->header().timestamp_sec() <
            FLAGS_system_status_lifetime_seconds) {
          if (chassis->signal().high_beam()) {
            // Currently we do nothing on high_beam signal.
            const bool ret = Trigger(HMIAction::NONE);
            AERROR_IF(!ret) << "Failed to execute high_beam action.";
          }
        }
      });
}

bool HMIWorker::Trigger(const HMIAction action) {
  AINFO << "HMIAction " << HMIAction_Name(action) << " was triggered!";
  switch (action) {
    case HMIAction::NONE:
      break;
    case HMIAction::SETUP_MODE:
      SetupMode();
      break;
    case HMIAction::ENTER_AUTO_MODE:
      return ChangeDrivingMode(Chassis::COMPLETE_AUTO_DRIVE);
    case HMIAction::DISENGAGE:
      return ChangeDrivingMode(Chassis::COMPLETE_MANUAL);
    case HMIAction::RESET_MODE:
      ResetMode();
      break;
    case HMIAction::LOAD_SCENARIOS:
      LoadScenarios();
      break;
    default:
      AERROR << "HMIAction not implemented, yet!";
      return false;
  }
  return true;
}

bool HMIWorker::Trigger(const HMIAction action, const std::string& value) {
  AINFO << "HMIAction " << HMIAction_Name(action) << "(" << value
        << ") was triggered!";
  switch (action) {
    case HMIAction::CHANGE_MODE:
      ChangeMode(value);
      break;
    case HMIAction::CHANGE_MAP:
      ChangeMap(value);
      break;
    case HMIAction::CHANGE_VEHICLE:
      ChangeVehicle(value);
      break;
    case HMIAction::START_MODULE:
      StartModule(value);
      break;
    case HMIAction::STOP_MODULE:
      StopModule(value);
      break;
    case HMIAction::CHANGE_SCENARIO_SET:
      ChangeScenarioSet(value);
      break;
    case HMIAction::DELETE_SCENARIO_SET:
      DeleteScenarioSet(value);
      ChangeScenario("");
      break;
    case HMIAction::CHANGE_SCENARIO:
      ChangeScenario(value);
      break;
    default:
      AERROR << "HMIAction not implemented, yet!";
      return false;
  }
  return true;
}

void HMIWorker::SubmitAudioEvent(const uint64_t event_time_ms,
                                 const int obstacle_id, const int audio_type,
                                 const int moving_result,
                                 const int audio_direction,
                                 const bool is_siren_on) {
  std::shared_ptr<AudioEvent> audio_event = std::make_shared<AudioEvent>();
  apollo::common::util::FillHeader("HMI", audio_event.get());
  // Here we reuse the header time field as the event occurring time.
  // A better solution might be adding an event time field to DriveEvent proto
  // to make it clear.
  audio_event->mutable_header()->set_timestamp_sec(
      static_cast<double>(event_time_ms) / 1000.0);
  audio_event->set_id(obstacle_id);
  audio_event->set_audio_type(
      static_cast<apollo::audio::AudioType>(audio_type));
  audio_event->set_moving_result(
      static_cast<apollo::audio::MovingResult>(moving_result));
  audio_event->set_audio_direction(
      static_cast<apollo::audio::AudioDirection>(audio_direction));
  audio_event->set_siren_is_on(is_siren_on);

  // Read the current localization pose
  localization_reader_->Observe();
  if (localization_reader_->Empty()) {
    AERROR << "Failed to get localization associated with the audio event: "
           << audio_event->DebugString() << "\n Localization reader is empty!";
    return;
  }

  const std::shared_ptr<LocalizationEstimate> localization =
      localization_reader_->GetLatestObserved();
  audio_event->mutable_pose()->CopyFrom(localization->pose());
  AINFO << "AudioEvent: " << audio_event->DebugString();

  audio_event_writer_->Write(audio_event);
}

void HMIWorker::SubmitDriveEvent(const uint64_t event_time_ms,
                                 const std::string& event_msg,
                                 const std::vector<std::string>& event_types,
                                 const bool is_reportable) {
  std::shared_ptr<DriveEvent> drive_event = std::make_shared<DriveEvent>();
  apollo::common::util::FillHeader("HMI", drive_event.get());
  // TODO(xiaoxq): Here we reuse the header time field as the event occurring
  // time. A better solution might be adding the field to DriveEvent proto to
  // make it clear.
  drive_event->mutable_header()->set_timestamp_sec(
      static_cast<double>(event_time_ms) / 1000.0);
  drive_event->set_event(event_msg);
  drive_event->set_is_reportable(is_reportable);
  for (const auto& type_name : event_types) {
    DriveEvent::Type type;
    if (DriveEvent::Type_Parse(type_name, &type)) {
      drive_event->add_type(type);
    } else {
      AERROR << "Failed to parse drive event type:" << type_name;
    }
  }
  drive_event_writer_->Write(drive_event);
}

void HMIWorker::SensorCalibrationPreprocess(const std::string& task_type) {
  std::string start_command = absl::StrCat(
      "nohup bash /apollo/modules/tools/sensor_calibration/extract_data.sh -t ",
      task_type, " &");
  System(start_command);
}

void HMIWorker::VehicleCalibrationPreprocess() {
  std::string start_command = absl::StrCat(
      "nohup bash /apollo/modules/tools/vehicle_calibration/preprocess.sh "
      "--vehicle_type=\"",
      status_.current_vehicle(), "\" --record_num=", record_count_, " &");
  System(start_command);
}

bool HMIWorker::ChangeDrivingMode(const Chassis::DrivingMode mode) {
  // Always reset to MANUAL mode before changing to other mode.
  const std::string mode_name = Chassis::DrivingMode_Name(mode);
  if (mode != Chassis::COMPLETE_MANUAL) {
    if (!ChangeDrivingMode(Chassis::COMPLETE_MANUAL)) {
      AERROR << "Failed to reset to MANUAL before changing to " << mode_name;
      return false;
    }
  }

  auto pad = std::make_shared<control::PadMessage>();
  switch (mode) {
    case Chassis::COMPLETE_MANUAL:
      pad->set_action(DrivingAction::RESET);
      break;
    case Chassis::COMPLETE_AUTO_DRIVE:
      pad->set_action(DrivingAction::START);
      break;
    default:
      AFATAL << "Change driving mode to " << mode_name << " not implemented!";
      return false;
  }

  static constexpr int kMaxTries = 3;
  static constexpr auto kTryInterval = std::chrono::milliseconds(500);
  for (int i = 0; i < kMaxTries; ++i) {
    // Send driving action periodically until entering target driving mode.
    common::util::FillHeader("HMI", pad.get());
    pad_writer_->Write(pad);

    std::this_thread::sleep_for(kTryInterval);

    chassis_reader_->Observe();
    if (chassis_reader_->Empty()) {
      AERROR << "No Chassis message received!";
    } else if (chassis_reader_->GetLatestObserved()->driving_mode() == mode) {
      return true;
    }
  }
  AERROR << "Failed to change driving mode to " << mode_name;
  return false;
}

bool HMIWorker::ChangeMap(const std::string& map_name) {
  const std::string* map_dir = FindOrNull(config_.maps(), map_name);
  AINFO << "map name:  " << map_name;
  if (map_dir == nullptr) {
    AERROR << "Unknown map " << map_name;
    return false;
  }

  {
    // Update current_map status.
    WLock wlock(status_mutex_);
    if (status_.current_map() == map_name) {
      return true;
    }
    status_.set_current_map(map_name);
    status_changed_ = true;
  }

  SetGlobalFlag("map_dir", *map_dir, &FLAGS_map_dir);
  ResetMode();
  return true;
}

void HMIWorker::ChangeVehicle(const std::string& vehicle_name) {
  const std::string* vehicle_dir = FindOrNull(config_.vehicles(), vehicle_name);
  if (vehicle_dir == nullptr) {
    AERROR << "Unknown vehicle " << vehicle_name;
    return;
  }

  {
    // Update current_vehicle status.
    WLock wlock(status_mutex_);
    if (status_.current_vehicle() == vehicle_name) {
      return;
    }
    status_.set_current_vehicle(vehicle_name);
    status_changed_ = true;
  }
  ResetMode();
  ACHECK(VehicleManager::Instance()->UseVehicle(*vehicle_dir));
  // Restart Fuel Monitor
  auto* monitors = FuelMonitorManager::Instance()->GetCurrentMonitors();
  if (monitors != nullptr) {
    for (const auto& monitor : *monitors) {
      if (monitor.second->IsEnabled()) {
        monitor.second->Restart();
      }
    }
  }
}

void HMIWorker::ChangeMode(const std::string& mode_name) {
  if (!ContainsKey(config_.modes(), mode_name)) {
    AERROR << "Cannot change to unknown mode " << mode_name;
    return;
  }

  {
    RLock rlock(status_mutex_);
    // Skip if mode doesn't actually change.
    if (status_.current_mode() == mode_name) {
      return;
    }
  }
  ResetMode();

  {
    WLock wlock(status_mutex_);
    status_.set_current_mode(mode_name);
    current_mode_ = LoadMode(config_.modes().at(mode_name));

    status_.clear_modules();
    for (const auto& iter : current_mode_.modules()) {
      status_.mutable_modules()->insert({iter.first, false});
    }

    // Update monitored components of current mode.
    status_.clear_monitored_components();
    for (const auto& iter : current_mode_.monitored_components()) {
      status_.mutable_monitored_components()->insert({iter.first, {}});
    }

    status_.clear_other_components();
    for (const auto& iter : current_mode_.other_components()) {
      status_.mutable_other_components()->insert({iter.first, {}});
    }
    status_changed_ = true;
  }

  FuelMonitorManager::Instance()->SetCurrentMode(mode_name);
  KVDB::Put(FLAGS_current_mode_db_key, mode_name);
}

void HMIWorker::StartModule(const std::string& module) const {
  const Module* module_conf = FindOrNull(current_mode_.modules(), module);
  if (module_conf != nullptr) {
    System(module_conf->start_command());
  } else {
    AERROR << "Cannot find module " << module;
  }

  if (module == "Recorder") {
    auto* monitors = FuelMonitorManager::Instance()->GetCurrentMonitors();
    if (monitors != nullptr) {
      auto iter = monitors->find(FLAGS_data_collection_monitor_name);
      if (iter != monitors->end()) {
        auto* data_collection_monitor = iter->second.get();
        if (data_collection_monitor->IsEnabled() && record_count_ == 0) {
          data_collection_monitor->Restart();
        }
      }
      ++record_count_;
    }
  }
}

void HMIWorker::StopModule(const std::string& module) const {
  const Module* module_conf = FindOrNull(current_mode_.modules(), module);
  if (module_conf != nullptr) {
    System(module_conf->stop_command());
  } else {
    AERROR << "Cannot find module " << module;
  }
}

HMIStatus HMIWorker::GetStatus() const {
  RLock rlock(status_mutex_);
  return status_;
}

void HMIWorker::SetupMode() const {
  for (const auto& iter : current_mode_.modules()) {
    System(iter.second.start_command());
  }
}

void HMIWorker::ResetMode() const {
  for (const auto& iter : current_mode_.modules()) {
    System(iter.second.stop_command());
  }
  record_count_ = 0;
}

void HMIWorker::StatusUpdateThreadLoop() {
  constexpr int kLoopIntervalMs = 200;
  while (!stop_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kLoopIntervalMs));
    UpdateComponentStatus();
    bool status_changed = false;
    {
      WLock wlock(status_mutex_);
      status_changed = status_changed_;
      status_changed_ = false;
    }
    // If status doesn't change, check if we reached update interval.
    if (!status_changed) {
      static double next_update_time = 0;
      const double now = Clock::NowInSeconds();
      if (now < next_update_time) {
        continue;
      }
      next_update_time = now + FLAGS_status_publish_interval;
    }

    // Trigger registered status change handlers.
    HMIStatus status = GetStatus();
    for (const auto handler : status_update_handlers_) {
      handler(status_changed, &status);
    }
  }
}

void HMIWorker::ResetComponentStatusTimer() {
  last_status_received_s_ = Clock::NowInSeconds();
  last_status_fingerprint_ = 0;
}

void HMIWorker::UpdateComponentStatus() {
  constexpr double kSecondsTillTimeout(2.5);
  const double now = Clock::NowInSeconds();
  if (now - last_status_received_s_.load() > kSecondsTillTimeout) {
    if (!monitor_timed_out_) {
      WLock wlock(status_mutex_);

      const uint64_t now_ms = static_cast<uint64_t>(now * 2e3);
      static constexpr bool kIsReportable = true;
      SubmitDriveEvent(now_ms, "Monitor timed out", {"PROBLEM"}, kIsReportable);
      AWARN << "System fault. Auto disengage.";
      Trigger(HMIAction::DISENGAGE);

      for (auto& monitored_component :
           *status_.mutable_monitored_components()) {
        monitored_component.second.set_status(ComponentStatus::UNKNOWN);
        monitored_component.second.set_message(
            "Status not reported by Monitor.");
      }
      status_changed_ = true;
    }
    monitor_timed_out_ = true;
  } else {
    monitor_timed_out_ = false;
  }
}

void HMIWorker::ChangeScenarioSet(const std::string& scenario_set_id) {
  {
    RLock rlock(status_mutex_);
    auto& scenario_set = status_.scenario_set();
    if ((!scenario_set_id.empty()) &&
        (scenario_set.find(scenario_set_id) == scenario_set.end())) {
      AERROR << "Cannot change to unknown scenario set!";
      return;
    }
    if (status_.current_scenario_set_id() == scenario_set_id) {
      return;
    }
  }

  {
    WLock wlock(status_mutex_);
    status_.set_current_scenario_set_id(scenario_set_id);
    status_changed_ = true;
  }
  return;
}

void HMIWorker::GetScenarioResourcePath(std::string& scenario_resource_path) {
  const std::string home = cyber::common::GetEnv("HOME");
  scenario_resource_path = home + FLAGS_resource_scenario_path;
}

void HMIWorker::GetScenarioSetPath(const std::string& scenario_set_id,
                                   std::string& scenario_set_path) {
  GetScenarioResourcePath(scenario_set_path);
  scenario_set_path = scenario_set_path + scenario_set_id;
  return;
}

bool HMIWorker::StopModuleByCommand(const std::string& stop_command) const {
  int ret = std::system(stop_command.data());
  if (ret < 0 || !WIFEXITED(ret)) {
    // 256 does not means failure
    AINFO << "This is ret:   " << ret;
    AERROR << "Failed to stop sim obstacle";
    return false;
  }
  return true;
}

bool HMIWorker::ResetSimObstacle(const std::string& scenario_set_id,
                                 const std::string& scenario_id) {
  // sim obstacle 不一定开着 todo:注意这个地方会不会有问题
  // 关闭sim obstacle
  // 检查sim obstacle是否存在
  const std::string absolute_path = cyber::common::GetEnv("HOME") + FLAGS_sim_obstacle_path;
  if (!cyber::common::PathExists(absolute_path)) {
    // sim obstacle Binary package not exists!
    AERROR << "Failed to find sim obstacle";
    return false;
  }
  bool res = StopModuleByCommand(FLAGS_sim_obstacle_stop_command);
  if (!res) {
    AERROR << "Failed to stop sim obstacle";
    return false;
  }
  // 拼接新场景路径
  std::string scenario_set_path;
  GetScenarioSetPath(scenario_set_id, scenario_set_path);
  const std::string scenario_path =
      scenario_set_path + "/scenarios/" + scenario_id + ".json";
  if (!cyber::common::PathExists(scenario_path)) {
    AERROR << "Failed to find scenario!";
    return false;
  }
  // 获取场景相关地图信息
  std::string map_name = "";
  {
    RLock rlock(status_mutex_);
    auto& scenario_set = status_.scenario_set();
    if (scenario_set.find(scenario_set_id) == scenario_set.end()) {
      AERROR << "Failed to find scenario set!";
      return false;
    }
    for (auto& scenario : scenario_set.at(scenario_set_id).scenarios()) {
      if (scenario.scenario_id() == scenario_id) {
        map_name = scenario.map_name();
        break;
      }
    }
    if (map_name.empty()) {
      AERROR << "Failed to find scenario and get map dir!";
      return false;
    }
  }
  // 更换地图
  if (!ChangeMap(map_name)) {
    AERROR << "Failed to change map!";
    return false;
  }
  // 启动sim obstacle
  const std::string start_command = "nohup " + absolute_path +
                                    " " + scenario_path + " &";
  int ret = std::system(start_command.data());
  if (ret != 0) {
    AERROR << "Failed to start sim obstacle";
    return false;
  }
  return true;
}

void HMIWorker::ChangeScenario(const std::string& scenario_id) {
  {
    RLock rlock(status_mutex_);
    // Skip if mode doesn't actually change.
    if (status_.current_scenario_id() == scenario_id) {
      return;
    }
    if (scenario_id.empty()) {
      // stop sim obstacle
      bool res = StopModuleByCommand(FLAGS_sim_obstacle_stop_command);
      if (!res) {
        AERROR << "Cannot stop sim obstacle!";
        return;
      }
    } else {
      auto scenario_set = status_.mutable_scenario_set();
      auto& scenario_set_id = status_.current_scenario_set_id();
      if (scenario_set->find(scenario_set_id) == scenario_set->end()) {
        AERROR << "Current scenario set is invalid!";
        return;
      }
      bool find_res = false;
      for (auto& scenario : (*scenario_set)[scenario_set_id].scenarios()) {
        if (scenario.scenario_id() == scenario_id) {
          find_res = true;
          break;
        }
      }
      if (!find_res) {
        AERROR << "Cannot change to unknown scenario!";
        return;
      }
      // restart sim obstacle
      if (!ResetSimObstacle(scenario_set_id, scenario_id)) {
        AERROR << "Cannot start sim obstacle by new scenario!";
        return;
      };
    }
  }

  {
    WLock wlock(status_mutex_);
    status_.set_current_scenario_id(scenario_id);
    status_changed_ = true;
  }
  return;
}

bool HMIWorker::UpdateScenarioSetToStatus(
    const std::string& scenario_set_id, const std::string& scenario_set_name) {
  ScenarioSet new_scenario_set;
  if (!UpdateScenarioSet(scenario_set_id, scenario_set_name,
                         new_scenario_set)) {
    AERROR << "Failed to update scenario_set!";
    return false;
  }
  {
    WLock wlock(status_mutex_);
    auto scenario_set = status_.mutable_scenario_set();
    // 如果找到一样的，也进行覆盖
    scenario_set->erase(scenario_set_id);
    (*scenario_set)[scenario_set_id] = new_scenario_set;
    status_changed_ = true;
  }
  return true;
}

bool HMIWorker::UpdateScenarioSet(const std::string& scenario_set_id,
                                  const std::string& scenario_set_name,
                                  ScenarioSet& new_scenario_set) {
  // 解析json // 获取scenario set所有信息 再+入status。
  std::string scenario_set_directory_path;
  GetScenarioSetPath(scenario_set_id, scenario_set_directory_path);
  scenario_set_directory_path = scenario_set_directory_path + "/scenarios/";
  if (!cyber::common::PathExists(scenario_set_directory_path)) {
    AERROR << "Failed to find scenario_set!";
    return false;
  }
  // 逐路径添加
  new_scenario_set.set_scenario_set_name(scenario_set_name);
  // 遍历文件
  DIR* directory = opendir(scenario_set_directory_path.c_str());
  if (directory == nullptr) {
    AERROR << "Cannot open directory " << scenario_set_directory_path;
    return false;
  }

  struct dirent* file;
  while ((file = readdir(directory)) != nullptr) {
    // skip directory_path/. and directory_path/..
    if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) {
      continue;
    }
    const std::string file_name = file->d_name;
    if (!absl::EndsWith(file_name, ".json")) {
      continue;
    }
    const std::string file_path = scenario_set_directory_path + file_name;
    // 解析Proto 获取结果
    // todo:add scenario proto
    SimTicket new_sim_ticket;
    if (!cyber::common::GetProtoFromJsonFile(file_path, &new_sim_ticket)) {
      AERROR << "Cannot parse this scenario:" << file_path;
      return false;
    }
    if (!new_sim_ticket.has_scenario()) {
      AERROR << "Cannot get scenario.";
      return false;
    }
    if (!new_sim_ticket.description_en_tokens_size()) {
      AERROR << "Cannot get scenario name.";
      return false;
    }
    if (!new_sim_ticket.scenario().has_map_dir()) {
      AERROR << "Cannot get scenario map dir.";
      return false;
    }
    std::string scenario_name = new_sim_ticket.description_en_tokens(0);
    for (int i = 1; i < new_sim_ticket.description_en_tokens_size(); i++) {
      scenario_name =
          scenario_name + "_" + new_sim_ticket.description_en_tokens(i);
    }
    ScenarioInfo* scenario_info = new_scenario_set.add_scenarios();
    const int index = file_name.rfind(".json");
    const std::string scenario_id = file_name.substr(0, index);
    scenario_info->set_scenario_id(scenario_id);
    scenario_info->set_scenario_name(scenario_name);
    // change scenario json map dir to map name
    // format:modules/map/data/${map_name}
    const std::string map_dir = new_sim_ticket.scenario().map_dir();
    size_t idx = map_dir.find_last_of('/');
    if (idx == map_dir.npos) {
      AERROR << "Cannot get scenario map name.";
      return false;
    }
    const std::string map_name = map_dir.substr(idx + 1);
    if (map_name.empty()) {
      AERROR << "Cannot get scenario map name.";
      return false;
    }
    // replay engine use xx_xx like:apollo_map
    // dv need Apollo Map
    scenario_info->set_map_name(TitleCase(map_name));
  }
  closedir(directory);
  return true;
}

bool HMIWorker::LoadScenarios() {
  // 遍历文件夹
  std::string directory_path;
  GetScenarioResourcePath(directory_path);
  if (!cyber::common::PathExists(directory_path)) {
    AERROR << "Failed to find scenario_set!";
    return false;
  }
  DIR* directory = opendir(directory_path.c_str());
  if (directory == nullptr) {
    AERROR << "Cannot open directory " << directory_path;
    return false;
  }
  struct dirent* file;
  std::map<std::string, ScenarioSet> scenario_sets;
  while ((file = readdir(directory)) != nullptr) {
    // skip directory_path/. and directory_path/..
    if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) {
      continue;
    }
    if (file->d_type != DT_DIR) {
      continue;
    }
    // 获取scenario set id
    const std::string scenario_set_id = file->d_name;
    // 获取本地scenario set name
    const std::string scenario_set_json_path =
        directory_path + scenario_set_id + "/scenario_set.json";
    // scenario_set.json use message:UserAdsGroup
    UserAdsGroup user_ads_group_info;
    if (!cyber::common::GetProtoFromJsonFile(scenario_set_json_path,
                                             &user_ads_group_info)) {
      AERROR << "Unable to parse UserAdsGroup from file "
             << scenario_set_json_path;
      return false;
    }
    if (!user_ads_group_info.has_name()) {
      AERROR << "Failed to get ads group name!";
      return false;
    }
    const std::string scenario_set_name = user_ads_group_info.name();
    ScenarioSet new_scenario_set;
    if (!UpdateScenarioSet(scenario_set_id, scenario_set_name,
                           new_scenario_set)) {
      AERROR << "Failed to update scenario_set!";
      return false;
    }
    scenario_sets[scenario_set_id] = new_scenario_set;
  }
  closedir(directory);
  {
    WLock wlock(status_mutex_);
    auto scenario_set = status_.mutable_scenario_set();
    // 清除之前的数据
    scenario_set->clear();
    for (auto iter = scenario_sets.begin(); iter != scenario_sets.end();
         iter++) {
      // AINFO<<"Scenario name:  "<<iter->first<<"    Scenarios size:
      // "<<(iter->second).scenarios_size();
      (*scenario_set)[iter->first] = iter->second;
    }
    status_changed_ = true;
  }
  return true;
}

void HMIWorker::DeleteScenarioSet(const std::string& scenario_set_id) {
  // 拼接路径 本地删除
  // 删除Local 字典里的scenario set及对应scenario
  // 校验current scenario set，如果是被删除，则置为""
  if (scenario_set_id.empty()) {
    return;
  }
  std::string directory_path;
  GetScenarioResourcePath(directory_path);
  directory_path = directory_path + scenario_set_id;
  if (!cyber::common::PathExists(directory_path)) {
    AERROR << "Failed to find scenario_set!";
    return;
  }
  std::string command = "rm -fr " + directory_path;
  // use cyber::common::removeFiles do not support sub-directory
  // use rmdir do not support not empty directory
  if (std::system(command.data()) != 0) {
    AERROR << "Failed to delete scenario set directory for: "
           << std::strerror(errno);
    return;
  }

  {
    RLock rlock(status_mutex_);
    auto& scenario_set = status_.scenario_set();
    if (scenario_set.find(scenario_set_id) == scenario_set.end()) {
      AERROR << "Cannot find unknown scenario set!";
      return;
    }
  }

  {
    WLock wlock(status_mutex_);
    status_.mutable_scenario_set()->erase(scenario_set_id);
    if (status_.current_scenario_set_id() == scenario_set_id) {
      status_.set_current_scenario_set_id(scenario_set_id);
    }
    status_changed_ = true;
  }
  return;
}

}  // namespace dreamview
}  // namespace apollo
