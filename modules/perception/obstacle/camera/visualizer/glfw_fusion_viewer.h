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

#ifndef MODULES_PERCEPTION_OBSTACLE_CAMERA_VISUALIZER_GLFW_FUSION_VIEWER_H_
#define MODULES_PERCEPTION_OBSTACLE_CAMERA_VISUALIZER_GLFW_FUSION_VIEWER_H_

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include "modules/perception/common/perception_gflags.h"
#include "modules/perception/obstacle/base/object.h"
#include "modules/perception/obstacle/camera/common/camera.h"
#include "modules/perception/obstacle/camera/visualizer/base_visualizer.h"
#include "modules/perception/obstacle/camera/visualizer/common/camera.h"
#include "modules/perception/obstacle/camera/visualizer/common/gl_raster_text.h"
#include "modules/perception/obstacle/camera/visualizer/frame_content.h"

namespace apollo {
namespace perception {
namespace lowcostvisualizer {

#define BUFFER_OFFSET(offset) (static_cast<GLvoid *>(offset))

typedef struct {
  GLfloat x;
  GLfloat y;
  GLfloat z;
} vec3;

using ::apollo::perception::FrameContent;

template <typename T = float>
T get_poly_value(T a, T b, T c, T d, T x) {
  T y = d;
  T v = x;
  y += (c * v);
  v *= x;
  y += (b * v);
  v *= x;
  y += (a * v);
  return y;
}

class GLFWFusionViewer {
 public:
  GLFWFusionViewer();

  virtual ~GLFWFusionViewer();

  bool initialize();

  void set_frame_content(FrameContent *frame_content) {
    _frame_content = frame_content;
  }

  void spin();

  void spin_once();

  void close();

  void set_background_color(Eigen::Vector3d i_bg_color) {
    _bg_color = i_bg_color;
  }

  void set_camera_para(Eigen::Vector3d i_position, Eigen::Vector3d i_scn_center,
                       Eigen::Vector3d i_up_vector);

  void set_forward_dir(Eigen::Vector3d forward) {
    _forward_dir = forward;
  }

  void set_main_car(const std::vector<Eigen::Vector3d> &main_car) {
    _main_car = main_car;
  }

  // callback assistants
  void resize_framebuffer(int width, int height);

  void mouse_move(double xpos, double ypos);

  void mouse_wheel(double delta);

  void reset();

  void keyboard(int key);

  void resize_window(int width, int height);

  // callback functions
  static void framebuffer_size_callback(GLFWwindow *window, int width,
                                        int height);

  static void window_size_callback(GLFWwindow *window, int width, int height);

  // input related
  static void key_callback(GLFWwindow *window, int key, int scancode,
                           int action, int mods);

  static void mouse_button_callback(GLFWwindow *window, int button, int action,
                                    int mods);

  static void mouse_cursor_position_callback(GLFWwindow *window, double xpos,
                                             double ypos);

  static void mouse_scroll_callback(GLFWwindow *window, double xoffset,
                                    double yoffset);

  // error handling
  static void error_callback(int error, const char *description);

 private:
  bool window_init();

  bool camera_init();

  bool opengl_init();

  void pre_draw();

  void render();

 protected:
  vec3 get_velocity_src_position(const ObjectPtr &object);

  // capture screen
  void capture_screen(const std::string &file_name);

  // for drawing camera 2d results
 protected:
  // @brief Get camera intrinsics with distortion coefficients from file
  bool get_camera_distort_intrinsics(
      const std::string &file_name,
      ::apollo::perception::CameraDistort<double> *camera_distort);

  // @brief Project 3D point to 2D image using pin-hole camera model with
  // distortion
  bool project_point_undistort(Eigen::Matrix4d w2c, Eigen::Vector3d pc,
                               Eigen::Vector2d *p2d);

  void get_8points(float width, float height, float length,
                   std::vector<Eigen::Vector3d> *point);

  bool get_boundingbox(Eigen::Vector3d center, Eigen::Matrix4d w2c, float width,
                       float height, float length, Eigen::Vector3d dir,
                       float theta, std::vector<Eigen::Vector2d> *points);

  bool get_project_point(Eigen::Matrix4d w2c, Eigen::Vector3d pc,
                         Eigen::Vector2d *p2d);

  void draw_line2d(const Eigen::Vector2d &p1, const Eigen::Vector2d &p2,
                   int line_width, int r, int g, int b, int offset_x,
                   int offset_y, int raw_image_width, int raw_image_height);

  void draw_camera_box2d(const std::vector<ObjectPtr> &objects,
                         Eigen::Matrix4d w2c, int offset_x, int offset_y,
                         int image_width, int image_height);

