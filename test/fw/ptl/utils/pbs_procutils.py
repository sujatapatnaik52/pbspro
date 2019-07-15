# coding: utf-8

# Copyright (C) 1994-2019 Altair Engineering, Inc.
# For more information, contact Altair at www.altair.com.
#
# This file is part of the PBS Professional ("PBS Pro") software.
#
# Open Source License Information:
#
# PBS Pro is free software. You can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.
# See the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# For a copy of the commercial license terms and conditions,
# go to: (http://www.pbspro.com/UserArea/agreement.html)
# or contact the Altair Legal Department.
#
# Altair’s dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of PBS Pro and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair’s trademarks, including but not limited to "PBS™",
# "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
# trademark licensing policies.

import sys
import time
import re
import threading
import logging
import socket
import os

from ptl.utils.pbs_dshutils import DshUtils


class ProcUtils(object):

    """
    Utilities to query process information
    """

    logger = logging.getLogger(__name__)
    du = DshUtils()
    platform = sys.platform

    def __init__(self):
        self.processes = {}
        self.__h2ps = {}

    def get_ps_cmd(self, hostname=None):
        """
        Get the ps command

        :param hostname: hostname of the machine
        :type hostname: str or None
        """
        if hostname is None:
            hostname = socket.gethostname()

        if hostname in self.__h2ps:
            return self.__h2ps[hostname]

        if not self.du.is_localhost(hostname):
            platform = self.du.get_platform(hostname)
        else:
            platform = self.platform

        # set some platform-specific arguments to ps
        ps_arg = '-C'
        ps_cmd = ['ps', '-o', 'pid,rss,vsz,pcpu,pmem,size,cputime,command']

        self.__h2ps[hostname] = (ps_cmd, ps_arg)

        return (ps_cmd, ps_arg)

    def _init_processes(self):
        self.processes = {}

    def _get_proc_open_fds(self, hostname=None, pid=None):
        """
        Helper function to get open file descriptors by a process
        :param pid: Process id
        :type pid: str or None
        :returns: integer representing the count
        """
        ls_cmd = self.du.which(exe="ls")
        cmd = [ls_cmd, "-l", "/proc/"+pid+"/fd"]
        if not pid==None:
            ret = self.du.run_cmd(hostname, (cmd), level=logging.DEBUG2,
                                  sudo=True)
        else:
            return
        return len(ret['out'])

    def _get_proc_info_unix(self, hostname=None, name=None,
                            pid=None, regexp=False):
        """
        Helper function to ``get_proc_info`` for Unix only system
        """
        (ps_cmd, ps_arg) = self.get_ps_cmd(hostname)
        if name is not None:
            if not regexp:
                cr = self.du.run_cmd(hostname, (ps_cmd + [ps_arg, name]),
                                     level=logging.DEBUG2)
            else:
                cr = self.du.run_cmd(hostname, ps_cmd + ['-e'],
                                     level=logging.DEBUG2)
        elif pid is not None:
            cr = self.du.run_cmd(hostname, ps_cmd + ['-p', pid],
                                 level=logging.DEBUG2)
        else:
            return

        if cr['rc'] == 0 and cr['out']:
            for proc in cr['out']:
                _pi = None
                try:
                    _s = proc.split()
                    p = _s[0]
                    rss = _s[1]
                    vsz = _s[2]
                    pcpu = _s[3]
                    pmem = _s[4]
                    size = _s[5]
                    cputime = _s[6]
                    command = " ".join(_s[4:])
                except:
                    continue

                if ((pid is not None and p == str(pid)) or
                    (name is not None and (
                        (regexp and re.search(name, command) is not None) or
                        (not regexp and name in command)))):
                    _pi = ProcInfo(name=command)
                    _pi.pid = p
                    _pi.rss = rss
                    _pi.vsz = vsz
                    _pi.pcpu = pcpu
                    _pi.pmem = pmem
                    _pi.size = size
                    _pi.cputime = cputime
                    _pi.command = command
                    _pi.open_fds = self._get_proc_open_fds(hostname, _pi.pid)
                if _pi is not None:
                    if command in self.processes:
                        self.processes[command].append(_pi)
                    else:
                        self.processes[command] = [_pi]
        return self.processes

    def get_proc_info(self, hostname=None, name=None, pid=None, regexp=False):
        """
        Return process information from a process name, or pid,
        on a given host

        :param hostname: The hostname on which to query the process
                         info. On Windows,only localhost is queried.
        :type hostname: str or none
        :param name: The name of the process to query.
        :type name: str or None
        :param pid: The pid of the process to query
        :type pid: int or None
        :param regexp: Match processes by regular expression. Defaults
                       to True. Does not apply to matching by PID.
        :type regexp: bool
        :returns: A list of ProcInfo objects, one for each matching
                  process.

        .. note:: If both, name and pid, are specified, name is used.
        """
        self._init_processes()
        return self._get_proc_info_unix(hostname, name, pid, regexp)

    def get_proc_state(self, hostname=None, pid=None):
        """
        :returns: PID's process state on host hostname

        On error the empty string is returned.
        """
        if not self.du.is_localhost(hostname):
            platform = self.du.get_platform(hostname)
        else:
            platform = sys.platform

        try:
            if platform.startswith('linux'):
                cmd = ['ps', '-o', 'stat', '-p', str(pid), '--no-heading']
                rv = self.du.run_cmd(hostname, cmd, level=logging.DEBUG2)
                return rv['out'][0][0]
        except:
            self.logger.error('Error getting process state for pid ' + pid)
            return ''

    def get_proc_children(self, hostname=None, ppid=None):
        """
        :returns: A list of children PIDs associated to ``PPID`` on
                  host hostname.

        On error, an empty list is returned.
        """
        try:
            if not isinstance(ppid, str):
                ppid = str(ppid)

            if int(ppid) <= 0:
                pass
                #raise "Error here"

            if not self.du.is_localhost(hostname):
                platform = self.du.get_platform(hostname)
            else:
                platform = sys.platform

            childlist = []

            if platform.startswith('linux'):
                cmd = ['ps', '-o', 'pid', '--ppid:%s' % ppid, '--no-heading']
                rv = self.du.run_cmd(hostname, cmd)
                children = rv['out'][:-1]
            else:
                children = []

            for child in children:
                child = child.strip()
                if child != '':
                    childlist.append(child)
                    childlist.extend(self.get_proc_children(hostname, child))

            return childlist
        except:
            self.logger.error('Error getting children processes of parent ' +
                              ppid)
            return []


