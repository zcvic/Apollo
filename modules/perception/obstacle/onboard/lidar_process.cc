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

#include "modules/perception/obstacle/onboard/lidar_process.h"
// #include <pcl/ros/conversions.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>
#include <Eigen/Core>
#include <string>
#include "gflags/gflags.h"

#include "modules/common/log.h"
#include "modules/perception/lib/config_manager/config_manager.h"
#include "ros/include/ros/ros.h"

namespace apollo {
namespace perception {

DEFINE_bool(enable_hdmap_input, false, "enable hdmap input for roi filter");
DEFINE_string(onboard_roi_filter, "DummyROIFilter", "onboard roi filter");
DEFINE_string(onboard_segmentor, "DummySegmentation", "onboard segmentation");
DEFINE_string(onboard_object_builder, "DummyObjectBuilder",
              "onboard object builder");
DEFINE_string(onboard_tracker, "DummyTracker", "onboard tracker");

DEFINE_int32(tf2_buff_in_ms, 10, "the tf2 buff size in ms");
DEFINE_string(lidar_tf2_frame_id, "world", "the tf2 transform frame id");
DEFINE_string(lidar_tf2_child_frame_id, "velodyne64",
              "the tf2 transform child frame id");

using pcl_util::Point;
using pcl_util::PointD;
using pcl_util::PointCloud;
using pcl_util::PointCloudPtr;
using pcl_util::PointIndices;
using pcl_util::PointIndicesPtr;
using Eigen::Matrix4d;
using Eigen::Affine3d;
using std::string;

bool LidarProcess::Init() {
  if (inited_) {
    return true;
  }
  if (!InitFrameDependence()) {
    AERROR << "failed to init frame dependence.";
    return false;
  }

  if (!InitAlgorithmPlugin()) {
    AERROR << "failed to init algorithm plugin.";
    return false;
  }

  inited_ = true;
  return true;
}

bool LidarProcess::Process(const sensor_msgs::PointCloud2& message,
                           std::vector<ObjectPtr>* tracked_objects) {
  if (!tracked_objects) {
    AERROR << "tracked_objects can not be NULL.";
    return false;
  }

  const double kTimeStamp = message.header.stamp.toSec();
  seq_num_++;

  /// get velodyne2world transfrom
  std::shared_ptr<Matrix4d> velodyne_trans = std::make_shared<Matrix4d>();
  if (!GetVelodyneTrans(kTimeStamp, velodyne_trans.get())) {
    AERROR << "failed to get trans at timestamp: " << kTimeStamp;
    return false;
  }

  PointCloudPtr point_cloud(new PointCloud);
  TransPointCloudToPCL(message, &point_cloud);

  /// call hdmap to get ROI
  HdmapStructPtr hdmap = nullptr;
  if (!hdmap_input_) {
    PointD velodyne_pose = {0.0, 0.0, 0.0, 0};  // (0,0,0)
    Affine3d temp_trans(*velodyne_trans);
    PointD velodyne_pose_world = pcl::transformPoint(velodyne_pose, temp_trans);
    hdmap.reset(new HdmapStruct);
    hdmap_input_->GetROI(velodyne_pose_world, hdmap);
  }

  /// call roi_filter
  PointCloudPtr roi_cloud(new PointCloud);
  if (roi_filter_ != nullptr) {
    PointIndicesPtr roi_indices(new PointIndices);
    ROIFilterOptions roi_filter_options;
    roi_filter_options.velodyne_trans = velodyne_trans;
    roi_filter_options.hdmap = hdmap;
    if (roi_filter_->Filter(point_cloud, roi_filter_options, roi_indices)) {
      pcl::copyPointCloud(*point_cloud, *roi_indices, *roi_cloud);
    } else {
      AERROR << "failed to call roi filter.";
      return false;
    }
  }

  /// call segmentor
  std::vector<ObjectPtr> objects;
  if (segmentor_ != nullptr) {
    SegmentationOptions segmentation_options;
    PointIndices non_ground_indices;
    non_ground_indices.indices.resize(roi_cloud->points.size());
    std::iota(non_ground_indices.indices.begin(),
              non_ground_indices.indices.end(), 0);
    if (!segmentor_->Segment(roi_cloud, non_ground_indices,
                             segmentation_options, &objects)) {
      AERROR << "failed to call segmention.";
      return false;
    }
  }

  /// call object builder
  if (object_builder_ != nullptr) {
    ObjectBuilderOptions object_builder_options;
    if (!object_builder_->Build(object_builder_options, &objects)) {
      AERROR << "failed to call object builder.";
      return false;
    }
  }

  /// call tracker
  if (tracker_ != nullptr) {
    TrackerOptions tracker_options;
    tracker_options.velodyne_trans = velodyne_trans;
    tracker_options.hdmap = hdmap;
    if (!tracker_->Track(objects, kTimeStamp, tracker_options,
                         tracked_objects)) {
      AERROR << "failed to call tracker.";
      return false;
    }
  }

  AINFO << "lidar process succ, there are " << tracked_objects->size()
        << " tracked objects.";
  return true;
}

bool LidarProcess::InitFrameDependence() {
  /// init config manager
  ConfigManager* config_manager = Singleton<ConfigManager>::Get();
  if (!config_manager) {
    AERROR << "failed to get ConfigManager instance.";
    return false;
  }
  if (!config_manager->Init()) {
    AERROR << "failed to init ConfigManager";
    return false;
  }
  AINFO << "Init config manager successfully, work_root: "
        << config_manager->work_root();

  /// init hdmap
  if (FLAGS_enable_hdmap_input) {
    hdmap_input_ = Singleton<HDMapInput>::Get();
    if (!hdmap_input_) {
      AERROR << "failed to get HDMapInput instance.";
      return false;
    }
    if (!hdmap_input_->Init()) {
      AERROR << "failed to init HDMapInput";
      return false;
    }
  }
  return true;
}

bool LidarProcess::InitAlgorithmPlugin() {
  /// init roi filter
  roi_filter_.reset(
      BaseROIFilterRegisterer::GetInstanceByName(FLAGS_onboard_roi_filter));
  if (!roi_filter_) {
    AERROR << "Failed to get instance: " << FLAGS_onboard_roi_filter;
    return false;
  }
  if (!roi_filter_->Init()) {
    AERROR << "Failed to init roi filter: " << roi_filter_->name();
    return false;
  }
  AINFO << "Init algorithm plugin successfully, roi_filter_: "
        << roi_filter_->name();

  /// init segmentation
  segmentor_.reset(
      BaseSegmentationRegisterer::GetInstanceByName(FLAGS_onboard_segmentor));
  if (!segmentor_) {
    AERROR << "Failed to get instance: " << FLAGS_onboard_segmentor;
    return false;
  }
  if (!segmentor_->Init()) {
    AERROR << "Failed to init segmentor: " << segmentor_->name();
    return false;
  }
  AINFO << "Init algorithm plugin successfully, segmentor: "
        << segmentor_->name();

  /// init object build
  object_builder_.reset(BaseObjectBuilderRegisterer::GetInstanceByName(
      FLAGS_onboard_object_builder));
  if (!object_builder_) {
    AERROR << "Failed to get instance: " << FLAGS_onboard_object_builder;
    return false;
  }
  if (!object_builder_->Init()) {
    AERROR << "Failed to init object builder: " << object_builder_->name();
    return false;
  }
  AINFO << "Init algorithm plugin successfully, object builder: "
        << object_builder_->name();

  /// init tracker
  tracker_.reset(
      BaseTrackerRegisterer::GetInstanceByName(FLAGS_onboard_tracker));
  if (!tracker_) {
    AERROR << "Failed to get instance: " << FLAGS_onboard_tracker;
    return false;
  }
  if (!tracker_->Init()) {
    AERROR << "Failed to init tracker: " << tracker_->name();
    return false;
  }
  AINFO << "Init algorithm plugin successfully, tracker: " << tracker_->name();

  return true;
}

void LidarProcess::TransPointCloudToPCL(
    const sensor_msgs::PointCloud2& in_msg, PointCloudPtr* out_cloud) {
  /*
  // transform from ros to pcl
  pcl::PointCloud<pcl_util::PointXYZIT> in_cloud;
  pcl::fromROSMsg(in_msg, in_cloud);
  // transform from xyzit to xyzi
  PointCloudPtr& cloud = *out_cloud;
  cloud->header = in_cloud.header;
  cloud->width = in_cloud.width;
  cloud->height = in_cloud.height;
  cloud->is_dense = in_cloud.is_dense;
  cloud->sensor_origin_ = in_cloud.sensor_origin_;
  cloud->sensor_orientation_ = in_cloud.sensor_orientation_;
  cloud->points.resize(in_cloud.points.size());
  for (size_t idx = 0; idx < in_cloud.size(); ++idx) {
    cloud->points[idx].x = in_cloud.points[idx].x;
    cloud->points[idx].y = in_cloud.points[idx].y;
    cloud->points[idx].z = in_cloud.points[idx].z;
    cloud->points[idx].intensity = in_cloud.points[idx].intensity;
  }
  */
}

bool LidarProcess::GetVelodyneTrans(const double query_time, Matrix4d* trans) {
  if (!trans) {
    AERROR << "failed to get trans, the trans ptr can not be NULL";
    return false;
  }

  /*
  ros::Time query_stamp(query_time);
  static tf2_ros::Buffer tf2_buffer;
  static tf2_ros::TransformListener tf2Listener(tf2_buffer);

  const double kTf2BuffSize = FLAGS_tf2_buff_in_ms / 1000.0;
  string err_msg;
  if (!tf2_buffer.canTransform(FLAGS_lidar_tf2_frame_id,
                               FLAGS_lidar_tf2_child_frame_id, query_stamp,
                               ros::Duration(kTf2BuffSize), &err_msg)) {
    AERROR << "Cannot transform frame: " << FLAGS_lidar_tf2_frame_id
           << " to frame " << FLAGS_lidar_tf2_child_frame_id
           << " , err: " << err_msg
           << ". Frames: " << tf2_buffer.allFramesAsString();
    return false;
  }

  geometry_msgs::TransformStamped transform_stamped;
  try {
    transform_stamped = tf2_buffer.lookupTransform(
        FLAGS_lidar_tf2_frame_id, FLAGS_lidar_tf2_child_frame_id, query_stamp);
  } catch (tf2::TransformException& ex) {
    AERROR << "Exception: " << ex.what();
    return false;
  }
  Affine3d affine_3d;
  tf::transformMsgToEigen(transform_stamped.transform, affine_3d);
  *trans = affine_3d.matrix();

  AINFO << "get " << FLAGS_lidar_tf2_frame_id << " to "
        << FLAGS_lidar_tf2_child_frame_id << " trans: " << *trans;
  */
  return true;
}

}  // namespace perception
}  // namespace apollo
