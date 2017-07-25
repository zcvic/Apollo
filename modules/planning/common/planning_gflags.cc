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

#include "modules/planning/common/planning_gflags.h"

DEFINE_int32(planning_loop_rate, 5, "Loop rate for planning node");

DEFINE_string(rtk_trajectory_filename, "modules/planning/data/garage.csv",
              "Loop rate for planning node");
DEFINE_string(map_filename, "modules/map/data/base_map.txt",
              "map data file");

DEFINE_uint64(rtk_trajectory_backward, 10,
              "The number of points to be included in RTK trajectory "
              "before the matched point");

DEFINE_uint64(rtk_trajectory_forward, 800,
              "The number of points to be included in RTK trajectory "
              "after the matched point");

DEFINE_double(replanning_threshold, 2.0,
              "The threshold of position deviation "
              "that triggers the planner replanning");

DEFINE_double(trajectory_resolution, 0.01,
              "The time resolution of "
              "output trajectory.");

DEFINE_double(cycle_duration_in_sec, 0.002, "# of seconds per planning cycle.");
DEFINE_double(maximal_delay_sec, 0.005, "# of seconds for delay.");

DEFINE_int32(max_history_result, 10,
             "The maximal number of result in history.");

DEFINE_int32(max_frame_size, 30, "max size for prediction window");

DEFINE_int32(state_fail_threshold, 5,
             "This is continuous fail threshold for FSM change to fail state.");

DEFINE_int32(object_table_obstacle_capacity, 200,
             "The number of obstacles we hold in the object table.");
DEFINE_int32(object_table_map_object_capacity, 200,
             "The number of map objects we hold in the object table.");

DEFINE_bool(use_stitch, true, "Use trajectory stitch if possible.");
DEFINE_double(forward_predict_time, 0.2,
              "The forward predict time in each planning cycle.");
DEFINE_double(replan_distance_threshold, 5.0,
              "The distance threshold of replan");
DEFINE_double(replan_s_threshold, 5.0,
              "The s difference to real position threshold of replan");
DEFINE_double(replan_l_threshold, 2.5,
              "The l difference to real position threshold of replan");

DEFINE_double(planning_distance, 100, "Planning distance");

DEFINE_double(trajectory_time_length, 8.0, "Trajectory time length");
DEFINE_double(trajectory_time_resolution, 0.1,
              "Trajectory time resolution in planning");
DEFINE_double(output_trajectory_time_resolution, 0.05,
              "Trajectory time resolution when publish");

DEFINE_double(speed_lower_bound, 0.0, "The lowest speed allowed.");
DEFINE_double(speed_upper_bound, 40.0, "The highest speed allowed.");

DEFINE_double(longitudinal_acceleration_lower_bound, -4.5,
              "The lowest longitudinal acceleration allowed.");
DEFINE_double(longitudinal_acceleration_upper_bound, 4.0,
              "The highest longitudinal acceleration allowed.");

DEFINE_double(lateral_acceleration_bound, 4.5,
              "Bound of lateral acceleration; symmetric for left and right");
DEFINE_double(lateral_jerk_bound, 4.0,
              "Bound of lateral jerk; symmetric for left and right");

DEFINE_double(longitudinal_jerk_lower_bound, -4.0,
              "The lower bound of longitudinal jerk.");
DEFINE_double(longitudinal_jerk_upper_bound, 4.0,
              "The upper bound of longitudinal jerk.");

DEFINE_double(default_active_set_eps_num, 1e-7,
              "qpOases wrapper error control numerator");
DEFINE_double(default_active_set_eps_den, 1e-7,
              "qpOases wrapper error control numerator");
DEFINE_double(default_active_set_eps_iter_ref, 1e-7,
              "qpOases wrapper error control numerator");
DEFINE_bool(default_enable_active_set_debug_info, false,
            "Enable print information");

DEFINE_double(stgraph_default_point_cost, 1e10,
              "The default stgraph point cost.");
DEFINE_double(stgraph_max_acceleration_divide_factor_level_1, 2.0,
              "The divide factor for max acceleration at level 1.");
DEFINE_double(stgraph_max_deceleration_divide_factor_level_1, 3.0,
              "The divide factor for max deceleration at level 1.");
DEFINE_double(stgraph_max_deceleration_divide_factor_level_2, 2.0,
              "The divide factor for max deceleration at level 2.");

// Prediction Part

DEFINE_int32(adc_id, -1, "Obstacle id for the global adc");
DEFINE_bool(local_debug, false, "enabling local debug will disabling xlogging");
DEFINE_string(new_pobs_dump_file, "/data/new_pobs_dump.bag",
              "bag file for newly merged pobs");
DEFINE_double(pcd_merge_thred, 0.25, "iou threshold for pcd merge");
DEFINE_double(vehicle_min_l, 3.5, "vehicle min l");
DEFINE_double(vehicle_min_w, 2.2, "vehicle min w");
DEFINE_double(vehicle_min_h, 1.5, "vehicle min h");
DEFINE_int32(good_pcd_size, 50,
             "at least these many points to make a reasonable pcd");
