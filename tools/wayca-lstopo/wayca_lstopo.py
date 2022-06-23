#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2022 HiSilicon Technologies Co., Ltd.
# Wayca scheduler is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
# http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#
# See the Mulan PSL v2 for more details.
#

"""
ultility to export wayca calibration data to a human readable format
"""

from enum import IntFlag, auto

import configparser
import subprocess
import traceback
import tempfile
import argparse
import logging
import sys
import os
import re


class TopoType(IntFlag):
    """
    enumeration type that describes the topological range which we would display
    """
    CPU = auto()
    NUMA = auto()


def check_binary(filepath):
    """
    Confirm whether the current file exists in the system.
    """
    if os.path.exists(filepath):
        return os.path.realpath(filepath)
    return None


def get_default_path():
    """
    If the user does not specify the path of the corresponding binary in
    the configuration file, we will go to the system path to find these
    binaries through which, and if not found, return a default path.
    """
    cmd = ["which", "lstopo-no-graphics"]
    logging.debug('get lstopo default path cmd:\n%s', cmd)
    ret = subprocess.run(cmd, capture_output=True)
    if ret.returncode:
        path = None
    else:
        #strip LF from result
        path = ret.stdout.decode('utf8').strip()
    if path is None:
        path = '/usr/bin/lstopo-no-graphics'
    return path


class WaycaLstopoArgs:
    """
    Class used to save user arguments.
    It is designed as a singleton pattern so that user parameter
    could be easily accessed in any context.
    """
    init = False
    __instance = None
    def __new__(cls, args=None, help_string=None):
        if cls.__instance is None:
            cls.__instance = object.__new__(cls)
        return cls.__instance

    def __init__(self, args=None, help_string=None):
        if self.__class__.init:
            return

        if args is None:
            raise ValueError("args is empty")

        self.__topo_type = None
        self.__scope = [-1, -1]
        self.__left_args = args[1]
        self.__load_helpinfo(help_string)
        self.__load_config()
        self.__get_args(args[0])

        #help info need lstopo so we print helpinfo after lstopo has been inited.
        if args[0].help:
            self.print_help()
            self.__class__.init = True
            return
        self.__validate_args()
        self.__class__.init = True

    def __load_helpinfo(self, help_string):
        self.__helpinfo = help_string
        self.__help_calibration = """Invalid calibration data!
Calibration data may not exist or scope may out of range.
Try to get an calibration data with wayca-calibration.

For more details, see: wayca-calibration --help
"""
    def print_help(self):
        ret = subprocess.run([self.lstopo, '--help'], capture_output=True)
        if ret.returncode:
            logging.error("\n%s", ret.stderr.decode('utf8'))
            raise RuntimeError("get lstopo help info failed")

        lstopo_help = ret.stdout.decode('utf8')
        help_iter = iter(lstopo_help.splitlines(keepends=True))
        for line in help_iter:
            if re.search(r'(Supported)', line, re.I) is not None:
                break

        print(self.__helpinfo)
        print('lstopo options:')
        print('---------------')
        print(''.join(list(help_iter)))

    def __get_args(self, args):
        self.__only = args.wayca_only
        self.__wayca_type = args.wayca_type
        if args.wayca_data is not None:
            self.__wayca_data = args.wayca_data
        self.__parse_scope(args.wayca_scope)

    def __load_config(self):
        config = configparser.ConfigParser()
        config.read("/etc/wayca-scheduler/wayca_lstopo.conf")

        self.__lstopo = config.get('General', 'lstopo',
                                 fallback=get_default_path())
        self.__wayca_data = config.get('General', 'wayca_data',
                                 fallback="/etc/wayca-scheduler/data.xml")

        log_level = config.get('General', 'log_level', fallback="error")
        str2log = {
                    "error": logging.ERROR, "warning": logging.WARNING,
                    "info": logging.INFO, "debug": logging.DEBUG}
        logger = logging.getLogger()
        logger.setLevel(level=str2log.get(log_level, logging.ERROR))

    def __validate_args(self):
        self.__lstopo = check_binary(self.__lstopo)
        if self.__lstopo is None:
            raise ValueError("lstopo-no-graphics not found")
        self.__wayca_data = check_binary(self.__wayca_data)
        if self.__wayca_data is None and self.__topo_type is not None:
            logging.error(self.help_calibration)
            raise ValueError("calibration data file not found")

    def __parse_scope(self, scope):
        """ match strings like cpu:1 4 or cpu:1-4 """
        if scope is None:
            logging.info('No calibration data will be present')
            return
        result = re.match(r'(cpu|numa):(\d+)(\x20|-)(\d+)$', scope, re.I)
        if result is None or int(result.group(2)) > int(result.group(4)):
            raise ValueError("Invalid wayca-scope")
        self.__topo_type = TopoType[result.group(1).upper()]
        self.__scope[0] = int(result.group(2))
        self.__scope[1] = int(result.group(4))

    @property
    def lstopo(self):
        return self.__lstopo

    @property
    def wayca_data(self):
        return self.__wayca_data

    @property
    def topo_type(self):
        return self.__topo_type

    @property
    def only(self):
        return self.__only

    @property
    def wayca_type(self):
        return self.__wayca_type

    @property
    def left_args(self):
        return self.__left_args

    @property
    def scope(self):
        return self.__scope

    @property
    def help_calibration(self):
        return self.__help_calibration

    def __str__(self):
        print_format = '{:20}: {:<}\n'
        string = '\nUser args:\n' +\
                    print_format.format('Lstopo path', self.lstopo) +\
                    print_format.format('Topo type', self.topo_type.name
                                        if self.topo_type is not None
                                        else "") +\
                    print_format.format('Scope',
                                        ','.join(str(a) for a in self.scope)) +\
                    print_format.format('Wayca only',
                                        'Yes' if self.only else 'No') +\
                    print_format.format('Data type', self.wayca_type) +\
                    print_format.format('Data file', self.wayca_data
                                        if self.wayca_data is not None
                                        else "") +\
                    print_format.format('lstopo args', ' '.join(self.left_args))
        return string


