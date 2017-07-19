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

#include <algorithm>
#include <vector>
#include <string>

#include "gtest/gtest.h"
#include "modules/map/hdmap/hdmap_impl.h"

namespace {

constexpr char kMapFilename[] = "modules/map/hdmap/test-data/map_data.dat.v2";

}  // namespace

namespace apollo {
namespace hdmap {

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class HDMapImplTestSuite : public ::testing::Test {
 public:
    HDMapImplTestSuite() {
    }
    virtual ~HDMapImplTestSuite() {
    }
    virtual void SetUp() {
    }
    virtual void TearDown() {
    }
    void initial_context();
 public:
    HDMapImpl _hdmap_impl;
};

void HDMapImplTestSuite::initial_context() {
    int ret = _hdmap_impl.load_map_from_file(kMapFilename);
    ASSERT_EQ(0, ret);
}

TEST_F(HDMapImplTestSuite, get_lane_by_id) {
    initial_context();
    Id lane_id;
    lane_id.set_id("1");
    ASSERT_TRUE(nullptr == _hdmap_impl.get_lane_by_id(lane_id));
    lane_id.set_id("1476433313307_1_-1");
    LaneInfoConstPtr lane_ptr = _hdmap_impl.get_lane_by_id(lane_id);
    ASSERT_TRUE(nullptr != lane_ptr);
    ASSERT_STREQ(lane_id.id().c_str(), lane_ptr->id().id().c_str());
}

TEST_F(HDMapImplTestSuite, get_junction_by_id) {
    initial_context();
    Id junction_id;
    junction_id.set_id("1");
    ASSERT_TRUE(nullptr == _hdmap_impl.get_junction_by_id(junction_id));
    junction_id.set_id("1473834008594");
    JunctionInfoConstPtr junction_ptr = _hdmap_impl.get_junction_by_id(
                                                            junction_id);
    ASSERT_TRUE(nullptr != junction_ptr);
    ASSERT_STREQ(junction_id.id().c_str(), junction_ptr->id().id().c_str());
}

TEST_F(HDMapImplTestSuite, get_signal_by_id) {
    initial_context();
    Id signal_id;
    signal_id.set_id("abc");
    ASSERT_TRUE(nullptr == _hdmap_impl.get_signal_by_id(signal_id));
    signal_id.set_id("0");
    SignalInfoConstPtr signal_ptr = _hdmap_impl.get_signal_by_id(signal_id);
    ASSERT_TRUE(nullptr != signal_ptr);
    ASSERT_STREQ(signal_id.id().c_str(), signal_ptr->id().id().c_str());
}

TEST_F(HDMapImplTestSuite, get_crosswalk_by_id) {
    initial_context();
    Id crosswalk_id;
    crosswalk_id.set_id("1");
    ASSERT_TRUE(nullptr == _hdmap_impl.get_crosswalk_by_id(crosswalk_id));
    crosswalk_id.set_id("1473840237307");
    CrosswalkInfoConstPtr crosswalk_ptr = _hdmap_impl.get_crosswalk_by_id(
                                                                crosswalk_id);
    ASSERT_TRUE(nullptr != crosswalk_ptr);
    ASSERT_STREQ(crosswalk_id.id().c_str(), crosswalk_ptr->id().id().c_str());
}

TEST_F(HDMapImplTestSuite, get_stop_sign_by_id) {
    initial_context();
    Id stop_sign_id;
    stop_sign_id.set_id("1");
    ASSERT_TRUE(nullptr == _hdmap_impl.get_stop_sign_by_id(stop_sign_id));
    stop_sign_id.set_id("stop_sign_1");
    StopSignInfoConstPtr stop_sign_ptr = _hdmap_impl.get_stop_sign_by_id(
                                                                stop_sign_id);
    ASSERT_TRUE(nullptr != stop_sign_ptr);
    ASSERT_STREQ(stop_sign_id.id().c_str(), stop_sign_ptr->id().id().c_str());
}

TEST_F(HDMapImplTestSuite, get_yield_sign_by_id) {
    initial_context();
    Id yield_sign_id;
    yield_sign_id.set_id("1");
    ASSERT_TRUE(nullptr == _hdmap_impl.get_yield_sign_by_id(yield_sign_id));
    yield_sign_id.set_id("yield_sign_1");
    YieldSignInfoConstPtr yield_sign_ptr = _hdmap_impl.get_yield_sign_by_id(
                                                                yield_sign_id);
    ASSERT_TRUE(nullptr != yield_sign_ptr);
    ASSERT_STREQ(yield_sign_id.id().c_str(), yield_sign_ptr->id().id().c_str());
}

TEST_F(HDMapImplTestSuite, get_overlap_by_id) {
    initial_context();
    Id overlap_id;
    overlap_id.set_id("1");
    ASSERT_TRUE(nullptr == _hdmap_impl.get_overlap_by_id(overlap_id));
    overlap_id.set_id("overlap_533");
    OverlapInfoConstPtr overlap_ptr = _hdmap_impl.get_overlap_by_id(overlap_id);
    ASSERT_TRUE(nullptr != overlap_ptr);
    ASSERT_STREQ(overlap_id.id().c_str(), overlap_ptr->id().id().c_str());
}

TEST_F(HDMapImplTestSuite, get_lanes) {
    initial_context();
    std::vector<LaneInfoConstPtr> lanes;
    apollo::hdmap::Point point;
    point.set_x(0.0);
    point.set_y(0.0);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_lanes(point, 1e-6, &lanes));
    EXPECT_EQ(0, lanes.size());
    EXPECT_EQ(0, _hdmap_impl.get_lanes(point, 5, &lanes));
    EXPECT_EQ(3, lanes.size());
    std::vector<std::string> ids;
    for (const auto &lane : lanes) {
        ids.push_back(lane->id().id());
    }
    std::sort(ids.begin(), ids.end());