DEFINE_int32(good_pcd_labeling_size, 500,
             "at least these many points for labeling");
DEFINE_double(front_sector_angle, 0.5236, "40 degree front sector");
DEFINE_double(max_reasonable_l, 15.0, "max reasonable l");
DEFINE_double(min_reasonable_l, 1.5, "min reasonable l");
DEFINE_double(max_reasonable_w, 3.5, "max reasonable w");
DEFINE_double(min_reasonable_w, 1.2, "min reasonable w");
DEFINE_double(
    merge_range, 30.0,
    "Radius of range for considering obstacle-merge centering at ADC");
DEFINE_double(pcd_labeling_range, 30.0,
              "Radius of range for labeling pcd centering at ADC");
DEFINE_bool(enable_merge_obstacle, true,
            "enabling local debug will disabling xlogging");
DEFINE_bool(enable_new_pobs_dump, true,
            "dump merged perception obstacles to a bag file");
DEFINE_bool(enable_pcd_labeling, false, "enabling generate pcd labeling image");
DEFINE_string(pcd_image_file_prefix, "/home/liyun/pcd_data/prediction_pcd_",
              "prefix for prediction generated pcd files");

DEFINE_double(prediction_total_time, 5.0, "Total prediction time");
DEFINE_double(prediction_pedestrian_total_time, 10.0,
              "Total prediction time for pedestrians");
DEFINE_double(prediction_model_time, 3.0, "Total prediction modeling time");
DEFINE_double(prediction_freq, 0.1, "Prediction frequency.");

// The following gflags are for internal use
DEFINE_bool(enable_unknown_obstacle, true, "Disable unknown obstacle.");
DEFINE_bool(enable_output_to_screen, false, "Enable output to screen");
DEFINE_bool(enable_EKF_tracking, false, "Enable tracking based on EKF");
DEFINE_bool(enable_acc, true, "Enable calculating speed by acc");
DEFINE_bool(enable_pedestrian_acc, false, "Enable calculating speed by acc");

// Cyclist UKF
DEFINE_double(
    road_heading_impact, 0.5,
    "Influence of the road heading (when available) in the state update");
DEFINE_double(process_noise_diag, 0.01,
              "Diagonal for process noise of cyclist UKF");
DEFINE_double(measurement_noise_diag, 0.01,
              "Diagonal for measurement noise of cyclist UKF");
DEFINE_double(state_covariance_diag_init, 0.01,
              "Diagonal for initial state noise of cyclist UKF");
DEFINE_double(ukf_alpha, 0.001, "Alpha parameter in cyclist UKF model.");
DEFINE_double(ukf_beta, 2.0, "Beta parameter in cyclist UKF model.");
DEFINE_double(ukf_kappa, 0.0, "Kappa parameter in cyclist UKF model.");

DEFINE_int32(stored_frames, 1, "The stored frames before predicting");
DEFINE_double(min_acc, -4.0, "Minimum acceleration");
DEFINE_double(max_acc, 2.0, "Maximum acceleration");
DEFINE_double(min_pedestrian_acc, -4.0, "minimum pedestrian acceleration");
DEFINE_double(max_pedestrian_acc, 2.0, "maximum pedestrian acceleration");
DEFINE_double(default_heading, 6.2831, "Defalut heading");
DEFINE_int32(max_obstacle_size, 1000, "Maximum number of obstacles");
DEFINE_double(max_prediction_length, 100.0,
              "Maximum trajectory length for any obstacle");
DEFINE_double(lane_search_radius, 3.0,
              "The radius for searching candidate lanes");
DEFINE_double(junction_search_radius, 10.0,
              "The radius for searching candidate junctions.");
DEFINE_double(min_lane_change_distance, 20.0,
              "The shortest distance for lane change.");
DEFINE_double(proto_double_precision, 1e-6, "Precision of proto double fields");
DEFINE_double(nearby_obstacle_range, 60,
              "Search range for considering nearby obstacles as feature");
DEFINE_int32(nearby_obstacle_num, 1,
             "Maximal number of nearby obstacles to search.");

// Kalman Filter
DEFINE_double(beta, 0.99, "l coefficient for state transferring of (s,l)");
DEFINE_double(cut_in_beta, 0.98, "l coefficient for cut in state transition.");
DEFINE_double(q_var, 0.01, "Q variance for processing");
DEFINE_double(r_var, 0.01, "R variance for measurement");
DEFINE_double(p_var, 0.1, "p variance for prediction");
DEFINE_double(kf_endl, 0.1,
              "Minimum L close enough with lane central reference line");

// Trajectory
DEFINE_bool(enable_rule_layer, true,
            "enable rule for trajectory before model computation");
DEFINE_double(car_length, 4.0, "Length of car");
DEFINE_int32(trajectory_num_frame, 50,
             "Maximum number of frames in trajectory");
DEFINE_double(trajectory_stretch, 0.5,
              "Percent of extra stretch of trajectory");
