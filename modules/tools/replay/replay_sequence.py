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


def generate_message(filename, pb_type):
    f_handle = file(filename, 'r')
    message = pb_type()
    text_format.Merge(f_handle.read(), message)
    f_handle.close()
    return message


def seq_publisher(seq_num, period):
    """publisher"""
    rospy.init_node('replay_node', anonymous=True)

    # topic_name => module_name, pb type, pb, publish_handler
    topic_name_map = {
        "/apollo/localization/pose":
        ["localization", localization_pb2.LocalizationEstimate, None, None],
        "/apollo/routing_response":
        ["routing", routing_pb2.RoutingResponse, None, None],
        "/apollo/perception/obstacles": [
            "perception", perception_obstacle_pb2.PerceptionObstacles, None,
            None
        ],
        "/apollo/prediction": [
            "prediction", prediction_obstacle_pb2.PredictionObstacles, None,
            None
        ],
        "/apollo/planning":
        ["planning", planning_pb2.ADCTrajectory, None, None],
    }
    for topic, module_features in topic_name_map.iteritems():
        filename = str(seq_num) + "_" + module_features[0] + ".pb.txt"
        print "trying to load pb file:", filename
        module_features[3] = rospy.Publisher(
            topic, module_features[1], queue_size=1)
        module_features[2] = generate_message(filename, module_features[1])
        if module_features[2] is None:
            print topic, " pb is none"

    rate = rospy.Rate(int(1.0 / period))  # 10hz
    while not rospy.is_shutdown():
        for topic, module_features in topic_name_map.iteritems():
            if not module_features[2] is None:
                module_features[3].publish(module_features[2])
        rate.sleep()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="replay a set of pb files with the same sequence number")
    parser.add_argument(
        "seq",
        action="store",
        type=int,
        default=-1,
        help="set sequence number to replay")
    parser.add_argument(
        "--period",
        action="store",
        type=float,
        default=1,
        help="set the topic publish time duration")
    args = parser.parse_args()
    try:
        seq_publisher(args.seq, args.period)

    except rospy.ROSInterruptException:
        print "failed to replay message"
