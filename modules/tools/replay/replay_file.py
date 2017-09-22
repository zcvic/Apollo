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
"""
This program can replay a planning output pb file via ros
"""
import os.path
import sys
import argparse
import rospy
from std_msgs.msg import String
from google.protobuf import text_format

from modules.localization.proto import localization_pb2
from modules.perception.proto import perception_obstacle_pb2
from modules.perception.proto import traffic_light_detection_pb2
from modules.planning.proto import planning_internal_pb2
from modules.planning.proto import planning_pb2
from modules.prediction.proto import prediction_obstacle_pb2
from modules.routing.proto import routing_pb2


def generate_message(topic, filename):
    """generate message from file"""
    message = None
    if topic == "/apollo/planning":
        message = planning_pb2.ADCTrajectory()
    elif topic == "/apollo/localization/pose":
        message = localization_pb2.LocalizationEstimate()
    elif topic == "/apollo/perception/obstacles":
        message = perception_obstacle_pb2.PerceptionObstacles()
    elif topic == "/apollo/prediction":
        message = prediction_obstacle_pb2.PredictionObstacles()
    elif topic == "/apollo/routing_response":
        message = routing_pb2.RoutingResponse()
    if not message:
        print "Unknown topic:", topic
        sys.exit(0)
    if not os.path.exists(filename):
        return None
    f_handle = file(filename, 'r')
    text_format.Merge(f_handle.read(), message)
    f_handle.close()
    return message


def topic_publisher(topic, filename, period):
    """publisher"""
    rospy.init_node('replay_node', anonymous=True)
    pub = rospy.Publisher(topic, String, queue_size=1)
    rate = rospy.Rate(int(1.0 / period))
    message = generate_message(topic, filename)
    while not rospy.is_shutdown():
        pub.publish(str(message))
        rate.sleep()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="replay a planning result pb file")
    parser.add_argument(
        "filename", action="store", type=str, help="planning result files")
    parser.add_argument(
        "topic", action="store", type=str, help="set the planning topic")
    parser.add_argument(
        "--period",
        action="store",
        type=float,
        default=1,
        help="set the topic publish time duration")
    args = parser.parse_args()
    try:
        topic_publisher(args.topic, args.filename, args.period)

    except rospy.ROSInterruptException:
        print "failed to replay message"
