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
import math
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy.polynomial.polynomial as poly
from modules.drivers.proto import mobileye_pb2
from modules.planning.proto import planning_pb2
from modules.localization.proto import localization_pb2
from modules.canbus.proto import chassis_pb2

planning_pub = None
f = open("benchmark.txt","w")
SPEED = 0 #m/s
CRUISE_SPEED = 10 #m/s
MINIMUM_PATH_LENGTH = 5 #meter
x= []
y = []
nx = []
ny = []

def get_central_line(mobileye_pb, path_length):
    ref_lane_x = []
    ref_lane_y = []

    rc0 = mobileye_pb.lka_768.position
    rc1 = mobileye_pb.lka_769.heading_angle
    rc2 = mobileye_pb.lka_768.curvature
    rc3 = mobileye_pb.lka_768.curvature_derivative
    #rrangex = mobileye_pb.lka_769.view_range

    lc0 = mobileye_pb.lka_766.position
    lc1 = mobileye_pb.lka_767.heading_angle
    lc2 = mobileye_pb.lka_766.curvature
    lc3 = mobileye_pb.lka_766.curvature_derivative
    #lrangex = mobileye_pb.lka_767.view_range
    for y in range(int(path_length)):
        rx = rc3 * (y * y * y) + rc2 * (y * y) + rc1 * y + rc0
        lx = lc3 * (y * y * y) + lc2 * (y * y) + lc1 * y + lc0
        ref_lane_x.append((rx + lx)/2.0)
        ref_lane_y.append(y)
    return ref_lane_x, ref_lane_y

def get_path(x, y, path_length):
    ind = int(math.floor((x[0] * 3.0) / 1) + 1)
    newx = [0]
    newy = [0]
    for i in range(len(y)-ind):
        newx.append(x[i+ind])
        newy.append(y[i+ind])
    coefs = poly.polyfit(newy, newx, 4) #x = f(y)
    nx = poly.polyval(y, coefs)
    return nx, y

def get_speed():
    return CRUISE_SPEED

def localization_callback(localization_pb):
    speed_x = localization_pb.pose.linear_velocity.x
    speed_y = localization_pb.pose.linear_velocity.y
    acc_x = localization_pb.linear_acceleration.x
    acc_y = localization_pb.linear_acceleration.y

def chassis_callback(chassis_pb):
    global SPEED
    SPEED = chassis_pb.speed_mps

def mobileye_callback(mobileye_pb):
    global x, y, nx ,ny
    start_timestamp = time.time()
    path_length = MINIMUM_PATH_LENGTH
    if path_length < SPEED * 2:
        path_length = math.ceil(SPEED * 2)
    x, y = get_central_line(mobileye_pb, path_length)
    nx, ny = get_path(x, y, path_length)

    adc_trajectory = planning_pb2.ADCTrajectory()
    adc_trajectory.header.timestamp_sec = rospy.get_rostime().secs
    adc_trajectory.header.module_name = "planning"
    adc_trajectory.gear = chassis_pb2.Chassis.GEAR_DRIVE
    adc_trajectory.latency_stats.total_time_ms = (time.time() - start_timestamp) * 1000
    for i in range(len(nx)):
        traj_point = adc_trajectory.trajectory_point.add()
        traj_point.path_point.x = nx[i]
        traj_point.path_point.y = ny[i]
        traj_point.path_point.theta = 0 ## TODO
        traj_point.path_point.s = 0 ##TODO
        traj_point.v = 0 #TODO
        traj_point.relative_time = 0#TODO

    planning_pub.publish(adc_trajectory)
    f.write("duration: " + str(time.time() - start_timestamp) + "\n")

def add_listener():
    global planning_pub
    rospy.init_node('planning_lite', anonymous=True)
    rospy.Subscriber('/apollo/sensor/mobileye',
                     mobileye_pb2.Mobileye,
                     mobileye_callback)
    #rospy.Subscriber('/apollo/localization/pose',
    #                 localization_pb2.LocalizationEstimate,
    #                 localization_callback)
    rospy.Subscriber('/apollo/canbus/chassis',
                     chassis_pb2.Chassis,
                     chassis_callback)

    planning_pub = rospy.Publisher(
        '/apollo/planning_lite', planning_pb2.ADCTrajectory, queue_size=1)


def update(frame_number):
    line1.set_xdata(x)
    line1.set_ydata(y)
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
        line2, = ax.plot([0], [0])
        ani = animation.FuncAnimation(fig, update, interval=100)
        plt.show()
    else:
        rospy.spin()
