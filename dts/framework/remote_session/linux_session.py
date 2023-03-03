# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

from framework.testbed_model import LogicalCore

from .posix_session import PosixSession


class LinuxSession(PosixSession):
    """
    The implementation of non-Posix compliant parts of Linux remote sessions.
    """

    def get_remote_cpus(self, use_first_core: bool) -> list[LogicalCore]:
        cpu_info = self.remote_session.send_command(
            "lscpu -p=CPU,CORE,SOCKET,NODE|grep -v \\#"
        ).stdout
        lcores = []
        for cpu_line in cpu_info.splitlines():
            lcore, core, socket, node = map(int, cpu_line.split(","))
            if core == 0 and socket == 0 and not use_first_core:
                self._logger.info("Not using the first physical core.")
                continue
            lcores.append(LogicalCore(lcore, core, socket, node))
        return lcores

    def get_dpdk_file_prefix(self, dpdk_prefix) -> str:
        return dpdk_prefix
