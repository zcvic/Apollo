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

#include "modules/drivers/lidar/lslidar/parser/lslidar_parser.h"

namespace apollo {
namespace drivers {
namespace lslidar {
		
LslidarCH32Parser::LslidarCH32Parser(const Config& config)
    : LslidarParser(config), previous_packet_stamp_(0), gps_base_usec_(0) {
  
}

void LslidarCH32Parser::GeneratePointcloud(
    const std::shared_ptr<LslidarScan>& scan_msg,
    std::shared_ptr<PointCloud> &out_msg) {
  // allocate a point cloud with same time and frame ID as raw data
    out_msg->mutable_header()->set_timestamp_sec(scan_msg->basetime()/1000000000.0);
    out_msg->mutable_header()->set_module_name(scan_msg->header().module_name());
    out_msg->mutable_header()->set_frame_id(scan_msg->header().frame_id());
    out_msg->set_height(1);
    out_msg->set_measurement_time(scan_msg->basetime()/1000000000.0);
    out_msg->mutable_header()->set_sequence_num(scan_msg->header().sequence_num());
    gps_base_usec_ = scan_msg->basetime();
  
  //const unsigned char* difop_ptr = (const unsigned char*)scan_msg->difop_pkts(0).data().c_str();
  
  packets_size = scan_msg->firing_pkts_size();

  for (size_t i = 0; i < packets_size; ++i) {
    Unpack(static_cast<int>(i), scan_msg->firing_pkts(static_cast<int>(i)), out_msg);
    last_time_stamp_ = out_msg->measurement_time();
    ADEBUG << "stamp: " << std::fixed << last_time_stamp_;
  }

  if (out_msg->point().empty()) {
    // we discard this pointcloud if empty
    AERROR << "All points is NAN!Please check lslidar:" << config_.model();
  }

  // set default width
  out_msg->set_width(out_msg->point_size());
}


/** @brief convert raw packet to point cloud
 *  @param pkt raw packet to Unpack
 *  @param pc shared pointer to point cloud (points are appended)
 */
void LslidarCH32Parser::Unpack(int num, const LslidarPacket& pkt,
                              std::shared_ptr<PointCloud> pc) {
  float x, y, z;
  uint64_t  packet_end_time;
  double z_sin_altitude = 0.0;
  double z_cos_altitude = 0.0;
  time_last = 0;
  const RawPacket* raw = (const RawPacket*)pkt.data().c_str();
  
  packet_end_time = pkt.stamp();
  
  for (size_t point_idx = 0; point_idx < POINTS_PER_PACKET; point_idx++) 
  {
	  firings[point_idx].vertical_line=raw->points[point_idx].vertical_line;
	  two_bytes point_amuzith;
	  point_amuzith.bytes[0] = raw->points[point_idx].azimuth_2;
	  point_amuzith.bytes[1] = raw->points[point_idx].azimuth_1;
	  firings[point_idx].azimuth=static_cast<double>(point_amuzith.uint)*0.01*DEG_TO_RAD;
	  four_bytes point_distance;
	  point_distance.bytes[0] = raw->points[point_idx].distance_3;
	  point_distance.bytes[1] = raw->points[point_idx].distance_2;
	  point_distance.bytes[2] = raw->points[point_idx].distance_1;
	  point_distance.bytes[3] = 0;
	  firings[point_idx].distance=static_cast<double>(point_distance.uint) * DISTANCE_RESOLUTION2;     
	  firings[point_idx].intensity=raw->points[point_idx].intensity;   
   }  

    double scan_laser_altitude_degree[8];
    if(1 ==  config_.degree_mode())
	{
		for(int i=0;i<8;i++)
			scan_laser_altitude_degree[i] = sin_scan_laser_altitude[i];
    }
	else
	{
		for(int i=0;i<8;i++)
			scan_laser_altitude_degree[i] = sin_scan_laser_altitude_ch32[i];
	}
	   
	for (size_t point_idx = 0; point_idx < POINTS_PER_PACKET; point_idx++) 
	{
		LaserCorrection& corrections = calibration_.laser_corrections_[firings[point_idx].vertical_line];
		
		if (config_.calibration()) 
			firings[point_idx].distance = firings[point_idx].distance + corrections.dist_correction;
		
    if (firings[point_idx].distance > config_.max_range() || firings[point_idx].distance < config_.min_range())
			continue;
		
		// Convert the point to xyz coordinate
		double cos_azimuth_half=cos(firings[point_idx].azimuth*0.5);
		z_sin_altitude=scan_laser_altitude_degree[firings[point_idx].vertical_line/4]+2*cos_azimuth_half*sin_scan_mirror_altitude[firings[point_idx].vertical_line%4];
		z_cos_altitude=sqrt(1-z_sin_altitude*z_sin_altitude);
		
		x = firings[point_idx].distance *z_cos_altitude * cos(firings[point_idx].azimuth);
		y = firings[point_idx].distance *z_cos_altitude * sin(firings[point_idx].azimuth);
		z = firings[point_idx].distance *z_sin_altitude;
		
		// Compute the time of the point
		uint64_t point_time = packet_end_time - 1665*(POINTS_PER_PACKET - point_idx - 1);
		if(time_last < point_time && time_last > 0){
		  point_time = time_last + 1665;
		}
		time_last = point_time;
		
		PointXYZIT* point = pc->add_point();
          (point_time/1000000000.0);
		point->set_intensity(firings[point_idx].intensity);
		
		if (config_.calibration()) {
			ComputeCoords2(firings[point_idx].vertical_line, CH32, firings[point_idx].distance, corrections,
				  firings[point_idx].azimuth, point);
		} else {
        if((y >= config_.bottom_left_x() && y <= config_.top_right_x())
            && (-x >= config_.bottom_left_y() && -x <= config_.top_right_y()))  {
            point->set_x(nan);
            point->set_y(nan);
            point->set_z(nan);
            point->set_timestamp(point_time);
            point->set_intensity(0);
        }else {
            point->set_x(y);
            point->set_y(-x);
            point->set_z(z);
        }
		 }
	}
    
}


void LslidarCH32Parser::Order(std::shared_ptr<PointCloud> cloud) {
  int width = 32;
  cloud->set_width(width);
  int height = cloud->point_size() / cloud->width();
  cloud->set_height(height);

  std::shared_ptr<PointCloud> cloud_origin = std::make_shared<PointCloud>();
  cloud_origin->CopyFrom(*cloud);

  for (int i = 0; i < width; ++i) {
    int col = lslidar::ORDER_32[i];

    for (int j = 0; j < height; ++j) {
      // make sure offset is initialized, should be init at setup() just once
      int target_index = j * width + i;
      int origin_index = j * width + col;
      cloud->mutable_point(target_index)
          ->CopyFrom(cloud_origin->point(origin_index));
    }
  }
}

}  // namespace lslidar
}  // namespace drivers
}  // namespace apollo