class ProcInfo(object):

    """
    Process information reports ``PID``, ``RSS``, ``VSZ``, Command
    and Time at which process information is collected
    """

    def __init__(self, name=None, pid=None):
        self.name = name
        self.pid = pid
        self.rss = None
        self.vsz = None
        self.pcpu = None
        self.pmem = None
        self.size = None
        self.cputime = None
        self.open_fds = None
        self.time = time.time()
        self.command = None

    def __str__(self):
        return "%s pid: %s rss: %s vsz: %s pcpu: %s pmem: %s \
               size: %s cputime: %s command: %s open_fds: %s" % \
               (self.name, str(self.pid), str(self.rss), str(self.vsz),
                str(self.pcpu), str(self.pmem), str(self.size),
                str(self.cputime), self.command, self.open_fds)


class ProcMonitor(threading.Thread):

    """
    A background process monitoring tool
    """
    du = DshUtils()
    logger = logging.getLogger(__name__)

    def __init__(self, name=None, regexp=False, frequency=60):
        threading.Thread.__init__(self)
        self.name = name
        self.frequency = frequency
        self.regexp = regexp
        self.stop_thread = threading.Event()
        self._pu = ProcUtils()
        #self._go = True
        self.db_proc_info = []
        self.sysstat = {}

    def set_frequency(self, value=60):
        """
        Set the frequency

        :param value: Frequency value
        :type value: int
        """
        self.logger.debug('procmonitor: set frequency to ' + str(value))
        self.frequency = value

    def get_system_stats(self, nw_protocols=['TCP']):
        """
        Run system monitoring
        """
        cmd = 'sar -rSub -n %s 1 1' % ','.join(nw_protocols)
        rv = self.du.run_cmd(cmd=cmd, as_script=True)
        op = rv['out'][2:]
        op = [i.split()[2:] for i in op if
              (i and not i.startswith('Average'))]
        op = [list(zip(op[i], op[i + 1])) for i in range(0, len(op), 2)]
        for i in op:
            self.sysstat.update(dict(i))

    def run(self):
        """
        Run the process monitoring
        """
        while not self.stop_thread.is_set():
            timenow = int(time.time())
            self._pu.get_proc_info(name=self.name, regexp=self.regexp)
            for _p in self._pu.processes.values():
                for _per_proc in _p:
                    if bool(re.search("^((?!benchpress).)*$", _per_proc.name)):
                        _to_db = {}
                        _to_db['time'] = time.ctime(timenow)
                        _to_db['rss'] = _per_proc.rss
                        _to_db['vsz'] = _per_proc.vsz
                        _to_db['pcpu'] = _per_proc.pcpu
                        _to_db['pmem'] = _per_proc.pmem
                        _to_db['size'] = _per_proc.size
                        _to_db['cputime'] = _per_proc.cputime
                        _to_db['open_fds'] = _per_proc.open_fds
                        _to_db['name'] = _per_proc.name
                        self.db_proc_info.append(_to_db)
            self.get_system_stats()
            _sys_info = {}
            _sys_info['name'] = "System"
            _sys_info['time'] = time.ctime(timenow)
            _sys_info['sysload'] = os.getloadavg()[0]
            _sys_info['pmemused'] = self.sysstat['%memused']
            _sys_info['psystem'] = self.sysstat['%system']
            _sys_info['pswpused'] = self.sysstat['%swpused']
            _sys_info['rtps'] = self.sysstat['rtps']
            _sys_info['wtps'] = self.sysstat['wtps']
            self.db_proc_info.append(_sys_info)
            time.sleep(self.frequency)

    def stop(self):
        """
        Stop the process monitoring
        """
        #self._go = False
        self.stop_thread.set()
        self.join()

if __name__ == '__main__':
    pm = ProcMonitor(name='.*pbs_server.*|.*pbs_sched.*', regexp=True,
                     frequency=1)
    pm.start()
    time.sleep(4)
    pm.stop()