DEFINE_double(coeff_mul_sigma, 2.0, "coefficient multiply standard deviation");
DEFINE_bool(enable_velocity_from_history, false,
            "whether to ge speed from feature history");
DEFINE_int32(num_trajectory_still_pedestrian, 6,
             "number of trajectories for static pedestrian");

// Offline model
DEFINE_bool(running_middle_mode, false, "Running middle mode");
DEFINE_string(feature_middle_file, "/data/example_features.label.bin",
              "Feature middle file with lane seq labels");
DEFINE_string(hdf5_file, "/data/final_feature_label.h5",
              "Final hdf5 file for neural network training");
DEFINE_bool(enable_labelling, false, "Enable labelling data and output result");
DEFINE_bool(enable_feature_to_file, false,
            "Enable to output the extracted feature to txt files");
DEFINE_bool(enable_evaluation, false, "Enable dumping evaluational features");
DEFINE_int32(num_evaluation_frame, 30,
             "Number of frames for evaluation network");
DEFINE_string(feature_file_path, "/data/features.bin",
              "Path for feature files");
DEFINE_string(feature_file_format, "binary",
              "format of the output feature file [binary / json]");
DEFINE_double(close_time, 0.1, "close in terms of time");
DEFINE_double(close_dist, 0.1, "close in terms of dist");

// Online model
DEFINE_double(target_lane_gap, 5.0, "target lane gap length");
DEFINE_double(model_prob_cutoff, 0.5, "Model probability cutoff");
DEFINE_double(prediction_feature_timeframe, 3.0,
              "Timeframe for generating features in modeling");
DEFINE_double(threshold_label_time_delta, 2.0,
              "Time threshold to select the labelled feature");
DEFINE_double(prediction_label_timeframe, 3.0,
              "Timeframe for generating label in modeling");
DEFINE_double(prediction_buffer_timeframe, 1.0,
              "Buffer Timeframe in case of message gets late");
DEFINE_int32(pedestrian_static_length, 30,
             "min # of frames to check that a pedestrian is still");
DEFINE_double(still_speed, 0.01, "speed considered to be still for pedestrian");
DEFINE_string(default_vehicle_model_file_path,
              "/prediction/parameters/default_model.bin",
              "default model file path");
DEFINE_string(user_vehicle_model_name, "MlpPlugin", "user model for vehicle");
DEFINE_string(user_vehicle_model_file_path,
              "/prediction/parameters/mlp_model.bin", "user model file path");
DEFINE_double(kf_max_speed, 20.0, "Maximum speed for tracking");
DEFINE_double(kf_min_speed, 0.5, "Minumum speed for tracking");
DEFINE_double(kf_still_position_diff, 0.5,
              "Default position difference for still obstacle");
DEFINE_double(max_angle_diff, 1.57, "Maximum angle diff is Pi/3");
DEFINE_double(pedestrian_max_speed, 10.0, "speed upper bound for pedestrian");
DEFINE_double(pedestrian_min_speed, 0.1, "min speed for still pedestrian");
DEFINE_int32(obstacle_static_length, 20,
             "stored frames to judge if a obstacle is static");
DEFINE_double(static_speed_threshold, 1.0,
              "speed threshold to judge if a obstacle is static");

// Setting
DEFINE_bool(enable_p_speed_override, false,
            "Override p_speed with tracked prediction speed");
DEFINE_double(threshold_timestamp_diff, 0.50,
              "Maximum timestamp gap between perception and prediction.");
DEFINE_double(pc_pob_tolerance, 0.15, "Time diff tolerance for pcd and pobs");

// Traffic law
DEFINE_bool(enable_traffic_light_law, true, "enable traffic light law.");
DEFINE_bool(enable_crosswalk_law, true, "enable crosswalk law.");
DEFINE_bool(enable_clear_zone_law, true, "enable clear zone law.");
DEFINE_double(polygon_length_box_length_max_diff, 2.0,
              "max length diff of polygon and box for checking sl point is "
              "right in uturn");

// Traffic light law
DEFINE_double(length_of_passing_stop_line_buffer, 4,
              "passing stop line buffer length");
DEFINE_double(master_min_speed, 0.1, "min speed when compute deacceleration");
DEFINE_double(
    max_deacceleration_for_red_light_stop, 6.0,
    "treat red light as red when deceleration (abstract value in m/s^2)"
    " is less than this threshold, otherwise treated as green light");
DEFINE_double(
    max_deacceleration_for_yellow_light_stop, 2.0,
    "treat yellow light as red when deceleration (abstract value in m/s^2)"
    " is less than this threshold; otherwise treated as green light");

DEFINE_string(planning_config_file,
              "modules/planning/conf/planning_config.pb.txt",
              "planning config file");

DEFINE_string(qp_spline_path_config_file,
              "modules/planning/conf/qp_spline_path_config.pb.txt",
              "Qp spline path config file");
DEFINE_string(dp_poly_path_config_file,
              "modules/planning/conf/dp_poly_path_config.pb.txt",
              "Dp poly path config file");