def parse_args():
    """ parse the command line argument """
    parser = argparse.ArgumentParser(
        description='An wrapper for lstopo-no-graphics.'
                    'This wrapper add support for displaying performance data of this system',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        add_help=False,
        epilog="""
Examples:
---------

To get the calibration data of cpu 0 to cpu 4:
        %(prog)s --wayca-scope "cpu:0 4"
""")
    parser.add_argument(
        '--wayca-scope',
        help="Data scope which would be displayed, format \"cpu: 0 20\", "
        "default diaplay all the cpu data.")
    parser.add_argument(
        '--wayca-type', choices=['all', 'latency', 'bandwidth'], default='all',
        help="Data type which shoud be display, default all.")
    parser.add_argument(
        '--wayca-data', help="Which Calibration data file will be used.")
    parser.add_argument(
        '--wayca-only', action='store_true',
        help="Display calibration data only.")
    parser.add_argument(
        '-h', '--help', action='store_true',
        help="show this help message and exit.")

    args = parser.parse_known_args()
    help_string = parser.format_help()
    wayca_args = WaycaLstopoArgs(args=args, help_string=help_string)
    if args[0].help:
        sys.exit()
    logging.debug(wayca_args)


def get_lstopo_output():
    if WaycaLstopoArgs().only:
        return ''
    cmd = [WaycaLstopoArgs().lstopo]
    cmd.extend(WaycaLstopoArgs().left_args)
    logging.debug('lstopo cmd:\n%s', cmd)
    ret = subprocess.run(cmd, shell=False, capture_output=True)
    error_out = ret.stderr.decode('utf8')
    if ret.returncode:
        logging.error("\n%s", error_out)
        raise RuntimeError(f"{cmd} excute failed")
    if len(error_out) > 1:
        WaycaLstopoArgs().print_help()
        sys.exit(1)
    return ret.stdout.decode('utf8')


