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

#include "modules/dreamview/backend/map/map_service.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::apollo::hdmap::Id;
using ::apollo::hdmap::Map;
using ::apollo::hdmap::Point;
using ::testing::UnorderedElementsAre;

namespace apollo {
namespace dreamview {

TEST(MapElementIdsTest, Hash) {
  MapElementIds ids;
  ids.lane = {"first_lane", "second_lane", "haha_lane"};
  ids.overlap = {"last_overlap"};
  EXPECT_EQ(
      std::hash<std::string>()("first_lanesecond_lanehaha_lanelast_overlap"),
      ids.Hash());
}

TEST(MapElementIdsTest, Json) {
  MapElementIds ids;
  ids.lane = {"first_lane", "second_lane", "haha_lane"};
  auto json = ids.Json();

  EXPECT_EQ(
      "{\"crosswalk\":[],\"junction\":[],\"lane\":[\"first_lane\","
      "\"second_lane\",\"haha_lane\"],\"overlap\":[],\"signal\":[],"
      "\"stopSign\":[],\"yield\":[]}",
      json.dump());

  MapElementIds from_json(json);
  EXPECT_THAT(from_json.lane,
              UnorderedElementsAre("first_lane", "second_lane", "haha_lane"));
  EXPECT_THAT(from_json.crosswalk, UnorderedElementsAre());
  EXPECT_THAT(from_json.junction, UnorderedElementsAre());
  EXPECT_THAT(from_json.signal, UnorderedElementsAre());
  EXPECT_THAT(from_json.stop_sign, UnorderedElementsAre());
  EXPECT_THAT(from_json.yield, UnorderedElementsAre());
  EXPECT_THAT(from_json.overlap, UnorderedElementsAre());
}

class MapServiceTest : public ::testing::Test {
 protected:
  MapServiceTest()
      : map_service("modules/dreamview/backend/testdata/garage.bin") {}

  MapService map_service;
};

TEST_F(MapServiceTest, LoadMap) {
  Id id;
  id.set_id("l1");
  EXPECT_EQ("l1", map_service.hdmap().get_lane_by_id(id)->id().id());
}

TEST_F(MapServiceTest, CollectMapElements) {
  Point p;
  MapElementIds map_element_ids = map_service.CollectMapElements(p, 20000.0);

  EXPECT_THAT(map_element_ids.lane, UnorderedElementsAre("l1"));
  EXPECT_THAT(map_element_ids.crosswalk, UnorderedElementsAre());
  EXPECT_THAT(map_element_ids.junction, UnorderedElementsAre());
  EXPECT_THAT(map_element_ids.signal, UnorderedElementsAre());
  EXPECT_THAT(map_element_ids.stop_sign, UnorderedElementsAre());
  EXPECT_THAT(map_element_ids.yield, UnorderedElementsAre());
  EXPECT_THAT(map_element_ids.overlap, UnorderedElementsAre());
}

TEST_F(MapServiceTest, RetrieveMapElements) {
  MapElementIds map_element_ids;
  map_element_ids.lane.push_back("l1");
  Map map = map_service.RetrieveMapElements(map_element_ids);
  EXPECT_EQ(1, map.lane_size());
  EXPECT_EQ("l1", map.lane(0).id().id());
}

}  // namespace dreamview
}  // namespace apollo
