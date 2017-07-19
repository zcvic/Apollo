/* Copyright 2017 The Apollo Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
=========================================================================*/

#ifndef MODULES_MAP_MAP_LOADER_INC_HDMAP_COMMON_H
#define MODULES_MAP_MAP_LOADER_INC_HDMAP_COMMON_H

#include <vector>
#include <memory>
#include <utility>

#include "modules/common/math/aabox2d.h"
#include "modules/common/math/vec2d.h"
#include "modules/common/math/polygon2d.h"
#include "modules/common/math/aaboxkdtree2d.h"
#include "modules/common/math/math_utils.h"
#include "modules/map/proto/map_lane.pb.h"
#include "modules/map/proto/map_junction.pb.h"
#include "modules/map/proto/map_signal.pb.h"
#include "modules/map/proto/map_crosswalk.pb.h"
#include "modules/map/proto/map_stop_sign.pb.h"
#include "modules/map/proto/map_yield_sign.pb.h"
#include "modules/map/proto/map_overlap.pb.h"

namespace apollo {
namespace hdmap {

template <class Object, class GeoObject> class ObjectWithAABox {
 public:
    ObjectWithAABox(const apollo::common::math::AABox2d &aabox,
                    const Object *object, const GeoObject *geo_object,
                    const int id)
        : _aabox(aabox), _object(object), _geo_object(geo_object), _id(id) {}
    ~ObjectWithAABox() {}
    const apollo::common::math::AABox2d &aabox() const { return _aabox; }
    double DistanceTo(const apollo::common::math::Vec2d &point) const {
        return _geo_object->DistanceTo(point);
    }
    double DistanceSquareTo(const apollo::common::math::Vec2d &point) const {
        return _geo_object->DistanceSquareTo(point);
    }
    const Object *object() const { return _object; }
    const GeoObject *geo_object() const { return _geo_object; }
    int id() const { return _id; }

 private:
    apollo::common::math::AABox2d _aabox;
    const Object *_object;
    const GeoObject *_geo_object;
    int _id;
};

class LaneInfo {
 public:
    explicit LaneInfo(const apollo::hdmap::Lane &lane);

    const apollo::hdmap::Id &id() const { return _lane.id(); }
    const apollo::hdmap::Lane &lane() const { return _lane; }
    const std::vector<apollo::common::math::Vec2d> &points() const {
        return _points;
    }
    const std::vector<apollo::common::math::Vec2d> &unit_directions() const {
        return _unit_directions;
    }
    const std::vector<double> &headings() const { return _headings; }
    const std::vector<apollo::common::math::LineSegment2d> &segments() const {
        return _segments;
    }
    const std::vector<double> &accumulate_s() const { return _accumulated_s; }

    double total_length() const { return _total_length; }

    using SampledWidth = std::pair<double, double>;
    const std::vector<SampledWidth> &sampled_left_width() const {
        return _sampled_left_width;
    }
    const std::vector<SampledWidth> &sampled_right_width() const {
        return _sampled_right_width;
    }
    void get_width(const double s, double *left_width,
                                            double *right_width) const;
    double get_width(const double s) const;
    double get_effective_width(const double s) const;

 private:
    void init();
    double get_width_from_sample(
                            const std::vector<LaneInfo::SampledWidth>& samples,
                            const double s) const;

 private:
    const apollo::hdmap::Lane &_lane;
    std::vector<apollo::common::math::Vec2d> _points;
    std::vector<apollo::common::math::Vec2d> _unit_directions;
    std::vector<double> _headings;
    std::vector<apollo::common::math::LineSegment2d> _segments;
    std::vector<double> _accumulated_s;
    double _total_length = 0.0;
    std::vector<SampledWidth> _sampled_left_width;
    std::vector<SampledWidth> _sampled_right_width;
};
using LaneSegmentBox =
    ObjectWithAABox<LaneInfo, apollo::common::math::LineSegment2d>;
using LaneSegmentKDTree = apollo::common::math::AABoxKDTree2d<LaneSegmentBox>;

class JunctionInfo {
 public:
    explicit JunctionInfo(const apollo::hdmap::Junction &junction);

    const apollo::hdmap::Id &id() const { return _junction.id(); }
    const apollo::hdmap::Junction &junction() const { return _junction; }
    const apollo::common::math::Polygon2d &polygon() const { return _polygon; }
    const apollo::common::math::AABox2d& mbr() const { return _mbr; }

 private:
    void init();

 private:
    const apollo::hdmap::Junction &_junction;
    apollo::common::math::Polygon2d _polygon;
    apollo::common::math::AABox2d _mbr;
};
using JunctionPolygonBox =
    ObjectWithAABox<JunctionInfo, apollo::common::math::Polygon2d>;
using JunctionPolygonKDTree =
    apollo::common::math::AABoxKDTree2d<JunctionPolygonBox>;

class SignalInfo {
 public:
  explicit SignalInfo(const apollo::hdmap::Signal &signal);