  void draw_camera_box3d(const std::vector<ObjectPtr> &camera_objects,
                         const std::vector<ObjectPtr> &segmented_objects,
                         Eigen::Matrix4d w2c, int offset_x, int offset_y,
                         int image_width, int image_height);

  void draw_rect2d(const Eigen::Vector2d &p1, const Eigen::Vector2d &p2,
                   int line_width, int r, int g, int b, int offset_x,
                   int offset_y, int image_width, int image_height);

  void draw_8pts_box(const std::vector<Eigen::Vector2d> &points,
                     const Eigen::Vector3f &color, int offset_x, int offset_y,
                     int image_width, int image_height);

  bool draw_car_forward_dir();
  void draw_objects(const std::vector<ObjectPtr> &objects,
                    const Eigen::Matrix4d &w2c, bool draw_cube,
                    bool draw_velocity, const Eigen::Vector3f &color,
                    bool use_class_color);
  bool draw_objects(FrameContent *content, bool draw_cube, bool draw_velocity);
  void draw_3d_classifications(FrameContent *content, bool show_fusion);
  void draw_camera_box(const std::vector<ObjectPtr> &objects,
                       Eigen::Matrix4d w2c, int offset_x, int offset_y,
                       int image_width, int image_height);

  void draw_objects2d(const std::vector<ObjectPtr> &objects,
                      Eigen::Matrix4d w2c, std::string name, int offset_x,
                      int offset_y, int image_width, int image_height);

 private:
  bool _init;

  GLFWwindow *_window;
  Camera *_pers_camera;
  Eigen::Vector3d _forward_dir;
  std::vector<Eigen::Vector3d> _main_car;

  Eigen::Vector3d _bg_color;
  int _win_width;
  int _win_height;
  int _mouse_prev_x;
  int _mouse_prev_y;
  Eigen::Matrix4d _mode_mat;
  Eigen::Matrix4d _view_mat;

  FrameContent *_frame_content;
  unsigned char *_rgba_buffer;

  double _vao_trans_x;
  double _vao_trans_y;
  double _vao_trans_z;
  double _Rotate_x;
  double _Rotate_y;
  double _Rotate_z;
  bool show_box;
  bool show_velocity;
  bool show_polygon;
  bool show_text;


  void get_class_color(int cls, float rgb[3]);

  enum { circle, cube, cloud, polygon, NumVAOs_typs };  // {0, 1, 2, 3, 4}
  enum { vertices, colors, elements, NumVBOs };         // {0, 1, 2, 3}

  // cloud
  static const int VAO_cloud_num = 35;
  static const int VBO_cloud_num = 10000;
  GLuint VAO_cloud[VAO_cloud_num];
  GLuint buffers_cloud[VAO_cloud_num][NumVBOs];
  GLfloat cloudVerts[VBO_cloud_num][3];

  bool draw_cloud(FrameContent *content);

  // circle
  static const int VAO_circle_num = 4;
  static const int VBO_circle_num = 360;
  GLuint VAO_circle[VAO_circle_num];
  vec3 get_velocity_src_position(FrameContent *content, int id);

  // fusion association
  void draw_fusion_association(FrameContent *content);

  GLuint image_to_gl_texture(const cv::Mat &mat, GLenum min_filter,
                             GLenum mag_filter, GLenum wrap_filter);

  void draw_camera_frame(FrameContent *content);

  // @brief, draw 2d camera frame, show 2d or 3d classification
  void draw_camera_frame(FrameContent *content, bool show_3d_class);

  bool _use_class_color = true;

  bool _capture_screen = false;
  bool _capture_video = false;

  int _scene_width;
  int _scene_height;
  int _image_width;
  int _image_height;

  Eigen::Matrix<double, 3, 4> _camera_intrinsic;  // camera intrinsic

  bool _show_fusion_pc;
  bool _show_radar_pc;
  bool _show_camera_box2d;     // show 2d bbox in camera frame
  bool _show_camera_box3d;     // show 3d bbox in camera frame
  bool _show_associate_color;  // show same color for both 3d pc bbox and camera
                               // bbox
  bool _show_type_id_label;
  bool _show_lane;
  bool _draw_lane_objects;

  static std::vector<std::vector<int>> s_color_table;
  std::shared_ptr<GLRasterText> _raster_text;

  // pin-hole camera model with distortion
  std::shared_ptr<::apollo::perception::CameraDistort<double>>
      _distort_camera_intrinsic;

  // frame count
  int _frame_count;
};

}  // namespace lowcostvisualizer
}  // namespace perception
}  // namespace apollo

#endif