def get_topo_size(line):
    result = re.search(r'between (\d+)', line, re.I)
    return int(result.group(1))


class WaycaMatrix():
    """ Class representing a calibration data matrix at a level """
    def __init__(self, dis_iter, line, title, scope=None):
        self.__minor = []
        self.__matrix = []
        self.__title = title
        self.__topo_size = get_topo_size(line)
        for _ in range(1, self.__topo_size + 2):
            self.__matrix.append(next(dis_iter).split())

        if scope is None:
            scope = [0, self.__topo_size - 1]

        self.__slice(scope[0], scope[1])

    def __slice(self, start, stop):
        """ Cut out the required minor from matrix """
        if stop >= self.__topo_size:
            logging.error(WaycaLstopoArgs().help_calibration)
            raise ValueError("Invalid wayca scope")

        self.__minor = []
        first_line = [self.__matrix[0][0]]
        first_line.extend(self.__matrix[0][start + 1 : stop + 2])
        self.__minor.append(first_line)
        for i in range(start + 1, stop + 2):
            row = [self.__matrix[i][0]]
            row.extend(self.__matrix[i][start + 1 : stop + 2])
            self.__minor.append(row)

    def __str__(self):
        """ Convert minor to string and align it """
        if len(self.__minor) == 0:
            return ''

        string = f'{self.__title}\n'
        for row in self.__minor:
            string_list = [str(x) for x in row]
            string += '\t'.join(string_list) + '\n'
        return string


def find_title(line):
    """
    The valid titile is like {banchmark}{topo}{LAT|Band}{cache}({unit})
    E.g. WAYCACPULATL1CACHE(MB)
    """
    topo_type = WaycaLstopoArgs().topo_type
    wayca_type = WaycaLstopoArgs().wayca_type
    if wayca_type == "latency":
        wayca_type = '(Lat)'
    elif wayca_type == "bandwidth":
        wayca_type = '(Band)'
    else:
        wayca_type = '(Lat|Band)'

    topo_name = f'({topo_type.name})'
    cache = r'(.*?)'
    unit = r'\((.*?)\)'
    result = re.search(r'(WAYCA|STREAM)' + topo_name + wayca_type +
                       cache + unit, line, re.I)
    if result is not None:
        return f'{result.group(1)} {result.group(2)} {result.group(4)} '\
               f'{result.group(3)} ({result.group(5)})'
    return None


def get_calibration_output():
    if WaycaLstopoArgs().topo_type is None:
        return ''

    xml = WaycaLstopoArgs().wayca_data
    cmd = [WaycaLstopoArgs().lstopo, '-i', xml, '--distances']
    scope = WaycaLstopoArgs().scope
    tmp_buf = ''

    logging.debug('get calibration data cmd:\n%s', cmd)
    ret = subprocess.run(cmd, capture_output=True)
    if ret.returncode:
        logging.error("\n%s", ret.stderr.decode('utf8'))
        raise RuntimeError("get calibration data failed")

    dis_iter= iter(ret.stdout.decode('utf8').splitlines())
    for line in dis_iter:
        title = find_title(line)
        if title is not None:
            matrix = WaycaMatrix(dis_iter, line, title, scope)
            tmp_buf += format(matrix)

    if len(tmp_buf) == 0:
        logging.error(WaycaLstopoArgs().help_calibration)
        raise ValueError("calibration data not found")
    return tmp_buf


def main():
    """ Program main function """
    try:
        logging.basicConfig(format='wayca-lstopo.%(levelname)s:%(message)s')
        parse_args()
        tmp_buf = ''
        tmp_buf += get_lstopo_output()
        tmp_buf += get_calibration_output()
        print(tmp_buf)

    except Exception:
        traceback.print_exc()
        sys.exit("excute error!")


if __name__ == "__main__":
    main()
