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
ultility to perform tests and export data to a human readable format
"""

from enum import IntFlag, auto
from abc import abstractmethod

import configparser
import subprocess
import traceback
import tempfile
import argparse
import logging
import copy
import csv
import sys
import os
import re


# The real version will be inserted here by cmake, don't change this line
WAYCA_CALIBRATION_VERSION = 0.0


group_name = None
group_name_init = False
DEF_MEMTEST_SIZE = 335544320


class CalibrationScope(IntFlag):
    """
    enumeration type that describes the topological ranges on which
    our tests will be performed
    """
    CPU = auto()
    CLUSTER = auto()
    NUMA = auto()
    ALL = CPU | CLUSTER | NUMA


class CalibrationType(IntFlag):
    """
    enumeration type that describes the benchmark type on which
    our tests will be performed
    """
    STREAM = auto()
    WAYCA = auto()
    ALL = STREAM | WAYCA


class CalibrationCacheType(IntFlag):
    """
    enumeration type that describes different caches
    """
    L1DCACHE = auto()
    L2CACHE = auto()
    L3CACHE = auto()
    ALL = L1DCACHE | L2CACHE | L3CACHE


def excute(cmd, out=None, log="unknown"):
    """
    encapsulation of subprocess, adding debugging info and exception handling
    """
    logging.info("%s cmd: %s", log, cmd)
    ret = subprocess.run(cmd, stdout=out, stderr=out)
    if out is not None and out != subprocess.DEVNULL:
        out.seek(0)
    if ret.returncode:
        if out is not None or out != subprocess.DEVNULL:
            logging.error("\n%s", out.read().decode('utf8'))
        raise RuntimeError(f"{log} failed")


class CalibrationTopo:
    """
    This class provides required topology information. And These information
    is obtained by using the hwloc utils().

    A Singleton Pattern is used since the topology information will never
    change in the system.

    No Arguments are required, But WaycaArgs has to be initialed before this
    class, since hwloc binary path is needed.

    Topology information is divided into two categories: cache and basic
    topology units, which are stored in related lists. If a particular
    topology level does not exist, it will not be instantiated, so it will not
    exist in the lists, and subsequent tests will skip the related topology
    """
    init = False
    __instance = None
    def __new__(cls):
        if cls.__instance is None:
            cls.__instance = object.__new__(cls)
        return cls.__instance

    def __init__(self):
        if self.__class__.init:
            return
        self.__lstopo = WaycaArgs().lstopo
        self.__cache = {}
        self.__basictopo = {}
        for i in CalibrationCacheType:
            if i == CalibrationCacheType.ALL:
                continue
            cache = CalibrationCache(self.__lstopo, i)
            if cache is None:
                continue
            self.__cache.update({i : cache})
        for i in CalibrationScope:
            if i == CalibrationScope.ALL:
                continue
            basic_topo = CalibrationBasicTopo(self.__lstopo, i)
            if basic_topo is None:
                continue
            self.__basictopo.update({i : basic_topo})

        self.__class__.init = True

    def caches(self):
        return self.__cache

    def basictopo(self):
        return self.__basictopo

    def __str__(self):
        print_format = '{}\n'
        string = 'Wayca Topo:\n'
        for i in self.basictopo().items():
            string += print_format.format(i[1])

        for i in self.caches().items():
            string += print_format.format(i[1])
        return string


def convert2byte(size):
    size = re.findall(r'[A-Za-z]+|\d+', size)
    if size[1].lower() == 'b':
        return int(size[0])
    if size[1].lower() == 'kb':
        return 1024 * int(size[0])
    if size[1].lower() == 'mb':
        return 1024 * 1024 * int(size[0])
    if size[1].lower() == 'gb':
        return 1024 * 1024 * 1024 * int(size[0])
    if size[1].lower() == 'gb':
        return 1024 * 1024 * 1024 * int(size[0])
    raise RuntimeError(f"cache size error {size}")


def get_cache_size(lstopo_path:str, cache_type:CalibrationCacheType):
    """
    Get cache size by using lstopo:
    lstopo --only {toponame}
    """
    with tempfile.TemporaryFile() as tmp:
        cache_name = cache_type.name.lower()
        cmd = [lstopo_path, "--only", cache_name]
        excute(cmd, out=tmp, log="get cache size")
        cache_string = tmp.readline().split()
        if len(cache_string) == 0:
            return 0
        cache_size = cache_string[-1].decode('utf8').strip('(')
        cache_size = cache_size.strip(')')
        cache_size = convert2byte(cache_size)
        return cache_size

class CalibrationCache:
    """
    CalibrationCache objects are used by CalibrationTopo, each object
    present a cache in this system.

    If the information of the specified cache type cannot be obtained
    through lstopo, the fallback value configured by the user in conf
    file will be used. If the user does not configure it, it will fail
    to be instantiated and return None without exception.
    """
    def __new__(cls, lstopo_path:str, cache_type:CalibrationCacheType):
        if get_cache_size(lstopo_path, cache_type) == 0 and \
                WaycaArgs().def_cache(cache_type) == 0:
            return None
        return object.__new__(cls)

    def __init__(self, lstopo_path:str, cache_type:CalibrationCacheType):
        self.__cache_type = cache_type
        self.__size = get_cache_size(lstopo_path, cache_type)
        if self.__size == 0:
            self.__size = WaycaArgs().def_cache(cache_type)

    @property
    def size(self):
        return self.__size

    @property
    def topotype(self):
        return self.__cache_type

    def __str__(self):
        print_format = '{:22}: {:>20}\n'
        return 'Cache\n' +\
                print_format.format('type', self.topotype.name) +\
                print_format.format('size', self.size)


def get_hwloc_topo_name(name, lstopo=''):
    """
    The names of some topo levels in hwloc may be different from
    the commonly used name.

    This function converts our commonly used topo level names into
    hwloc-like names
    """
    global group_name
    global group_name_init
    if name == CalibrationScope.CLUSTER:
        if group_name_init is False:
            if len(lstopo) == 0:
                raise RuntimeError("error lstopo path")
            group_name =  get_group_name(lstopo)
            group_name_init = True
        return group_name
    if name == CalibrationScope.NUMA:
        return "numa"
    if name == CalibrationScope.CPU:
        return "core"
    if name == CalibrationScope.ALL:
        return "all"
    raise RuntimeError("unsupported type!")


def valid_topo_level(lstopo:str, hwloc_name):
    with tempfile.TemporaryFile() as tmp:
        cmd = [lstopo, "--only", hwloc_name]
        excute(cmd, out=tmp, log=f"valid topo level {hwloc_name}")
        if tmp.readline().decode('utf8').lower().find('unsupported') != -1:
            return False
        return True


def get_group_name(lstopo:str):
    """
    get the group name of CLUSTER.

    CLUSTER topo level has been called "group" in lstopo. There may be
    multiple groups in the system, some of which are used to describe
    cluster, and some groups may be used to describe other topo structures
    """
    with tempfile.TemporaryFile() as tmp:
        cmd = [lstopo, "--only", "group"]
        excute(cmd=cmd, out=tmp, log="get group name")
        content = tmp.read().decode('utf8')
        if content.lower().find('unsupported') != -1:
            return None
        logging.debug("\n%s", content)
        group_pat = re.compile(r'\b[A-Za-z0-9]*\b(?=\(\bcluster\b\))')
        groupname = re.search(group_pat, content.lower())
        if groupname is None:
            return None
        return groupname.group()


class CalibrationBasicTopo:
    """
    CalibrationBasicTopo objects are used by CalibrationTopo, each object
    present a topo level in this system. Now, we only support CPU, CLUSTER,
    and NUMA level.

    If the information of the specified topo cannot be obtained through lstopo,
    it will fail to be instantiated and return None without exception.
    """
    def __new__(cls, lstopo_path:str, topotype:CalibrationScope):
        name = get_hwloc_topo_name(topotype, lstopo=lstopo_path)
        if name is None:
            return None
        if valid_topo_level(lstopo_path, name):
            return object.__new__(cls)
        return None

    def __init__(self, lstopo_path:str, topotype:CalibrationScope):
        if self is None:
            return
        self.__hwloc_cal = os.path.dirname(lstopo_path) + '/hwloc-calc'
        self.__topotype = topotype
        self.__cnt = self.__get_cnt()
        self.__cpu_nr = self.__get_cpu_nr()

    def __hwloc_cal_num(self, element, scope):
        """
        hwloc-cal -N {element} {scope}
        """
        with tempfile.TemporaryFile() as tmp:
            cmd = [self.__hwloc_cal, "-N", element, scope]
            excute(cmd=cmd, out=tmp, log=f"get {element} in {scope}")
            cnt = tmp.readline()
            return int(cnt)

    def __get_cnt(self):
        return self.__hwloc_cal_num(get_hwloc_topo_name(self.topotype), "all")

    def __get_cpu_nr(self):
        scope = get_hwloc_topo_name(self.topotype)
        scope += ':0'
        return self.__hwloc_cal_num("core", scope)

    @property
    def topotype(self):
        return self.__topotype

    @property
    def cnt(self):
        return self.__cnt

    @property
    def cpu_nr(self):
        return self.__cpu_nr

    def __str__(self):
        print_format = '{:22}: {:>20}\n'
        return 'Topo element\n' +\
                print_format.format('type', self.topotype.name) +\
                print_format.format('cnt', self.cnt) +\
                print_format.format('cpu_nr', self.cpu_nr)


class WaycaArgs:
    init = False
    __instance = None
    def __new__(cls, args=None):
        if cls.__instance is None:
            cls.__instance = object.__new__(cls)
        return cls.__instance

    def __init__(self, args=None):
        if self.__class__.init:
            return

        if args is None:
            raise ValueError("args is empty")

        self.__wayca = None
        self.__stream = None
        self.__tmpdir = None
        self.__conf_file = args.config_file
        self.__cont_test = args.continue_test
        self.__test_type = CalibrationType[args.type.upper()]
        self.__scope = CalibrationScope[args.scope.upper()]
        if self.__test_type is CalibrationType.WAYCA and \
                not self.__scope & CalibrationScope.CPU:
            raise RuntimeError(f"wayca-memory-bench do not support " \
                               f"scope:{self.__scope.name}")
        self.__load_config()
        self.__set_xmlfile(args.filename)
        self.__class__.init = True

    def __load_config(self):
        config = configparser.ConfigParser()
        config.read(self.__conf_file)
        lstopo_path = config.get('General', 'HwlocBinaryPath',
                            fallback=get_default_path('lstopo-no-graphics'))
        self.__lstopo = self.check_binary(lstopo_path)
        dirname = os.path.dirname(self.__lstopo)
        self.check_binary(dirname + '/hwloc-bind')
        self.check_binary(dirname + '/hwloc-info')
        self.check_binary(dirname + '/hwloc-calc')
        self.check_binary(dirname + '/hwloc-annotate')

        if self.__test_type is not CalibrationType.STREAM:
            wayca_path = config.get('Wayca', 'WaycaPath',
                                fallback=get_default_path('wayca-memory-bench'))
            self.__wayca = self.check_binary(wayca_path)

        if self.__test_type is not CalibrationType.WAYCA:
            stream_path = config.get('Stream', 'StreamPath',
                                    fallback=get_default_path('stream'))
            self.__stream = self.check_binary(stream_path)
        self.__tmpdir = config.get('General', 'TempPath', fallback=None)
        self.__def_l1cache = config.getint('GoBack', 'L1CacheSize', fallback=0)
        self.__def_l2cache = config.getint('GoBack', 'L2CacheSize', fallback=0)
        self.__def_l3cache = config.getint('GoBack', 'L3CacheSize', fallback=0)

    @staticmethod
    def check_binary(filepath):
        """
        Confirm whether the current binary exists in the system.
        """
        if os.path.exists(filepath):
            return os.path.realpath(filepath)
        raise RuntimeError(f"{filepath} not exists.")

    def validate_hwloc_xml(self):
        """
        lstopo -i {filename}
        """
        if os.path.exists(self.__xmlfile) is False:
            return
        cmd = [self.__lstopo, "-i", self.__xmlfile]
        excute(cmd=cmd, out=subprocess.DEVNULL,
                log=f"valid xml {self.__xmlfile}")

    def __set_xmlfile(self, filename):
        """ check whether filename is valid """
        if filename is None:
            self.__xmlfile = os.path.realpath(os.getcwd() + '/data.xml')
        else:
            self.__xmlfile = os.path.realpath(filename)
            self.validate_hwloc_xml()

    def def_cache(self, cache_type):
        """
        Returns the default cache size set by the user.
        """
        if cache_type == CalibrationCacheType.L1DCACHE:
            return self.__def_l1cache
        if cache_type == CalibrationCacheType.L2CACHE:
            return self.__def_l2cache
        if cache_type == CalibrationCacheType.L3CACHE:
            return self.__def_l3cache

        raise RuntimeError("wrong cache type")

    @property
    def xmlfile(self):
        return self.__xmlfile

    @property
    def lstopo(self):
        return self.__lstopo

    @property
    def stream(self):
        return self.__stream

    @property
    def wayca(self):
        return self.__wayca

    @property
    def test_type(self):
        return self.__test_type

    @property
    def cont_test(self):
        return self.__cont_test

    @property
    def scope(self):
        return self.__scope

    @property
    def tmpdir(self):
        return self.__tmpdir

    def __str__(self):
        print_format = '{:30}: {:<}\n'
        string = 'User args:\n' +\
                    print_format.format('XML file path', self.xmlfile) +\
                    print_format.format('Test type', self.test_type.name) +\
                    print_format.format('Test scope', self.scope.name) +\
                    print_format.format('conf file', self.__conf_file) + \
                    print_format.format('continue test', self.cont_test)

        string += '\nConfiguration: \n' + \
                    print_format.format('lstopo path', self.lstopo)
        if self.wayca is not None:
            string += print_format.format('wayca-memory-bench path', self.wayca)
        if self.stream is not None:
            string += print_format.format('stream path', self.stream)
        for i in CalibrationCacheType:
            if i == CalibrationCacheType.ALL:
                continue
            name = f'def {i.name} size'
            string += print_format.format(name, self.def_cache(i))
        return string


def get_default_path(binary):
    """
    If the user does not specify the path of the corresponding binary in
    the configuration file, we will go to the system path to find these
    binaries through which, and if not found, return a default path.
    """
    with tempfile.TemporaryFile() as tmp:
        cmd = ["which", str(binary)]
        ret = subprocess.run(cmd, stdout=tmp, stderr=tmp)
        logging.info("get default path: %s", cmd)
        tmp.seek(0)
        if ret.returncode:
            path = None
        else:
            path = tmp.readline().decode('utf8').strip()
    if path is None:
        if binary == 'lstopo-no-graphics':
            path = '/usr/bin/lstopo-no-graphics'
        elif binary == 'stream':
            path = '/usr/lib/lmbench/bin/stream'
        elif binary == 'wayca-memory-bench':
            path = '/usr/bin/wayca-memory-bench'
        else:
            raise RuntimeError("Can't find a default path.")
    return path


class CalibrationPerTest():
    """
    The abstract class of all testcases class, all testcases inherit
    from this class, and implement get_formance and export_perf_data methods

    All testcase should support a breakpoint based retesting. This could be
    achieved by saving the test data in a temporary file. When the user
    specifies the "-c" parameter, object should read the original data
    from the existing temporary file.
    """
    def __init__(self, test_type:CalibrationType):
        self.__tmpfile = {}
        wayca_args = WaycaArgs()
        self.__topo = CalibrationTopo()
        self.__test_type = test_type
        self.__finish = False
        self.__xmlfile = wayca_args.xmlfile
        self.__hwloc_bind = os.path.dirname(wayca_args.lstopo) +\
                            '/hwloc-bind'
        self.__hwloc_info = os.path.dirname(wayca_args.lstopo) +\
                            '/hwloc-info'
        self.__hwloc_annotate = os.path.dirname(wayca_args.lstopo) + \
                                '/hwloc-annotate'
        self.__lstopo = wayca_args.lstopo
        self.__cont_test = wayca_args.cont_test

    @abstractmethod
    def get_performance(self):
        """
        This method is used to execute test cases. All subclasses need
        to implement this method.
        """
        pass

    @abstractmethod
    def export_perf_data(self):
        """
        This method is used to export performance data to HWLOC XML. All
        subclasses need to implement this method.
        """
        pass

    def register_tmpfile(self, name="calibration"):
        """
        This method will generate a temporary file prefixed with name,
        and the directory of the temporary file is determined by the
        user's configuration file.

        Users can use name to obtain the corresponding file object
        through the tmpfile() method.
        """
        if self.cont_test:
            tmpdir = WaycaArgs().tmpdir
            if tmpdir is None:
                tmpdir = tempfile.gettempdir()
            for filename in os.listdir(tmpdir):
                fp = os.path.join(tmpdir, filename)
                if os.path.isfile(fp) and name in filename:
                    tmpfile = open(fp, "r+")
                    self.__tmpfile.update({name: tmpfile})
                    return

        tmpfile = tempfile.NamedTemporaryFile(prefix=name,
                                                  dir=WaycaArgs().tmpdir,
                                                  delete=False, mode="w+")
        self.__tmpfile.update({name: tmpfile})

    def finish(self):
        """
        Used to mark the current test as complete, so that all temporary
        files that have been registered will be deleted at the end of the
        object's lifetime.
        """
        self.__finish = True

    def __del__(self):
        for tmpfile in self.__tmpfile.values():
            filename = tmpfile.name
            tmpfile.close()
            if self.__finish:
                os.remove(filename)

    @property
    def topo(self):
        return self.__topo

    @property
    def test_type(self):
        return self.__test_type

    def tmpfile(self, name):
        return self.__tmpfile[name]

    @property
    def hwloc_bind(self):
        return self.__hwloc_bind

    @property
    def hwloc_annotate(self):
        return self.__hwloc_annotate

    @property
    def hwloc_info(self):
        return self.__hwloc_info

    @property
    def xmlfile(self):
        return self.__xmlfile

    @property
    def cont_test(self):
        return self.__cont_test

    @property
    def lstopo(self):
        return self.__lstopo

    def __str__(self):
        print_format = '{:22}: {:<}\n'
        string = "Calibration perftest type:\n" + \
                    print_format.format("test_type", self.test_type.name)
        for tmpfile in self.__tmpfile.values():
            string += print_format.format("tmpfile", tmpfile.name)
        return string


def make_xmlfile(lstopo, xmlfile):
    excute(cmd=[lstopo, xmlfile], out=subprocess.DEVNULL,
            log=f"make xml {xmlfile}")


def init_dis_file(tmpfile, scope, prefix):
    """
    Organize the format of the temporary file into a way that
    HWLOC recognizes. The format is as follows:
    {test name}
    {flag}
    {obj num}
    {obj name[0]}
    ...
    {obj name[num - 1]}
    {data[0]}
    ...
    {data[num * num - 1]}
    """
    cur_topo = CalibrationTopo().basictopo()[scope]
    tmpfile.write(f"name=\"{prefix}\"\n")
    # flag 6 means a self defined data, more details in hwloc header file.
    tmpfile.write("6\n")
    tmpfile.write(f"{cur_topo.cnt}\n")
    for i in range(0, cur_topo.cnt):
        scope_name = get_hwloc_topo_name(scope, WaycaArgs().lstopo)
        tmpfile.write(f"{scope_name}:{i}\n")


def get_dis_breakpoint(tmpfile, scope, prefix):
    """
    Get breakpoint to avoid duplicating test.
    """
    with tempfile.TemporaryFile() as tmp:
        cmd = ['wc','-l', tmpfile.name]
        excute(cmd=cmd, out=tmp, log="get distances break point")
        lines = int(tmp.read().decode("utf8").split()[0])
        cur_topo = CalibrationTopo().basictopo()[scope]
        lines -= cur_topo.cnt + 3
        if lines < 0:
            tmpfile.seek(0)
            init_dis_file(tmpfile, scope, prefix)
            return {'ini': 0, 'tgt': 0}
        tmpfile.seek(0, 2)
    return {'ini': lines // cur_topo.cnt, 'tgt': lines % cur_topo.cnt}


def write_dis_data_to_xml(tmp_filename, xml_filename, hwloc_annotate):
    """
    Add Data to XML, our tempfile format could be recognized by
    the hwloc-annotate, relative command is as followd:
    hwloc_annotate {xmlfile} {xmlfile} -- dummy -- distances {tmpfile}
    """
    with tempfile.TemporaryFile() as tmp:
        cmd = [hwloc_annotate, xml_filename, xml_filename,
               "dummy", "distances", tmp_filename]

        excute(cmd=cmd, out=tmp,
                log=f"wayca export to xml {xml_filename}")


class WaycaPerfTest(CalibrationPerTest):
    def __new__(cls, scope:CalibrationScope):
        if CalibrationTopo().basictopo().get(scope) is None or \
                scope != CalibrationScope.CPU:
            return None
        return object.__new__(cls)

    def __init__(self, scope:CalibrationScope):
        super().__init__(CalibrationType.WAYCA)
        self.wayca = WaycaArgs().wayca
        self.__scope = scope
        self.__band_prefix = []
        self.__lat_prefix = []

    def export_perf_data(self):
        """
        export wayca-memory-bench perf data to hwloc XML.
        """
        if os.path.exists(self.xmlfile) is False:
            make_xmlfile(self.lstopo, self.xmlfile)
        for tmp in self.__band_prefix:
            self.__write_to_xml(tmp)
        for tmp in self.__lat_prefix:
            self.__write_to_xml(tmp)
        self.finish()

    def __write_to_xml(self, prefix):
        write_dis_data_to_xml(self.tmpfile(prefix).name, self.xmlfile,
                                   self.hwloc_annotate)

    def get_performance(self):
        """
        Perform wayca-memory-bench test.

        For each test, two temporary files are created to save latency
        and bandwidth data respectively.
        """
        for cache in self.topo.caches().values():
            band_prefix = CalibrationType.WAYCA.name + \
                            CalibrationScope.CPU.name + 'Band' +\
                            cache.topotype.name + '(MB)'
            lat_prefix = CalibrationType.WAYCA.name + \
                            CalibrationScope.CPU.name + 'Lat' + \
                            cache.topotype.name + '(ps)'
            self.__band_prefix.append(band_prefix)
            self.__lat_prefix.append(lat_prefix)
            self.register_tmpfile(band_prefix)
            self.register_tmpfile(lat_prefix)
            start = {'ini': 0, 'tgt': 0}
            if self.cont_test is False:
                self.__init_tmpfile(lat_prefix)
                self.__init_tmpfile(band_prefix)
            else:
                # retest from a breakpoint
                start.update(self.__get_breakpoint(lat_prefix, band_prefix))
            logging.info("continue :%s", start)
            self.__test_cache(cache, lat_prefix, band_prefix, start)
            self.tmpfile(lat_prefix).flush()
            self.tmpfile(band_prefix).flush()

    def __init_tmpfile(self, name):
        init_dis_file(self.tmpfile(name), self.__scope, name)

    def __get_breakpoint(self, lat_prefix, band_prefix):
        """
        Both files are flushed at the same time, so just return one breakpoint
        """
        get_dis_breakpoint(self.tmpfile(lat_prefix),
                                self.__scope, lat_prefix)
        return get_dis_breakpoint(self.tmpfile(band_prefix),
                                       self.__scope, band_prefix)

    def __test_cache(self, cache:CalibrationCache, lat_prefix, band_prefix,
                     start:dict):
        """
        wayca-memory-bench -l {memsize} -i {ini} -t {tgt}
        """
        cur_topo = self.topo.basictopo()[self.__scope]
        size = cache.size
        for i in range(start['ini'], cur_topo.cnt):
            for j in range(start['tgt'], cur_topo.cnt):
                with tempfile.TemporaryFile() as tmp:
                    cmd = [self.wayca, '-l', str(size),
                            '-i', str(i), '-t', str(j)]
                    excute(cmd=cmd, out=tmp, log="wayca test cache")
                    lat, band = self.__proc_wayca_data(tmp.read().decode('utf8'))

                    self.tmpfile(lat_prefix).write(f"{int(float(lat) * 1000)}\n")
                    self.tmpfile(band_prefix).write(f"{int(float(band))}\n")
                    self.tmpfile(lat_prefix).flush()
                    self.tmpfile(band_prefix).flush()
            start.update({'tgt': 0})

    @staticmethod
    def __proc_wayca_data(content):
        """
        Process the raw data returned by the wayca-memory-bench through
        regular expressions
        """
        latency_pat = re.compile(r'(?<=Measuring load latency:).-?\d+\.?\d*')
        bandwidth_pat = re.compile(r'(?<=Stream-copy bandwidth:).-?\d+\.?\d*')
        logging.debug("\n%s", content)
        lat = re.search(latency_pat, content).group().strip(' ')
        band = re.search(bandwidth_pat, content).group().strip(' ')
        logging.debug("%s %s", lat, band)
        return lat, band


class StreamPerfTest(CalibrationPerTest):
    """
    Class used to execute stream test cases.

    The raw data obtained by the stream will be written to a temporary file
    in CSV format to support retesting based on breakpoints.
    """
    def __new__(cls, scope:CalibrationScope):
        if CalibrationTopo().basictopo().get(scope) is None:
            return None
        return object.__new__(cls)

    def __init__(self, scope:CalibrationScope):
        super().__init__(CalibrationType.STREAM)
        self.stream = WaycaArgs().stream
        self.__scope = scope
        self.__fieldnames = ['scope', 'type', 'index', 'numa id', 'mem type',
                             'mem size', 'value']
        self.__prefix = CalibrationType.STREAM.name + 'COMMON' + scope.name
        self.register_tmpfile(self.__prefix)
        self.__has_dis = False
        if self.__scope == CalibrationScope.NUMA:
            self.__has_dis = True
            self.__dis_band_prefix = CalibrationType.STREAM.name + \
                                        CalibrationScope.NUMA.name + 'Band' +\
                                        '(MB)'
            self.register_tmpfile(self.__dis_band_prefix)
            self.__dis_lat_prefix = CalibrationType.STREAM.name + \
                                        CalibrationScope.NUMA.name + 'Lat' +\
                                        '(ps)'
            self.register_tmpfile(self.__dis_lat_prefix)

        self.__writer = csv.DictWriter(self.tmpfile(self.__prefix),
                                       fieldnames=self.__fieldnames)

    def export_perf_data(self):
        """
        The data of the stream test case is saved in a temporary file in
        csv format, we read the data again and write it into XML.
        """
        if os.path.exists(self.xmlfile) is False:
            make_xmlfile(self.lstopo, self.xmlfile)

        self.tmpfile(self.__prefix).seek(0)
        get_data_type = lambda x: 'StreamLatency(ps)' \
                            if x=='lat' else 'StreamBandwidth(MB)'
        self.__init_memattrs(get_data_type('lat'))
        self.__init_memattrs(get_data_type('band'))

        reader = csv.DictReader(self.tmpfile(self.__prefix),
                                fieldnames=self.__fieldnames)
        for row in reader:
            logging.debug("data in tmp: %s", row)
            ini = f"{get_hwloc_topo_name(self.__scope)}:{row['index']}"
            if row['mem type'] == 'DRAM':
                tgt = f"{CalibrationScope.NUMA.name.lower()}:{row['numa id']}"
            else:
                tgt = self.__get_cache_obj(ini, row['mem type'])
                if len(tgt) == 0:
                    tgt = f"{row['mem type']}:{row['index']}"

            self.__write_to_xml(ini, tgt, row['value'],
                                data_type=get_data_type(row['type']))
            continue

        if self.__has_dis:
            filename = self.tmpfile(self.__dis_lat_prefix).name
            write_dis_data_to_xml(filename, self.xmlfile, self.hwloc_annotate)
            filename = self.tmpfile(self.__dis_band_prefix).name
            write_dis_data_to_xml(filename, self.xmlfile, self.hwloc_annotate)

        self.finish()

    def __get_cache_obj(self, ini, cachetype):
        """
        Get the name and index of the currently tested cache object in hwloc.
        The final data is presented as follows:

        Memory attribute #9 name `StreamBandwidth' flags 5
        L2 L#69 = 18992 from Core L#69
        ...
        L3 L#3 = 11138 from Core L#126


        hwloc-info -s --descendants {cachetype} {ini}
        hwloc-info -s --ancestor {cachetype} {ini}
        """
        with tempfile.TemporaryFile() as tmp:
            def get_cache(tmp, search_scope):
                cmd = [self.hwloc_info, "-s", search_scope, cachetype, ini]
                obj = ""
                try:
                    excute(cmd=cmd, out=tmp, log="get descendants cache obj")
                    obj = tmp.readline().decode('utf8')
                except RuntimeError:
                    pass
                except Exception as err:
                    raise err
                return obj
            cacheobj = get_cache(tmp, "--descendants")
            if len(cacheobj) != 0:
                return cacheobj.replace("\n", "")
            cacheobj = get_cache(tmp, "--ancestor")
            return cacheobj.replace("\n", "")

    def __write_to_xml(self, initiator, target, value, data_type='Latency'):
        """
        Write data to hwloc XML.

        data_type must be Latency or Bandwidth

        command format is as followd:
        hwloc-annotate {fromfile} {tofile} {tgt} memattr {cmd_type} {ini} {val}
        """
        with tempfile.TemporaryFile() as tmp:
            cmd = [self.hwloc_annotate, self.xmlfile, self.xmlfile,
                   target, "memattr", data_type, initiator, value]
            excute(cmd=cmd, out=tmp,
                    log=f"export stream data to xml {self.xmlfile}")

    def __init_memattrs(self, data_type):
        """
        Add new memattrs, cmd format:
        hwloc-annotate {fromfile} {tofile} ignored memattr {cmd_type} need_init,higher
        """
        with tempfile.TemporaryFile() as tmp:
            cmd = [self.hwloc_annotate, self.xmlfile, self.xmlfile,
                   "ignored", "memattr", data_type, "need_init,higher"]
            ret = subprocess.run(cmd, stdout=tmp, stderr=tmp)
            logging.info("Add data to xml cmd: %s", cmd)
            if ret.returncode:
                tmp.seek(0)
                string = tmp.read().decode('utf8')
                if "resource busy" not in string:
                    logging.error("\n%s", string)
                    raise RuntimeError(f"excute stream failed {cmd}")

    def get_performance(self):
        max_cache_size = 0
        for cache in self.topo.caches().values():
            self.__test_cache(cache)
            max_cache_size = max(cache.size, max_cache_size)
        self.__test_memory(max_cache_size * 10)

    def __get_data_lines(self, name):
        with tempfile.TemporaryFile() as tmp:
            cmd = ['grep','-nrc', name, self.tmpfile(self.__prefix).name]
            subprocess.run(cmd, stdout=tmp, stderr=tmp)
            logging.info("get stream breakpoint cmd: %s", cmd)
            tmp.seek(0)
            lines=int(tmp.read().decode("utf8"))
            self.tmpfile(self.__prefix).seek(0, 2)
            if lines <= 0:
                return 0
        return lines

    def __get_cache_breakpoint(self, cache:CalibrationCache):
        return self.__get_data_lines(cache.topotype.name) // 2

    def __get_mem_breakpoint(self):
        lines = self.__get_data_lines("DRAM") // 2
        numa_topo = self.topo.basictopo()[CalibrationScope.NUMA]
        return {'ele': lines // numa_topo.cnt, 'numa': lines % numa_topo.cnt}

    def __test_cache(self, cache:CalibrationCache):
        """
        hwloc-bind {topotype}:{idx} -- stream -M {memsize}K -P {nr} -W 2 -N 5
        """
        if self.__scope not in self.topo.basictopo():
            return
        cur_topo = self.topo.basictopo()[self.__scope]
        cpu_nr = cur_topo.cpu_nr
        topo_name = get_hwloc_topo_name(self.__scope)
        size = cache.size
        start = 0
        if self.cont_test:
            # retest from a breakpoint
            start = self.__get_cache_breakpoint(cache)
            logging.info("stream %s test continue from %s",
                         cache.topotype.name, start)
        for i in range(start, cur_topo.cnt):
            with tempfile.TemporaryFile() as tmp:
                cmd = [self.hwloc_bind, topo_name + ":" + str(i), "--",
                       self.stream, "-M", str(size//1024) + "K", "-P",
                       str(cpu_nr), "-W", "2", "-N", "5"]
                excute(cmd=cmd, out=tmp, log="stream test cache")
                lat, band = self.__proc_stream_data(tmp.read().decode('utf8'))
                lat_data = {
                            'scope': self.__scope.name,
                            'type': 'lat',
                            'index': i,
                            'numa id': -1,
                            'mem type': cache.topotype.name,
                            'mem size': size//1024,
                            'value': str(float(lat)*1000)}
                self.__writer.writerow(lat_data)

                band_data = copy.deepcopy(lat_data)
                band_data.update({'type': 'band','value': band})
                self.__writer.writerow(band_data)
                self.tmpfile(self.__prefix).flush()

    @staticmethod
    def __proc_stream_data(content):
        """
        Process the raw data returned by the stream through regular expressions
        """
        latency_pat = re.compile(r'(?<=copy latency:).\d+\.?\d*')
        bandwidth_pat = re.compile(r'(?<=copy bandwidth:).\d+\.?\d*')
        logging.debug("stream data:\n%s", content)
        lat = re.search(latency_pat, content).group().strip(' ')
        band = re.search(bandwidth_pat, content).group().strip(' ')
        logging.debug("lat:%s band:%s", lat, band)
        return lat, band

    def __prep_dis_file(self):
        if self.__has_dis is False:
            return
        lat_prefix = self.__dis_lat_prefix
        band_prefix = self.__dis_band_prefix
        if self.cont_test:
            get_dis_breakpoint(self.tmpfile(lat_prefix),
                               self.__scope, lat_prefix)
            get_dis_breakpoint(self.tmpfile(band_prefix),
                               self.__scope, band_prefix)
        else:
            init_dis_file(self.tmpfile(lat_prefix), self.__scope, lat_prefix)
            init_dis_file(self.tmpfile(band_prefix), self.__scope, band_prefix)

    def __write_dis_data(self, lat, band):
        if self.__has_dis is False:
            return
        lat_prefix = self.__dis_lat_prefix
        band_prefix = self.__dis_band_prefix
        self.tmpfile(lat_prefix).write(f"{int(float(lat) * 1000)}\n")
        self.tmpfile(band_prefix).write(f"{int(float(band))}\n")
        self.tmpfile(lat_prefix).flush()
        self.tmpfile(band_prefix).flush()

    def __test_memory(self, size):
        """
        hwloc-bind --cpubind {topotype}:{idx} --membind node:{numa_id} -- " \
              "stream -M {memsize}K -P {nr} -W 2 -N 5"
        """
        if size == 0:
            size = DEF_MEMTEST_SIZE
        cur_topo = self.topo.basictopo()[self.__scope]
        numa_topo = self.topo.basictopo()[CalibrationScope.NUMA]
        topo_name = get_hwloc_topo_name(self.__scope)
        start = {'ele': 0, 'numa': 0}
        if self.cont_test:
            # retest from a breakpoint
            start.update(self.__get_mem_breakpoint())
        self.__prep_dis_file()

        for i in range(start['ele'], cur_topo.cnt):
            for j in range(start['numa'], numa_topo.cnt):
                with tempfile.TemporaryFile() as tmp:
                    cmd = [self.hwloc_bind, "--cpubind",
                           topo_name + ":" + str(i), "--membind",
                           "node:" + str(j), "--", self.stream,
                           "-M", str(size//1024) + "K",
                           "-P", str(cur_topo.cpu_nr), "-W", "2", "-N", "5"]
                    excute(cmd=cmd, out=tmp, log="stream test memory")
                    streamdata = tmp.read().decode('utf8')
                    lat, band = self.__proc_stream_data(streamdata)

                    # write data to distances
                    self.__write_dis_data(lat, band)

                    lat_data = {'scope': self.__scope.name,
                                'type': 'lat',
                                'index': i,
                                'numa id': j,
                                'mem type': "DRAM",
                                'mem size': size//1024,
                                'value': str(float(lat)*1000)}
                    self.__writer.writerow(lat_data)

                    band_data = copy.deepcopy(lat_data)
                    band_data.update({'type': 'band', 'value': band})
                    self.__writer.writerow(band_data)
                    self.tmpfile(self.__prefix).flush()

            start.update({'numa': 0})


def parse_args():
    """ parse the command line argument """
    parser = argparse.ArgumentParser(
        description=f'wayca-calibration version:{WAYCA_CALIBRATION_VERSION}\n'
                    'Auto calibration tool used to automate performance '
                    'testing and export the results to an XML which '
                    'could be used by HWLOC',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
---------

To get the stream result on numa:
        %(prog)s -t stream -s numa data.xml
""")
    parser.add_argument(
        '-t', '--type', choices=['stream', 'wayca', 'all'],
        default='all', help="Which benchmark tool to use, default: all")
    parser.add_argument(
        '-s', '--scope', default='all',
        choices=['cpu', 'cluster', 'numa', 'all'],
        help="Which scope need be tested, default: all")
    parser.add_argument(
        'filename', metavar='Filename', nargs='?',
        help="Result XML filename")
    parser.add_argument(
        '--log-level', default='error',
        choices=['error', 'warning', 'info', 'debug'],
        help="Log level of this scripts")
    parser.add_argument(
        '-f', '--config-file',
        default='/etc/wayca-scheduler/wayca_calibration.conf',
        help="which configuration file will be used")
    parser.add_argument(
        '-c', '--continue-test', action='store_true',
        help="Continue unfinished work, use previous datas "
             "which get from speciefied path")

    args = parser.parse_args()
    str2log = {
                "error": logging.ERROR, "warning": logging.WARNING,
                "info": logging.INFO, "debug": logging.DEBUG}
    logger = logging.getLogger()
    logger.setLevel(level=str2log[args.log_level])

    wayca_args = WaycaArgs(args)
    print(wayca_args)


def testsuit(test_type:CalibrationType, test_scope:CalibrationScope):
    """
    Get CalibrationPerTest objects list, where each object represents
    a test case that the user needs.
    """
    suit = []
    for scope in CalibrationScope:
        if scope == CalibrationScope.ALL or bool(test_scope & scope) is False:
            continue

        if bool(test_type & CalibrationType.STREAM):
            perftest = StreamPerfTest(scope)
            if perftest is not None:
                suit.append(perftest)

        if bool(test_type & CalibrationType.WAYCA):
            perftest = WaycaPerfTest(scope)
            if perftest is not None:
                suit.append(perftest)
    return suit


def main():
    """ Program main function """
    try:
        logging.basicConfig(format='wayca_calibration.%(levelname)s:%(message)s')
        parse_args()
        print(CalibrationTopo())
        suit = testsuit(WaycaArgs().test_type, WaycaArgs().scope)
        for test in suit:
            print(test)
            test.get_performance()
            test.export_perf_data()

    except Exception:
        traceback.print_exc()
        sys.exit("excute error!")


if __name__ == "__main__":
    main()
