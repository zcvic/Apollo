/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
#include "modules/perception/camera/tools/offline/visualizer.h"

#include "cyber/common/log.h"

namespace apollo {
namespace perception {
namespace camera {

std::vector<cv::Scalar> colorlistobj = {
    cv::Scalar(0, 0, 255),     cv::Scalar(0, 100, 255),
    cv::Scalar(0, 200, 255),   cv::Scalar(100, 255, 255),
    cv::Scalar(200, 255, 255), cv::Scalar(255, 100, 255),
    cv::Scalar(255, 0, 255),   cv::Scalar(255, 255, 100),
    cv::Scalar(255, 255, 0),   cv::Scalar(255, 0, 100),
    cv::Scalar(255, 0, 0),     cv::Scalar(0, 255, 0),
    cv::Scalar(100, 255, 100)};

std::vector<cv::Vec3b> colorlistlane = {
    cv::Vec3b(0, 0, 255),     cv::Vec3b(0, 100, 255),
    cv::Vec3b(0, 200, 255),   cv::Vec3b(100, 255, 255),
    cv::Vec3b(200, 255, 255), cv::Vec3b(255, 100, 255),
    cv::Vec3b(255, 0, 255),   cv::Vec3b(255, 255, 100),
    cv::Vec3b(255, 255, 0),   cv::Vec3b(255, 0, 100),
    cv::Vec3b(255, 0, 0),     cv::Vec3b(0, 255, 0),
    cv::Vec3b(100, 255, 100)};

std::map<base::LaneLinePositionType, cv::Scalar> colormapline = {
    {base::LaneLinePositionType::UNKNOWN, cv::Scalar(0, 0, 255)},
    {base::LaneLinePositionType::FOURTH_LEFT, cv::Scalar(0, 100, 255)},
    {base::LaneLinePositionType::THIRD_LEFT, cv::Scalar(0, 200, 255)},
    {base::LaneLinePositionType::ADJACENT_LEFT, cv::Scalar(100, 255, 255)},
    {base::LaneLinePositionType::EGO_LEFT, cv::Scalar(200, 255, 255)},
    {base::LaneLinePositionType::EGO_CENTER, cv::Scalar(255, 100, 255)},
    {base::LaneLinePositionType::EGO_RIGHT, cv::Scalar(255, 0, 255)},
    {base::LaneLinePositionType::ADJACENT_RIGHT, cv::Scalar(255, 255, 100)},
    {base::LaneLinePositionType::THIRD_RIGHT, cv::Scalar(255, 255, 0)},
    {base::LaneLinePositionType::FOURTH_RIGHT, cv::Scalar(255, 0, 100)},
    {base::LaneLinePositionType::OTHER, cv::Scalar(255, 0, 0)},
    {base::LaneLinePositionType::CURB_LEFT, cv::Scalar(0, 255, 0)},
    {base::LaneLinePositionType::CURB_RIGHT, cv::Scalar(100, 255, 100)}};

Eigen::Matrix3d Camera2CarHomograph(Eigen::Matrix3d intrinsic,
                                    Eigen::Matrix4d extrinsic_camera2lidar,
                                    Eigen::Matrix4d extrinsic_lidar2imu,
                                    double pitch_adj) {
  AINFO << "intrinsic parameter of camera: " << intrinsic;
  AINFO << "extrinsic parameter of camera to lidar: " << extrinsic_camera2lidar;
  AINFO << "extrinsic parameter of lidar to imu: " << extrinsic_lidar2imu;
  // rotate 90 degree around z axis to make x point forward
  Eigen::Matrix4d Rz;
  Rz << 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;
  Eigen::Matrix4d extrinsic_camera2car;
  extrinsic_camera2car = extrinsic_camera2lidar * extrinsic_lidar2imu * Rz;
  // adjust pitch in camera coords
  Eigen::Matrix4d Rx;
  Rx << 1, 0, 0, 0, 0, cos(pitch_adj), -sin(pitch_adj), 0, 0, sin(pitch_adj),
      cos(pitch_adj), 0, 0, 0, 0, 1;
  extrinsic_camera2car = extrinsic_camera2car * Rx;
  AINFO << "extrinsic parameter from camera to car: " << extrinsic_camera2car;

  // compute the homography matrix, such that H [u, v, 1]' ~ [X_l, Y_l, 1]
  Eigen::Matrix3d K = intrinsic;
  Eigen::Matrix3d R = extrinsic_camera2car.block(0, 0, 3, 3);
  Eigen::Vector3d T = extrinsic_camera2car.block(0, 3, 3, 1);
  Eigen::Matrix3d H;

  H.block(0, 0, 3, 2) = (K * R.transpose()).block(0, 0, 3, 2);
  H.block(0, 2, 3, 1) = -K * R.transpose() * T;
  return H.inverse();
}

bool Visualizer::Init(const std::vector<std::string> &camera_names,
                      TransformServer *tf_server) {
  tf_server_ = tf_server;
  CHECK(tf_server_ != nullptr);
  last_timestamp_ = 0;
  small_h_ = static_cast<int>(image_height_ * scale_ratio_);
  small_w_ = static_cast<int>(image_width_ * scale_ratio_);
  world_h_ = 2 * small_h_;

  for (size_t i = 0; i < camera_names.size(); ++i) {
    camera_image_[camera_names[i]] =
        cv::Mat(small_h_, small_w_, CV_8UC3, cv::Scalar(0, 0, 0));
  }
  world_image_ = cv::Mat(world_h_, wide_pixel_, CV_8UC3, cv::Scalar(0, 0, 0));
  return true;
}

bool Visualizer::Init_all_info_single_camera(
    const std::string &camera_name,
    std::map<std::string, Eigen::Matrix3f> intrinsic_map,
    std::map<std::string, Eigen::Matrix4d> extrinsic_map,
    Eigen::Matrix4d ex_lidar2imu, double pitch_adj, int image_height,
    int image_width) {
  image_height_ = image_height;
  image_width_ = image_width;
  intrinsic_map_ = intrinsic_map;
  extrinsic_map_ = extrinsic_map;
  last_timestamp_ = 0;
  small_h_ = static_cast<int>(image_height_ * scale_ratio_);
  small_w_ = static_cast<int>(image_width_ * scale_ratio_);
  world_h_ = 2 * small_h_;

  camera_image_[camera_name + "_2D"] =
      cv::Mat(small_h_, small_w_, CV_8UC3, cv::Scalar(0, 0, 0));
  camera_image_[camera_name + "_3D"] =
      cv::Mat(small_h_, small_w_, CV_8UC3, cv::Scalar(0, 0, 0));
  world_image_ = cv::Mat(world_h_, wide_pixel_, CV_8UC3, cv::Scalar(0, 0, 0));
  draw_range_circle();

  ex_lidar2imu.block(0, 3, 3, 1) = -ex_lidar2imu.block(0, 3, 3, 1);
  extrinsic_map_.at(camera_name).block(0, 3, 3, 1) =
      -extrinsic_map_.at(camera_name).block(0, 3, 3, 1);
  homography_im2car_ = Camera2CarHomograph(
      intrinsic_map_.at(camera_name).cast<double>(),
      extrinsic_map_.at(camera_name), ex_lidar2imu, pitch_adj);
  AINFO << "homography_im2car_: " << homography_im2car_;

  // compute FOV points
  p_fov_1_.x = 0;
  p_fov_1_.y = static_cast<int>(image_height_ * fov_cut_ratio_);

  p_fov_2_.x = image_width_ - 1;
  p_fov_2_.y = static_cast<int>(image_height_ * fov_cut_ratio_);

  p_fov_3_.x = 0;
  p_fov_3_.y = image_height_ - 1;

  p_fov_4_.x = image_width_ - 1;
  p_fov_4_.y = image_height_ - 1;

  return true;
}

void Visualizer::SetDirectory(const std::string &path) {
  std::string command;
  command = "mkdir -p " + path;
  system(command.c_str());
  command = "rm " + path + "/*.jpg";
  system(command.c_str());
  path_ = path;
}

std::string Visualizer::type_to_string(
    const apollo::perception::base::ObjectType type) {
  switch (type) {
    case apollo::perception::base::ObjectType::UNKNOWN:
      return "UNKN";
    case apollo::perception::base::ObjectType::UNKNOWN_MOVABLE:
      return "U_MO";
    case apollo::perception::base::ObjectType::UNKNOWN_UNMOVABLE:
      return "UNMO";
    case apollo::perception::base::ObjectType::PEDESTRIAN:
      return "PED";
    case apollo::perception::base::ObjectType::BICYCLE:
      return "CYC";
    case apollo::perception::base::ObjectType::VEHICLE:
      return "VEH";
    default:
      return "WRNG";
  }
  return "WRNG";
}

std::string Visualizer::sub_type_to_string(
    const apollo::perception::base::ObjectSubType type) {
  switch (type) {
    case apollo::perception::base::ObjectSubType::UNKNOWN:
      return "UNKN";
    case apollo::perception::base::ObjectSubType::UNKNOWN_MOVABLE:
      return "U_MO";
    case apollo::perception::base::ObjectSubType::UNKNOWN_UNMOVABLE:
      return "UNMO";
    case apollo::perception::base::ObjectSubType::CAR:
      return "CAR";
    case apollo::perception::base::ObjectSubType::VAN:
      return "VAN";
    case apollo::perception::base::ObjectSubType::TRUCK:
      return "TRUC";
    case apollo::perception::base::ObjectSubType::BUS:
      return "BUS";
    case apollo::perception::base::ObjectSubType::CYCLIST:
      return "CYC";
    case apollo::perception::base::ObjectSubType::MOTORCYCLIST:
      return "MCYC";
    case apollo::perception::base::ObjectSubType::TRICYCLIST:
      return "TCYC";
    case apollo::perception::base::ObjectSubType::PEDESTRIAN:
      return "PED";
    case apollo::perception::base::ObjectSubType::TRAFFICCONE:
      return "CONE";
    default:
      return "WRNG";
  }
  return "WRNG";
}

void Visualizer::Draw2Dand3D(const cv::Mat &img, const CameraFrame &frame) {
  cv::Mat image = img.clone();
  Eigen::Affine3d pose;
  if (!tf_server_->QueryPos(frame.timestamp, &pose)) {
    pose.setIdentity();
  }
  Eigen::Affine3d lidar2novatel;
  tf_server_->QueryTransform("velodyne128", "novatel", &lidar2novatel);
  Eigen::Affine3d lidar2world = pose * lidar2novatel;
  Eigen::Affine3d world2lidar = lidar2world.inverse();
  for (const auto &object : frame.tracked_objects) {
    base::RectF rect(object->camera_supplement.box);
    cv::Rect r(static_cast<int>(rect.x), static_cast<int>(rect.y),
               static_cast<int>(rect.width), static_cast<int>(rect.height));
    cv::rectangle(image, r,
                  colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::putText(image, std::to_string(object->track_id),
                cv::Point(static_cast<int>(rect.x), static_cast<int>(rect.y)),
                cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(0, 0, 255), 2);
    Eigen::Vector3d theta;
    theta << cos(object->theta), sin(object->theta), 0;
    theta = world2lidar.linear() * theta;
    float yaw = static_cast<float>(atan2(theta[1], theta[0]));
    Eigen::Matrix2d rotate;
    rotate << cos(yaw), -sin(yaw), sin(yaw), cos(yaw);

    Eigen::Vector3d pos;
    pos << object->center[0], object->center[1], object->center[2];
    pos = world2lidar * pos;
    Eigen::Vector2d pos_2d;
    pos_2d << pos[0], pos[1];
    Eigen::Vector3d v;
    v << object->velocity[0], object->velocity[1], object->velocity[2];
    v = world2lidar.linear() * v;
    Eigen::Vector2d v_2d;
    v_2d << v[0] + pos_2d[0], v[1] + pos_2d[1];
    Eigen::Vector2d p1;
    p1 << object->size[0] * 0.5, object->size[1] * 0.5;
    p1 = rotate * p1 + pos_2d;
    Eigen::Vector2d p2;
    p2 << -object->size[0] * 0.5, object->size[1] * 0.5;
    p2 = rotate * p2 + pos_2d;
    Eigen::Vector2d p3;
    p3 << -object->size[0] * 0.5, -object->size[1] * 0.5;
    p3 = rotate * p3 + pos_2d;
    Eigen::Vector2d p4;
    p4 << object->size[0] * 0.5, -object->size[1] * 0.5;
    p4 = rotate * p4 + pos_2d;

    cv::line(world_image_, world_point_to_bigimg(p1), world_point_to_bigimg(p2),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(world_image_, world_point_to_bigimg(p2), world_point_to_bigimg(p3),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(world_image_, world_point_to_bigimg(p3), world_point_to_bigimg(p4),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(world_image_, world_point_to_bigimg(p4), world_point_to_bigimg(p1),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(world_image_, world_point_to_bigimg(pos_2d),
             world_point_to_bigimg(v_2d),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
  }
  last_timestamp_ = frame.timestamp;
  camera_image_[frame.data_provider->sensor_name()] = image;
  cv::resize(image, camera_image_[frame.data_provider->sensor_name()],
             cv::Size(small_w_, small_h_));
}

void Visualizer::ShowResult(const cv::Mat &img, const CameraFrame &frame) {
  cv::Mat image = img.clone();
  std::string camera_name = frame.data_provider->sensor_name();

  if (frame.timestamp - last_timestamp_ > 0.02) {
    cv::Mat bigimg(world_h_, small_w_ + wide_pixel_, CV_8UC3);
    camera_image_["front_6mm"].copyTo(
        bigimg(cv::Rect(0, 0, small_w_, small_h_)));
    camera_image_["front_12mm"].copyTo(
        bigimg(cv::Rect(0, small_h_, small_w_, small_h_)));
    world_image_.copyTo(bigimg(cv::Rect(small_w_, 0, wide_pixel_, world_h_)));
    if (write_out_img_) {
      char path[1000];
      snprintf(path, sizeof(path), "%s/%06d.jpg", path_.c_str(),
               frame.frame_id);
      AINFO << path;
      cv::imwrite(path, bigimg);
    }

    if (cv_imshow_img_) {
      cv::imshow("", bigimg);
      cvWaitKey(30);
    }
    world_image_ = cv::Mat(world_h_, wide_pixel_, CV_8UC3, cv::Scalar(0, 0, 0));
    draw_range_circle();
  }

  cv::putText(image, "timestamp:" + std::to_string(frame.timestamp),
              cv::Point(10, 50), cv::FONT_HERSHEY_DUPLEX, 1.3,
              cv::Scalar(0, 0, 255), 3);
  cv::putText(image, "camera_name: " + camera_name, cv::Point(10, 100),
              cv::FONT_HERSHEY_DUPLEX, 1.3, cv::Scalar(0, 0, 255), 3);
  cv::putText(image, "frame id: " + std::to_string(frame.frame_id),
              cv::Point(10, 150), cv::FONT_HERSHEY_DUPLEX, 1.3,
              cv::Scalar(0, 0, 255), 3);
  Draw2Dand3D(image, frame);
}

void Visualizer::Draw2Dand3D_all_info_single_camera(const cv::Mat &img,
                                                    const CameraFrame &frame,
                                                    Eigen::Matrix3d intrinsic,
                                                    Eigen::Matrix4d extrinsic) {
  cv::Mat image_2D = img.clone();

  // plot FOV
  cv::line(image_2D, p_fov_1_, p_fov_2_, cv::Scalar(255, 255, 255), 2);
  cv::line(image_2D, p_fov_1_, p_fov_3_, cv::Scalar(255, 255, 255), 2);
  cv::line(image_2D, p_fov_2_, p_fov_4_, cv::Scalar(255, 255, 255), 2);

  cv::line(world_image_, world_point_to_bigimg(image2ground(p_fov_1_)),
           world_point_to_bigimg(image2ground(p_fov_2_)),
           cv::Scalar(255, 255, 255), 2);
  cv::line(world_image_, world_point_to_bigimg(image2ground(p_fov_1_)),
           world_point_to_bigimg(image2ground(p_fov_3_)),
           cv::Scalar(255, 255, 255), 2);
  cv::line(world_image_, world_point_to_bigimg(image2ground(p_fov_2_)),
           world_point_to_bigimg(image2ground(p_fov_4_)),
           cv::Scalar(255, 255, 255), 2);

  AINFO << "FOV point 1: " << image2ground(p_fov_1_);
  AINFO << "FOV point 2: " << image2ground(p_fov_2_);
  AINFO << "FOV point 3: " << image2ground(p_fov_3_);
  AINFO << "FOV point 4: " << image2ground(p_fov_4_);
  cv::Mat image_3D = image_2D.clone();

  // plot lane map overlaid on image_2D
  int lane_map_width = frame.lane_detected_blob->width();
  int lane_map_height = frame.lane_detected_blob->height();
  cv::Mat lane_map(lane_map_height, lane_map_width, CV_32FC1);
  lane_map.setTo(cv::Scalar(0));
  memcpy(lane_map.data, frame.lane_detected_blob->cpu_data(),
         lane_map_width * lane_map_height * sizeof(float));
  for (int yi = lane_map.rows - 1; yi >= 0; yi--) {
    for (int xi = 0; xi < lane_map.cols; xi++) {
      int line_idx = static_cast<int>(std::round(lane_map.at<float>(yi, xi)));
      if (line_idx >= 1) {
        image_2D.at<cv::Vec3b>(
            static_cast<int>(yi * roi_height_ / lane_map.rows + roi_start_),
            static_cast<int>(xi * roi_width_ / lane_map.cols)) =
            colorlistlane[static_cast<int>(line_idx)];
      }
    }
  }

  // plot laneline on image and ground plane
  for (const auto &object : frame.lane_objects) {
    cv::Scalar lane_color = colormapline[object.pos_type];
    cv::Point p_prev;
    p_prev.x = static_cast<int>(object.curve_image_point_set[0].x);
    p_prev.y = static_cast<int>(object.curve_image_point_set[0].y);
    Eigen::Vector2d p_prev_ground = image2ground(p_prev);

    for (unsigned i = 1; i < object.curve_image_point_set.size(); i++) {
      cv::Point p_cur;
      p_cur.x = static_cast<int>(object.curve_image_point_set[i].x);
      p_cur.y = static_cast<int>(object.curve_image_point_set[i].y);
      Eigen::Vector2d p_cur_ground = image2ground(p_cur);

      cv::line(image_3D, p_prev, p_cur, lane_color, 2);
      cv::line(world_image_, world_point_to_bigimg(p_prev_ground),
               world_point_to_bigimg(p_cur_ground), lane_color, 2);
      p_prev = p_cur;
      p_prev_ground = p_cur_ground;
    }
  }

  for (const auto &object : frame.tracked_objects) {
    // plot 2D box on image_2D
    base::RectF rect(object->camera_supplement.box);
    cv::Rect r(static_cast<int>(rect.x), static_cast<int>(rect.y),
               static_cast<int>(rect.width), static_cast<int>(rect.height));
    cv::rectangle(image_2D, r,
                  colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::putText(image_2D, std::to_string(object->track_id),
                cv::Point(static_cast<int>(rect.x), static_cast<int>(rect.y)),
                cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(0, 0, 255), 2);

    cv::putText(
        image_2D,
        // type_to_string(object->type) + "->" +
        sub_type_to_string(object->sub_type),
        cv::Point(static_cast<int>(rect.x), static_cast<int>(rect.y) + 30),
        cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(255, 0, 0), 1);

    // compute 8 vetices in camera coodinates
    Eigen::Vector3d pos;
    pos << object->camera_supplement.local_center[0],
        object->camera_supplement.local_center[1],
        object->camera_supplement.local_center[2];
    double theta_ray = atan2(pos[0], pos[2]);
    double theta = object->camera_supplement.alpha + theta_ray;

    Eigen::Matrix3d rotate_ry;
    rotate_ry << cos(theta), 0, sin(theta), 0, 1, 0, -sin(theta), 0, cos(theta);
    std::vector<Eigen::Vector3d> p(8);
    p[0] << object->size[0] * 0.5, object->size[2] * 0.5, object->size[1] * 0.5;
    p[1] << -object->size[0] * 0.5, object->size[2] * 0.5,
        object->size[1] * 0.5;
    p[2] << -object->size[0] * 0.5, object->size[2] * 0.5,
        -object->size[1] * 0.5;
    p[3] << object->size[0] * 0.5, object->size[2] * 0.5,
        -object->size[1] * 0.5;
    p[4] << object->size[0] * 0.5, -object->size[2] * 0.5,
        object->size[1] * 0.5;
    p[5] << -object->size[0] * 0.5, -object->size[2] * 0.5,
        object->size[1] * 0.5;
    p[6] << -object->size[0] * 0.5, -object->size[2] * 0.5,
        -object->size[1] * 0.5;
    p[7] << object->size[0] * 0.5, -object->size[2] * 0.5,
        -object->size[1] * 0.5;
    for (uint i = 0; i < p.size(); i++) p[i] = rotate_ry * p[i] + pos;

    // compute 4 bottom vetices in lidar coordinate
    // Eigen::Vector3d p1_l_3d = extrinsic.block(0,0,3,3) * p1 +
    //                           extrinsic.block(0,3,3,1);
    // Eigen::Vector3d p2_l_3d = extrinsic.block(0,0,3,3) * p2 +
    //                           extrinsic.block(0,3,3,1);
    // Eigen::Vector3d p3_l_3d = extrinsic.block(0,0,3,3) * p3 +
    //                           extrinsic.block(0,3,3,1);
    // Eigen::Vector3d p4_l_3d = extrinsic.block(0,0,3,3) * p4 +
    //                           extrinsic.block(0,3,3,1);

    // Eigen::Vector2d p1_l = p1_l_3d.block(0, 0, 2, 1);
    // Eigen::Vector2d p2_l = p2_l_3d.block(0, 0, 2, 1);
    // Eigen::Vector2d p3_l = p3_l_3d.block(0, 0, 2, 1);
    // Eigen::Vector2d p4_l = p4_l_3d.block(0, 0, 2, 1);

    // compute obstacle center in lidar ground
    cv::Point c_2D;
    c_2D.x = static_cast<int>(rect.x + rect.width / 2);
    c_2D.y = static_cast<int>(rect.y + rect.height);
    Eigen::Vector2d c_2D_l = image2ground(c_2D);
    Eigen::Matrix2d rotate_rz;
    theta = theta - M_PI_2;
    rotate_rz << cos(theta), sin(theta), -sin(theta), cos(theta);
    // plot obstacles on ground plane in lidar coordinates
    Eigen::Vector2d p1_l;
    p1_l << object->size[0] * 0.5, object->size[1] * 0.5;
    p1_l = rotate_rz * p1_l + c_2D_l;
    Eigen::Vector2d p2_l;
    p2_l << -object->size[0] * 0.5, object->size[1] * 0.5;
    p2_l = rotate_rz * p2_l + c_2D_l;
    Eigen::Vector2d p3_l;
    p3_l << -object->size[0] * 0.5, -object->size[1] * 0.5;
    p3_l = rotate_rz * p3_l + c_2D_l;
    Eigen::Vector2d p4_l;
    p4_l << object->size[0] * 0.5, -object->size[1] * 0.5;
    p4_l = rotate_rz * p4_l + c_2D_l;
    cv::line(world_image_, world_point_to_bigimg(p1_l),
             world_point_to_bigimg(p2_l),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(world_image_, world_point_to_bigimg(p2_l),
             world_point_to_bigimg(p3_l),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(world_image_, world_point_to_bigimg(p3_l),
             world_point_to_bigimg(p4_l),
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(world_image_, world_point_to_bigimg(p4_l),
             world_point_to_bigimg(p1_l),
             colorlistobj[object->track_id % colorlistobj.size()], 2);

    // plot projected 3D box on image_3D
    for (uint i = 0; i < p.size(); i++) p[i] = intrinsic * p[i];

    std::vector<cv::Point> p_proj(8);
    for (uint i = 0; i < p_proj.size(); i++) {
      p_proj[i].x = static_cast<int>(p[i][0] / p[i][2]);
      p_proj[i].y = static_cast<int>(p[i][1] / p[i][2]);
    }

    cv::line(image_3D, p_proj[0], p_proj[1],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[1], p_proj[2],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[2], p_proj[3],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[3], p_proj[0],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[4], p_proj[5],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[5], p_proj[6],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[6], p_proj[7],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[7], p_proj[4],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[0], p_proj[4], cv::Scalar(255, 255, 255), 2);
    cv::line(image_3D, p_proj[1], p_proj[5],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[2], p_proj[6],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
    cv::line(image_3D, p_proj[3], p_proj[7],
             colorlistobj[object->track_id % colorlistobj.size()], 2);
  }
  last_timestamp_ = frame.timestamp;
  camera_image_[frame.data_provider->sensor_name() + "_2D"] = image_2D;
  cv::resize(image_2D,
             camera_image_[frame.data_provider->sensor_name() + "_2D"],
             cv::Size(small_w_, small_h_));
  camera_image_[frame.data_provider->sensor_name() + "_3D"] = image_3D;
  cv::resize(image_3D,
             camera_image_[frame.data_provider->sensor_name() + "_3D"],
             cv::Size(small_w_, small_h_));
}

void Visualizer::ShowResult_all_info_single_camera(const cv::Mat &img,
                                                   const CameraFrame &frame) {
  if (frame.timestamp - last_timestamp_ < 0.02) return;

  // draw results on visulization panel
  cv::Mat image = img.clone();
  std::string camera_name = frame.data_provider->sensor_name();
  cv::putText(image, "timestamp:" + std::to_string(frame.timestamp),
              cv::Point(10, 50), cv::FONT_HERSHEY_DUPLEX, 1.3,
              cv::Scalar(0, 0, 255), 3);
  cv::putText(image, "camera_name: " + camera_name, cv::Point(10, 100),
              cv::FONT_HERSHEY_DUPLEX, 1.3, cv::Scalar(0, 0, 255), 3);
  cv::putText(image, "frame id: " + std::to_string(frame.frame_id),
              cv::Point(10, 150), cv::FONT_HERSHEY_DUPLEX, 1.3,
              cv::Scalar(0, 0, 255), 3);
  if (intrinsic_map_.find(camera_name) != intrinsic_map_.end() &&
      extrinsic_map_.find(camera_name) != extrinsic_map_.end()) {
    Draw2Dand3D_all_info_single_camera(
        image, frame, intrinsic_map_.at(camera_name).cast<double>(),
        extrinsic_map_.at(camera_name));
  } else {
    AERROR << "fail to find necessuary intrinsic or extrinsic params.";
  }

  // copy visual results into visualization panel
  cv::Mat bigimg(world_h_, small_w_ + wide_pixel_, CV_8UC3);
  camera_image_[camera_name + "_2D"].copyTo(
      bigimg(cv::Rect(0, 0, small_w_, small_h_)));
  camera_image_[camera_name + "_3D"].copyTo(
      bigimg(cv::Rect(0, small_h_, small_w_, small_h_)));
  world_image_.copyTo(bigimg(cv::Rect(small_w_, 0, wide_pixel_, world_h_)));

  // output visualization panel
  if (write_out_img_) {
    char path[1000];
    snprintf(path, sizeof(path), "%s/%06d.jpg", path_.c_str(), frame.frame_id);
    AINFO << path;
    cv::imwrite(path, bigimg);
  }

  if (cv_imshow_img_) {
    cv::imshow("", bigimg);
    cvWaitKey(30);
  }

  // re-initialize empty world_image_
  world_image_ = cv::Mat(world_h_, wide_pixel_, CV_8UC3, cv::Scalar(0, 0, 0));
  draw_range_circle();
}

void Visualizer::draw_range_circle() {
  cv::Scalar color(255, 100, 0);
  cv::circle(world_image_, cv::Point(wide_pixel_ / 2, world_h_), 1 * m2pixel_,
             color, 1);
  for (int i = 20; i < 300; i += 20) {
    cv::circle(world_image_, cv::Point(wide_pixel_ / 2, world_h_), i * m2pixel_,
               color, 2);
  }
  for (int i = 50; i < 300; i += 50) {
    cv::putText(world_image_, std::to_string(i),
                cv::Point(wide_pixel_ / 2, world_h_ - i * m2pixel_),
                cv::FONT_HERSHEY_DUPLEX, 1, cv::Scalar(0, 0, 255), 2);
  }
}

cv::Point Visualizer::world_point_to_bigimg(const Eigen::Vector2d &p) {
  cv::Point point;
  point.x = static_cast<int>(-p[1] * m2pixel_ + wide_pixel_ * 0.5);
  point.y = static_cast<int>(world_h_ - p[0] * m2pixel_);
  return point;
}

Eigen::Vector2d Visualizer::image2ground(cv::Point p_img) {
  Eigen::Vector3d p_homo;
  p_homo << p_img.x, p_img.y, 1;
  Eigen::Vector3d p_ground;
  p_ground = homography_im2car_ * p_homo;
  p_ground[0] = p_ground[0] / p_ground[2];
  p_ground[1] = p_ground[1] / p_ground[2];
  return p_ground.block(0, 0, 2, 1);
}
}  // namespace camera
}  // namespace perception
}  // namespace apollo
