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
"""Global config access."""
import logging
import os

import gflags
import google.protobuf.text_format as text_format

import modules.hmi.proto.config_pb2 as config_pb2

gflags.DEFINE_string(
    'conf', 'modules/hmi/conf/config.pb.txt',
    'HMI config file, which should be text-formatted config proto.')


class Config(object):
    """Global config."""

    pb_singleton = None
    apollo_root = os.path.abspath(
        os.path.join(os.path.dirname(__file__), '../../..'))
    ros_root = os.path.abspath(os.path.join(
        os.environ.get('ROS_ROOT', '/apollo/ros/share/ros'), '../..'))

    log = logging.getLogger('HMI')
    record_replay_required_modules = [
        'GPS', 'control', 'canbus', 'localization', 'dreamview', 'record_bag']

    @classmethod
    def get_pb(cls):
        """Get a pb instance from the config."""
        if cls.pb_singleton is None:
            cls.__init_once(logging.DEBUG)
        return cls.pb_singleton

    @classmethod
    def __init_once(cls, log_level=logging.DEBUG):
        """Init config once."""
        # Init the config by reading conf file.
        with open(gflags.FLAGS.conf, 'r') as conf_file:
            cls.pb_singleton = text_format.Merge(conf_file.read(),
                                                 config_pb2.Config())
            cls.log.info('Get config: %s', str(cls.pb_singleton))

        # Init logger
        file_handler = logging.handlers.TimedRotatingFileHandler(
            cls.get_realpath(cls.pb_singleton.server.log_file),
            when='H', interval=1, backupCount=0)
        file_handler.setLevel(log_level)
        file_handler.setFormatter(logging.Formatter(
            '[%(name)s][%(levelname)s] %(asctime)s '
            '%(filename)s:%(lineno)s %(message)s'))
        file_handler.suffix = "%Y%m%d%H%M.log"
        cls.log.addHandler(file_handler)
        cls.log.setLevel(log_level)

    @classmethod
    def get_hardware(cls, hardware_name):
        """Get Hardware config by name."""
        return cls.__find_by_name(hardware_name, cls.get_pb().hardware)

    @classmethod
    def get_module(cls, module_name):
        """Get module config by name."""
        return cls.__find_by_name(module_name, cls.get_pb().modules)

    @classmethod
    def get_tool(cls, tool_name):
        """Get module config by name."""
        return cls.__find_by_name(tool_name, cls.get_pb().tools)

    @classmethod
    def get_map(cls, map_name):
        """Get map config by name."""
        return cls.__find_by_name(map_name, cls.get_pb().available_maps)

    @classmethod
    def get_vehicle(cls, vehicle_name):
        """Get map config by name."""
        return cls.__find_by_name(vehicle_name, cls.get_pb().available_vehicles)

    @classmethod
    def global_flagfile(cls):
        """Get global flagfile path."""
        return cls.get_realpath(cls.get_pb().global_flagfile)

    @classmethod
    def get_realpath(cls, path_str, from_ros_root=False):
        """
        Get realpath from a path string in config.

        1. Starting with '/' indicates an absolute path.
        2. from_ros_root=True indicates a relative path against ROS root.
        3. Otherwise it will be taken as a relative path against Apollo root.
        """
        if path_str.startswith('/'):
            return path_str
        if from_ros_root:
            return os.path.join(cls.ros_root, path_str)
        return os.path.join(cls.apollo_root, path_str)

    @staticmethod
    def __find_by_name(name, value_list):
        """Find a value in list by name."""
        return next((value for value in value_list if value.name == name), None)
