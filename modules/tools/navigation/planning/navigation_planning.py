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

import rospy
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from modules.drivers.proto import mobileye_pb2
from modules.planning.proto import planning_pb2
from modules.canbus.proto import chassis_pb2
from path_decider import PathDecider
from speed_decider import SpeedDecider
from trajectory_generator import TrajectoryGenerator
from provider_mobileye import MobileyeProvider
from provider_chassis import ChassisProvider
from provider_localization import LocalizationProvider

planning_pub = None
PUB_NODE_NAME = "planning"
PUB_TOPIC = "/apollo/" + PUB_NODE_NAME
f = open("benchmark.txt", "w")
CRUISE_SPEED = 10  # m/s

path_decider = PathDecider()
speed_decider = SpeedDecider()
traj_generator = TrajectoryGenerator()
mobileye_provider = MobileyeProvider()
chassis_provider = ChassisProvider()
localization_provider = LocalizationProvider()

nx = []
ny = []


def localization_callback(localization_pb):
    localization_provider.update(localization_pb)


def chassis_callback(chassis_pb):
    chassis_provider.update(chassis_pb)


def mobileye_callback(mobileye_pb):
    global nx, ny
    start_timestamp = time.time()

    mobileye_provider.update(mobileye_pb)
    mobileye_provider.process_obstacles()

    path_coef, path_length = path_decider.get_path(
        mobileye_provider, chassis_provider)

    speed, final_path_length = speed_decider.get_target_speed_and_path_length(
        mobileye_provider, chassis_provider, path_length)

    adc_trajectory = traj_generator.generate(path_coef, final_path_length,
                                             speed,
                                             start_timestamp=start_timestamp)
    planning_pub.publish(adc_trajectory)
    f.write("duration: " + str(time.time() - start_timestamp) + "\n")


def add_listener():
    global planning_pub
    rospy.init_node(PUB_NODE_NAME, anonymous=True)
    rospy.Subscriber('/apollo/sensor/mobileye',
                     mobileye_pb2.Mobileye,
                     mobileye_callback)
    # rospy.Subscriber('/apollo/localization/pose',
    #                 localization_pb2.LocalizationEstimate,
    #                 localization_callback)
    rospy.Subscriber('/apollo/canbus/chassis',
                     chassis_pb2.Chassis,
                     chassis_callback)

    planning_pub = rospy.Publisher(
        PUB_TOPIC, planning_pb2.ADCTrajectory, queue_size=1)


def update(frame_number):
    line2.set_xdata(nx)
    line2.set_ydata(ny)


if __name__ == '__main__':

    DEBUG = False
    line1 = None
    line2 = None
    add_listener()

    if DEBUG:
        fig = plt.figure()
        ax = plt.subplot2grid((1, 1), (0, 0), rowspan=1, colspan=1)
        line1, = ax.plot([-10, 10, -10, 10], [-10, 150, 150, -10])
        line1.set_visible(False)
        line2, = ax.plot([0], [0])
        ani = animation.FuncAnimation(fig, update, interval=100)
        ax.axvline(x=0.0, alpha=0.3)
        ax.axhline(y=0.0, alpha=0.3)

        plt.show()
    else:
        rospy.spin()