  const apollo::hdmap::Id &id() const { return _signal.id(); }
  const apollo::hdmap::Signal &signal() const { return _signal; }
  const std::vector<apollo::common::math::LineSegment2d> &segments() const {
    return _segments;
  }

 private:
  void init();

 private:
  const apollo::hdmap::Signal &_signal;
  std::vector<apollo::common::math::LineSegment2d> _segments;
};
using SignalSegmentBox =
    ObjectWithAABox<SignalInfo, apollo::common::math::LineSegment2d>;
using SignalSegmentKDTree =
    apollo::common::math::AABoxKDTree2d<SignalSegmentBox>;

class CrosswalkInfo {
 public:
  explicit CrosswalkInfo(const apollo::hdmap::Crosswalk &crosswalk);

  const apollo::hdmap::Id &id() const { return _crosswalk.id(); }
  const apollo::hdmap::Crosswalk &crosswalk() const { return _crosswalk; }
  const apollo::common::math::Polygon2d &polygon() const { return _polygon; }

 private:
  void init();

 private:
  const apollo::hdmap::Crosswalk &_crosswalk;
  apollo::common::math::Polygon2d _polygon;
};
using CrosswalkPolygonBox =
    ObjectWithAABox<CrosswalkInfo, apollo::common::math::Polygon2d>;
using CrosswalkPolygonKDTree =
    apollo::common::math::AABoxKDTree2d<CrosswalkPolygonBox>;

class StopSignInfo {
 public:
  explicit StopSignInfo(const apollo::hdmap::StopSign &stop_sign);

  const apollo::hdmap::Id &id() const { return _stop_sign.id(); }
  const apollo::hdmap::StopSign &stop_sign() const { return _stop_sign; }
  const std::vector<apollo::common::math::LineSegment2d> &segments() const {
    return _segments;
  }

 private:
  void init();

 private:
  const apollo::hdmap::StopSign &_stop_sign;
  std::vector<apollo::common::math::LineSegment2d> _segments;
};
using StopSignSegmentBox =
    ObjectWithAABox<StopSignInfo, apollo::common::math::LineSegment2d>;
using StopSignSegmentKDTree =
    apollo::common::math::AABoxKDTree2d<StopSignSegmentBox>;

class YieldSignInfo {
 public:
  explicit YieldSignInfo(const apollo::hdmap::YieldSign &yield_sign);

  const apollo::hdmap::Id &id() const { return _yield_sign.id(); }
  const apollo::hdmap::YieldSign &yield_sign() const {
    return _yield_sign;
  }
  const std::vector<apollo::common::math::LineSegment2d> &segments() const {
    return _segments;
  }

 private:
  void init();

 private:
  const apollo::hdmap::YieldSign &_yield_sign;
  std::vector<apollo::common::math::LineSegment2d> _segments;
};
using YieldSignSegmentBox =
    ObjectWithAABox<YieldSignInfo, apollo::common::math::LineSegment2d>;
using YieldSignSegmentKDTree =
    apollo::common::math::AABoxKDTree2d<YieldSignSegmentBox>;

class OverlapInfo {
 public:
  explicit OverlapInfo(const apollo::hdmap::Overlap &overlap);

  const apollo::hdmap::Id &id() const { return _overlap.id(); }
  const apollo::hdmap::Overlap &overlap() const { return _overlap; }
  const apollo::hdmap::ObjectOverlapInfo *
  get_object_overlap_info(const apollo::hdmap::Id &id) const;

 private:
  const apollo::hdmap::Overlap &_overlap;
};

typedef std::shared_ptr<const apollo::hdmap::LaneInfo>    LaneInfoConstPtr;
typedef std::shared_ptr<const apollo::hdmap::JunctionInfo> JunctionInfoConstPtr;
typedef std::shared_ptr<const apollo::hdmap::SignalInfo>    SignalInfoConstPtr;
typedef std::shared_ptr<const apollo::hdmap::CrosswalkInfo>
                                                        CrosswalkInfoConstPtr;
typedef std::shared_ptr<const apollo::hdmap::StopSignInfo> StopSignInfoConstPtr;
typedef std::shared_ptr<const apollo::hdmap::YieldSignInfo>
                                                        YieldSignInfoConstPtr;
typedef std::shared_ptr<const apollo::hdmap::OverlapInfo>  OverlapInfoConstPtr;

}  // namespace hdmap
}  // namespace apollo

#endif  // MODULES_MAP_MAP_LOADER_INC_HDMAP_COMMON_H
