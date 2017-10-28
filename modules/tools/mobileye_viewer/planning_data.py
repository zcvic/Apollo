#!/usr/bin/env python

###############################################################################
# Copyright 2017 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###############################################################################

import threading

class PlanningData:
    def __init__(self, planning_pb=None):
        self.planning_pb = planning_pb
        self.path_x = []
        self.path_y = []

    def update(self, planning_pb):
        self.planning_pb = planning_pb

    def compute_path(self):
        if self.planning_pb is None:
            return
        path_x = []
        path_y = []
        for point in self.planning_pb.trajectory_point:
            path_x.append(point.path_point.x)
            path_y.append(point.path_point.y)
        self.path_x = path_x
        self.path_y = path_y