    EXPECT_EQ("1476433313307_1_-1", ids[0]);
    EXPECT_EQ("1476761826058_1_-1", ids[1]);
    EXPECT_EQ("1476762215457_1_-1", ids[2]);
}

TEST_F(HDMapImplTestSuite, get_junctions) {
    initial_context();
    std::vector<JunctionInfoConstPtr> junctions;
    apollo::hdmap::Point point;
    point.set_x(-36.0);
    point.set_y(-28.0);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_junctions(point, 1, &junctions));
    EXPECT_EQ(0, junctions.size());
    EXPECT_EQ(0, _hdmap_impl.get_junctions(point, 3, &junctions));
    EXPECT_EQ(1, junctions.size());
    EXPECT_EQ("1476433239227", junctions[0]->id().id());
}

TEST_F(HDMapImplTestSuite, get_crosswalks) {
    initial_context();
    std::vector<CrosswalkInfoConstPtr> crosswalks;
    apollo::hdmap::Point point;
    point.set_x(199.0);
    point.set_y(-440.0);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_crosswalks(point, 1, &crosswalks));
    EXPECT_EQ(0, crosswalks.size());
    EXPECT_EQ(0, _hdmap_impl.get_crosswalks(point, 3, &crosswalks));
    EXPECT_EQ(1, crosswalks.size());
    EXPECT_EQ("1473840237307", crosswalks[0]->id().id());
}

TEST_F(HDMapImplTestSuite, get_signals) {
    initial_context();
    std::vector<SignalInfoConstPtr> signals;
    apollo::hdmap::Point point;
    point.set_x(-250.0);
    point.set_y(405.0);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_signals(point, 4.8, &signals));
    EXPECT_EQ(0, signals.size());
    EXPECT_EQ(0, _hdmap_impl.get_signals(point, 6.6, &signals));
    EXPECT_EQ(3, signals.size());
    EXPECT_EQ("11", signals[0]->id().id());
    EXPECT_EQ("22", signals[1]->id().id());
    EXPECT_EQ("0", signals[2]->id().id());
}

TEST_F(HDMapImplTestSuite, get_stop_signs) {
    initial_context();
    std::vector<StopSignInfoConstPtr> stop_signs;
    apollo::hdmap::Point point;
    point.set_x(0.0);
    point.set_y(0.0);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_stop_signs(point, 100.0, &stop_signs));
    EXPECT_EQ(1, stop_signs.size());
    EXPECT_EQ("stop_sign_8", stop_signs[0]->id().id());
}

TEST_F(HDMapImplTestSuite, get_yield_signs) {
    initial_context();
    std::vector<YieldSignInfoConstPtr> yield_signs;
    apollo::hdmap::Point point;
    point.set_x(0.0);
    point.set_y(0.0);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_yield_signs(point, 100.0, &yield_signs));
    EXPECT_EQ(0, yield_signs.size());
}

TEST_F(HDMapImplTestSuite, get_nearest_lane) {
    initial_context();
    LaneInfoConstPtr lane;
    double s = 0.0;
    double l = 0.0;

    apollo::hdmap::Point point;
    point.set_x(2.5);
    point.set_y(-20.0);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_nearest_lane(point, &lane, &s, &l));
    EXPECT_EQ("1476433297955_1_-1", lane->id().id());
    EXPECT_NEAR(s, 4.18, 1e-3);
    EXPECT_NEAR(l, -1.931, 1e-3);

    point.set_x(-40.0);
    point.set_y(1.8);
    point.set_z(0.0);
    EXPECT_EQ(0, _hdmap_impl.get_nearest_lane(point, &lane, &s, &l));
    EXPECT_EQ("14791047360960_2_-1", lane->id().id());
    EXPECT_NEAR(s, 285.365, 1e-3);
    EXPECT_NEAR(l, -4.734, 1e-3);
}

}  // namespace hdmap
}  // namespace apollo
