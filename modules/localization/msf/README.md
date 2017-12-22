# Multi-sensor Fusion Localization

## Introduction
  The goal of multi-sensor localization is to provide a robust method which can achieve high localization accuracy and resilience in challenging scenes, such as urban downtown, highways, and tunnels. It adaptively uses information from complementary sensors such as GNSS, LiDAR and IMU. If you want to know more details, please refer to our paper [Robust and Precise Vehicle Localization based on Multi-sensor Fusion in Diverse City Scenes] (https://arxiv.org/abs/1711.05805).

## Input
  * Point cloud data from LiDAR sensor (ROS topic /apollo/sensor/velodyne64/compensator/PointCloud2)
  * Gnss observation and ephemeris data from Gnss sensor. The corresponding ROS topics are `/apollo/sensor/gnss/rtk_obs` and `/apollo/sensor/gnss/rtk_eph`, and the definition be found in `drivers/gnss/proto/gnss_raw_observation.proto`.
  * Gnss best pose from Gnss sensor. The corresponding ROS topic is `/apollo/sensor/gnss/best_pose`, and the definition can be found in `drivers/gnss/proto/gnss_best_pose.proto`.
  * Imu data from IMU sensor. The corresponding ROS topic is `/apollo/sensor/gnss/corrected_imu`, and the definition can be found in `localization/proto/imu.proto`.
  * Localization map (FLAGS_map_dir + "/" + FLAGS_local_map_name)

## Output
  * IMU pose defined by Protobuf message `LocalizationEstimate`, and the corresponding proto is `localization/proto/localization.proto`. (ROS topic `/apollo/localization/pose`)
  * We also provide intermediate results: Gnss localization result, LiDAR localization result. (ROS topic `/apollo/localization/msf_gnss`, `/apollo/localization/msf_lidar`)

## Modes

### MSF Localization Modes
  We provide 4 modes for msf localization module. 3-Systems uses gnss localization result all the time, while 2-Systems only apply gnss localization result to initilize SINS alignment.
  1. 3-Systems(BestGnss + LiDAR + SINS): gnss_mode(0), gnss_only_init(false).
  2. 3-Systems(Local-Gnss + LiDAR + SINS): gnss_mode(1), gnss_only_init(false).
  3. 2-Systems(BestGnss + LiDAR + SINS):gnss_mode(0), gnss_only_init(true).
  4. 2-Systems(Local-Gnss + LiDAR + SINS): gnss_mode(1), gnss_only_init(true).

### GNSS Localization Setting
  This module works in two modes. One is directly to use gnss best pose (`/apollo/sensor/gnss/best_pose`) from INS system, such as Novatel system. In the second mode, the initial gnss observations and ephemeris information (`/apollo/sensor/gnss/rtk_obs` and `/apollo/sensor/gnss/rtk_eph`) are used to calculate gnss localization result.

  You can set `gnss_mode` in `apollo/localization/conf/localization.conf` to decide which mode you want to use. The default mode is gnss best pose.

### LiDAR Localization Setting
  We provide three modes of LiDAR localization: intensity, altitude, and fusion. You can set the parameter `lidar_localization_mode` in file localization.conf to choose the mode. Considering computing ability of different platforms, we provide `lidar_filter_size`, `lidar_thread_num` and `point_cloud_step` to adjust the computation cost.

## Gnerate Localization Map
  Localization map is used for LiDAR-based localization, which is a grid-cell representation of the environment. Each cell stores the statistics of laser reflection intensity and altitude. The map is organized as a group of map nodes. For more information refer to `apollo/modules/localization/msf/local_map`.

  We provide a script (`apollo/scripts/msf_local_map.sh`) to generate localization map. You need to provide a group of point cloud frames (as .pcd file), corresponding poses file, and UTM zone id. The format of the poses file is `pcd_number timestamp x y z qx qy qz qw`.

## Visualization Tool
  We provide a simple online visulazation tool for debug multi-sensor fusion localization module. The parameter `use_visualize` in localizaiton.conf is used to enable the online visulazation tool.

  We also provide an offline visulazation tool which can show localization map, point cloud, horizontal position and variance circle of LiDAR-localization, Gnss-localization, and fused localization results. Since this visualization tool is an offline version, you need to record rosbag and use our script (`apollo/scripts/msf_monitor_data_exporter.sh`) to decode it to pcd and pose files. The localization results contains three files (gnss_loc.txt, lidar_loc.txt, and fusion_loc.txt)  corresponding to the localization result of Gnss, Lidar, and fusion.

  After encoding rosbag file, the script (`apollo/scripts/msf_local_visualization.sh`) to visualize the localization map, point cloud and localization results. You need to set the encoded directory to visualization tool. The directory should include `velodyne64_novatel_extrinsics_example.yaml`,  `local_map`, `pcd` and gnss_loc.txt, lidar_loc.txt fusion_loc.txt.
