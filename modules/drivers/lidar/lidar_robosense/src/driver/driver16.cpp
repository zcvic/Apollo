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
#include <time.h>
#include <unistd.h>

#include <cmath>
#include <memory>
#include <string>
#include <thread>

#include "modules/drivers/lidar/lidar_robosense/include/driver/driver.h"

namespace autobot {
namespace drivers {
namespace robosense {

Robosense16Driver::Robosense16Driver(
  const  cybertron::proto::SutengConfig& robo_config)
    : RobosenseDriver() {
  _config = robo_config;
}

Robosense16Driver::~Robosense16Driver() {
  _running.store(false);
  if (positioning_thread_.joinable()) {
    positioning_thread_.join();
  }
}

void Robosense16Driver::init() {
  _running.store(true);
  double packet_rate =
      750;  // 840.0;          //每秒packet的数目   packet frequency (Hz)
            // velodyne-754  beike v6k-781.25  v6c-833.33
  double frequency =
      (_config.rpm() / 60.0);   //  每秒的圈数  expected Hz rate

  // default number of packets for each scan is a single revolution
  // (fractions rounded up)
  _config.set_npackets(
      ceil(packet_rate / frequency));  // 每frame，即转一圈packet的数目
  AINFO << "Time Synchronized is using time of pakcet";
  AINFO << "_config.npackets() == " << _config.npackets();
  if (_config.has_pcap_file()) {
    AINFO << "_config.pcap_file(): " << _config.pcap_file();
    _input.reset(
        new robosense::PcapInput(packet_rate, _config.pcap_file(), false));
    _input->init();

    AINFO << "_config.pcap_file(): " << _config.pcap_file();
    _positioning_input.reset(new robosense::PcapInput(
        packet_rate, _config.accurate_vehicle_config_path(), false));
    _positioning_input->init();
    AINFO << "driver16-inited----";
  } else {
    _input.reset(new robosense::SocketInput());
    _input->init(_config.firing_data_port());

    _positioning_input.reset(new robosense::SocketInput());
    _positioning_input->init(_config.positioning_data_port());
  }
  positioning_thread_ =
      std::thread(&Robosense16Driver::poll_positioning_packet, this);
}

/** poll the device
 *
 *  @returns true unless end of file reached
 */
bool Robosense16Driver::poll(
  const  std::shared_ptr<adu::common::sensor::suteng::SutengScan>& scan) {
  int poll_result = SOCKET_TIMEOUT;

  // Synchronizing all laser radars
  if (_config.main_frame()) {
    poll_result = poll_sync_count(scan, true);
  } else {
    poll_result = poll_sync_count(scan, false);
  }

  if (poll_result == PCAP_FILE_END) {
    return false;  // read the end of pcap file, return false stop poll;
  }

  if (poll_result == SOCKET_TIMEOUT || poll_result == RECIEVE_FAIL) {
    return false;  // poll again
  }

  if (scan->firing_pkts_size() == 0) {
    AINFO << "Get a empty scan from port: " << _config.firing_data_port();
    return false;
  }

  scan->set_model(_config.model());
  scan->set_mode(_config.mode());
  scan->mutable_header()->set_frame_id(_config.frame_id());
  scan->mutable_header()->set_lidar_timestamp(
      apollo::cyber::Time().Now().ToNanosecond());

  scan->set_basetime(_basetime);
  AINFO << "time: " << apollo::cyber::Time().Now().ToNanosecond();
  return true;
}

/** poll the device
 *
 *  @returns true unless end of file reached
 */
void Robosense16Driver::poll_positioning_packet(void) {
  while (!apollo::cyber::IsShutdown() && _running.load()) {
    NMEATimePtr nmea_time(new NMEATime);
    if (!_config.use_gps_time()) {
      _basetime = 1;  // temp
      time_t t = time(NULL);
      struct tm ptm;
      struct tm* current_time = localtime_r(&t, &ptm);
      nmea_time->year = current_time->tm_year - 100;
      nmea_time->mon = current_time->tm_mon + 1;
      nmea_time->day = current_time->tm_mday;
      nmea_time->hour = current_time->tm_hour;
      nmea_time->min = current_time->tm_min;
      nmea_time->sec = current_time->tm_sec;
      AINFO << "frame_id:" << _config.frame_id() << "-F(local-time):"
            << "year:" << nmea_time->year << "mon:" << nmea_time->mon
            << "day:" << nmea_time->day << "hour:" << nmea_time->hour
            << "min:" << nmea_time->min << "sec:" << nmea_time->sec;
    } else {
      while (true && _running.load()) {
        // AINFO<<"11111->gps timeing...";
        if (_positioning_input == nullptr) {
          AERROR << " _positioning_input uninited.";
          return;
        }

        int rc = _positioning_input->get_positioning_data_packtet(nmea_time);
        AINFO << "gprmc data rc: " << rc << "---rc is normal when it is 0";

        tm pkt_time;
        memset(&pkt_time, 0, sizeof(pkt_time));
        pkt_time.tm_year = nmea_time->year + 100;
        pkt_time.tm_mon = nmea_time->mon - 1;
        pkt_time.tm_mday = nmea_time->day;
        pkt_time.tm_hour = nmea_time->hour + 8;
        pkt_time.tm_min = nmea_time->min;
        pkt_time.tm_sec = 0;

        uint64_t timestamp_sec = static_cast<uint64_t>(mktime(&pkt_time)) * 1e9;
        uint64_t timestamp_nsec =
            static_cast<uint64_t>(nmea_time->sec * 1e6 + nmea_time->msec * 1e3 +
                                  nmea_time->usec) *
                1e3 +
            timestamp_sec;  // ns
        AINFO << "first POS-GPS-timestamp: [" << timestamp_nsec << "]";
        _basetime = timestamp_nsec;
        AINFO << "frame_id:" << _config.frame_id()
              << "-T(gps-time):" << nmea_time->year << "-" << nmea_time->mon
              << "-" << nmea_time->day << "  " << nmea_time->hour << "-"
              << nmea_time->min << "-" << nmea_time->sec;
        AINFO << "first POS-GPS-time: [" << nmea_time->year << "/"
              << nmea_time->mon << "/" << nmea_time->day << "-"
              << nmea_time->hour << "/" << nmea_time->min << "/"
              << nmea_time->sec << "-" << nmea_time->msec << "/"
              << nmea_time->usec << "]";
        _start_time = apollo::cyber::Time().Now().ToNanosecond();
        AINFO << "first _start_time:[" << _start_time << "]";
        if (rc == 0) {
          break;  // got a full packet
        }
      }
    }
    if (_basetime != 0) break;  // temp
    // if (_basetime == 0 && ret) {
    //   set_base_time_from_nmea_time(nmea_time, _basetime,
    //   _config.use_gps_time()); AINFO<<"basetime(outer): ";//<<basetime;
    //   break;
    // }
  }
}

}  // namespace robosense
}  // namespace drivers
}  // namespace autobot
