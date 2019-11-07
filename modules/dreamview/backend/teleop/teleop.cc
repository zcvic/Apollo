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

#include "modules/dreamview/backend/teleop/teleop.h"

#include "cyber/common/log.h"
#include "cyber/time/time.h"
#include "google/protobuf/util/json_util.h"
#include "modules/common/util/message_util.h"

namespace apollo {
namespace dreamview {

using Json = nlohmann::json;
using apollo::cyber::Time;
using apollo::planning::PadMessage;
using ::google::protobuf::util::MessageToJsonString;
using modules::teleop::network::ModemInfo;
using modules::teleop::teleop::DaemonServiceCmd;
using modules::teleop::teleop::DaemonServiceRpt;

// modem ids
const std::string modem0_id = "0";
const std::string modem1_id = "1";
const std::string modem2_id = "2";

const unsigned int encoder_count = 2;

// channels
const std::string modem0_channel = "/apollo/teleop/network/modem0";
const std::string modem1_channel = "/apollo/teleop/network/modem1";
const std::string modem2_channel = "/apollo/teleop/network/modem2";
const std::string car_daemon_cmd_channel =
    "/apollo/teleop/car/daemon_service/cmd";
const std::string car_daemon_rpt_channel =
    "/apollo/teleop/car/daemon_service/rpt";
const std::string operator_daemon_cmd_channel =
    "/apollo/teleop/operator/daemon_service/cmd";
const std::string operator_daemon_rpt_channel =
    "/apollo/teleop/operator/daemon_service/rpt";

TeleopService::TeleopService(WebSocketHandler *websocket)
    : node_(cyber::CreateNode("teleop")), websocket_(websocket) {
  RegisterMessageHandlers();

  teleop_status_["audio"] = false;
  teleop_status_["mic"] = false;
  teleop_status_["video"] = false;
  teleop_status_["video_starting"] = false;
  teleop_status_["video_stopping"] = false;
}

void TeleopService::Start() {
  // TODO get topic names from proto
  // TODO update proto to get all modems' info combined with rank

  modem0_info_reader_ = node_->CreateReader<ModemInfo>(
      modem0_channel, [this](const std::shared_ptr<ModemInfo> &msg) {
        UpdateModem(modem0_id, msg);
      });

  modem1_info_reader_ = node_->CreateReader<ModemInfo>(
      modem1_channel, [this](const std::shared_ptr<ModemInfo> &msg) {
        UpdateModem(modem1_id, msg);
      });

  modem2_info_reader_ = node_->CreateReader<ModemInfo>(
      modem2_channel, [this](const std::shared_ptr<ModemInfo> &msg) {
        UpdateModem(modem2_id, msg);
      });

  car_daemon_cmd_writer_ =
      node_->CreateWriter<DaemonServiceCmd>(car_daemon_cmd_channel);

  operator_daemon_cmd_writer_ =
      node_->CreateWriter<DaemonServiceCmd>(operator_daemon_cmd_channel);

  car_daemon_rpt_reader_ = node_->CreateReader<DaemonServiceRpt>(
      car_daemon_rpt_channel,
      [this](const std::shared_ptr<DaemonServiceRpt> &msg) {
        UpdateCarDaemonRpt(msg);
      });

  operator_daemon_rpt_reader_ = node_->CreateReader<DaemonServiceRpt>(
      operator_daemon_rpt_channel,
      [this](const std::shared_ptr<DaemonServiceRpt> &msg) {
        UpdateOperatorDaemonRpt(msg);
      });
}

void TeleopService::RegisterMessageHandlers() {
  // Send current teleop status to the new client.
  websocket_->RegisterConnectionReadyHandler(
      [this](WebSocketHandler::Connection *conn) { SendStatus(conn); });
  // Start/Stop local and remote audio
  websocket_->RegisterMessageHandler(
      "ToggleAudio",
      [this](const Json &json, WebSocketHandler::Connection *conn) {
        // TODO
        {
          boost::unique_lock<boost::shared_mutex> writer_lock(mutex_);
          teleop_status_["audio"] = !teleop_status_["audio"];

          // turn on/off the mic based on the audio status
          teleop_status_["mic"] = teleop_status_["audio"];
        }
      });
  // Mute/Unmute local microphone
  websocket_->RegisterMessageHandler(
      "ToggleMic",
      [this](const Json &json, WebSocketHandler::Connection *conn) {
        // TODO
        {
          boost::unique_lock<boost::shared_mutex> writer_lock(mutex_);
          teleop_status_["mic"] = !teleop_status_["mic"];
        }
      });
  // Start/stop local decoder and viewer, start/stop remote encoder and
  // compositor
  websocket_->RegisterMessageHandler(
      "ToggleVideo",
      [this](const Json &json, WebSocketHandler::Connection *conn) {
        // TODO
        {
          bool sendStartVideo = false;
          bool sendStopVideo = false;
          // create a scope for the mutex lock
          {
            boost::shared_lock<boost::shared_mutex> writer_lock(mutex_);
            // toggle depending on current state change
            if (teleop_status_["video_starting"]) {
              teleop_status_["video_starting"] = false;
              teleop_status_["video_stopping"] = true;
            } else if (teleop_status_["video_stopping"]) {
              teleop_status_["video_starting"] = true;
              teleop_status_["video_stopping"] = false;
            }
            // not currently starting or stopping video
            else {
              // toggle depending on current state
              if (teleop_status_["video"]) {
                teleop_status_["video_stopping"] = true;
                teleop_status_["video_starting"] = false;
              } else {
                teleop_status_["video_stopping"] = false;
                teleop_status_["video_starting"] = true;
              }
            }
            AINFO << "ToggleVideo: " << teleop_status_["video_starting"];
          }
          if (sendStartVideo || sendStopVideo) {
            SendVideoStreamCmd(sendStartVideo);
          }
        }
      });
  // Issue pull-over command to remote
  websocket_->RegisterMessageHandler(
      "PullOver", [this](const Json &json, WebSocketHandler::Connection *conn) {
        // TODO
      });
  // Issue emergency-stop command to remote
  websocket_->RegisterMessageHandler(
      "EStop", [this](const Json &json, WebSocketHandler::Connection *conn) {
        // TODO
      });
  // Issue resume-cruise command to remote
  websocket_->RegisterMessageHandler(
      "ResumeCruise",
      [this](const Json &json, WebSocketHandler::Connection *conn) {
        // TODO
      });
  // Request to get updated modem info for client display
  websocket_->RegisterMessageHandler(
      "RequestTeleopStatus",
      [this](const Json &json, WebSocketHandler::Connection *conn) {
        SendStatus(conn);
      });
}

void TeleopService::SendStatus(WebSocketHandler::Connection *conn) {
  std::string to_send;
  {
    boost::shared_lock<boost::shared_mutex> reader_lock(mutex_);
    to_send = teleop_status_.dump();
  }
  websocket_->SendData(conn, to_send);
}

void TeleopService::UpdateModem(const std::string &modem_id,
                                const std::shared_ptr<ModemInfo> &modem_info) {
  // TODO simplify data and only send necessary info for display
  // update modem_info_
  if (modem_info->has_technology()) {
    // teleop_status_["modems"][modem_info->provider()] =
    //  modem_info->technology();
    boost::unique_lock<boost::shared_mutex> writer_lock(mutex_);
    teleop_status_["modems"][modem_id] = modem_info->technology();
  }
}

void TeleopService::UpdateCarDaemonRpt(
    const std::shared_ptr<DaemonServiceRpt> &daemon_rpt) {
  {
    bool videoIsRunning = false;
    bool voipIsRunning = false;
    unsigned int runningEncoders = 0;
    for (int i = 0; i < daemon_rpt->services_size(); i++) {
      // look for voip_encoder or encoder0..1.2
      // check 'voip_encoder' first because it contains 'encoder'
      std::string service = daemon_rpt->services(i);
      if (service.find("voip_encoder") != std::string::npos) {
        voipIsRunning = true;
      } else if (service.find("encoder") != std::string::npos) {
        runningEncoders++;
      }
    }

    // all  video encoders are running.
    videoIsRunning = runningEncoders == encoder_count;

    // we may need to write commands to start/stop the video stream
    bool sendStartVideo = false;
    bool sendStopVideo = false;

    // scope for the lock
    {
      boost::unique_lock<boost::shared_mutex> writer_lock(mutex_);
      teleop_status_["video"] = videoIsRunning;
      teleop_status_["audio"] = voipIsRunning;

      if (teleop_status_["video"]) {
        if (teleop_status_["video_starting"]) {
          // video has started
          teleop_status_["video_starting"] = false;
        } else if (teleop_status_["video_stopping"]) {
          // not stopped yet
          sendStopVideo = true;
        }
      }
      // video not running
      else {
        if (teleop_status_["video_starting"]) {
          // not started yet
          sendStartVideo = true;
        } else if (teleop_status_["video_stopping"]) {
          // video is stopped
          teleop_status_["video_stopping"] = false;
        }
      }
    }
    if (sendStartVideo || sendStopVideo) {
      SendVideoStreamCmd(sendStartVideo);
    }
  }
}

void TeleopService::UpdateOperatorDaemonRpt(
    const std::shared_ptr<DaemonServiceRpt> &daemon_rpt) {
  {
    bool voipIsRunning = false;
    for (int i = 0; i < daemon_rpt->services_size(); i++) {
      std::string service = daemon_rpt->services(i);
      if (service.find("voip_encoder") != std::string::npos) {
        voipIsRunning = true;
        break;
      }
    }
    boost::unique_lock<boost::shared_mutex> writer_lock(mutex_);
    teleop_status_["mic"] = voipIsRunning;
  }
}

void TeleopService::SendVideoStreamCmd(bool start_stop) {
  DaemonServiceCmd msg;
  if (start_stop) {
    msg.set_cmd("start");
  } else {
    msg.set_cmd("kill");
  }
  // we send a message to each encoder.
  for (unsigned int i = 0; i < encoder_count; i++) {
    char encoderName[20];
    snprintf(encoderName, 20, "encoder%u", i);
    msg.set_service(encoderName);
    common::util::FillHeader("dreamview", &msg);
    car_daemon_cmd_writer_->Write(msg);
    AINFO << encoderName << " " << msg.cmd();
  }
}

}  // namespace dreamview
}  // namespace apollo
