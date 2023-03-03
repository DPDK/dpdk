# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

from framework.exception import RemoteCommandExecutionError
from framework.testbed_model import LogicalCore
from framework.utils import expand_range

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

    def setup_hugepages(self, hugepage_amount: int, force_first_numa: bool) -> None:
        self._logger.info("Getting Hugepage information.")
        hugepage_size = self._get_hugepage_size()
        hugepages_total = self._get_hugepages_total()
        self._numa_nodes = self._get_numa_nodes()

        if force_first_numa or hugepages_total != hugepage_amount:
            # when forcing numa, we need to clear existing hugepages regardless
            # of size, so they can be moved to the first numa node
            self._configure_huge_pages(hugepage_amount, hugepage_size, force_first_numa)
        else:
            self._logger.info("Hugepages already configured.")
        self._mount_huge_pages()

    def _get_hugepage_size(self) -> int:
        hugepage_size = self.remote_session.send_command(
            "awk '/Hugepagesize/ {print $2}' /proc/meminfo"
        ).stdout
        return int(hugepage_size)

    def _get_hugepages_total(self) -> int:
        hugepages_total = self.remote_session.send_command(
            "awk '/HugePages_Total/ { print $2 }' /proc/meminfo"
        ).stdout
        return int(hugepages_total)

    def _get_numa_nodes(self) -> list[int]:
        try:
            numa_count = self.remote_session.send_command(
                "cat /sys/devices/system/node/online", verify=True
            ).stdout
            numa_range = expand_range(numa_count)
        except RemoteCommandExecutionError:
            # the file doesn't exist, meaning the node doesn't support numa
            numa_range = []
        return numa_range

    def _mount_huge_pages(self) -> None:
        self._logger.info("Re-mounting Hugepages.")
        hugapge_fs_cmd = "awk '/hugetlbfs/ { print $2 }' /proc/mounts"
        self.remote_session.send_command(f"umount $({hugapge_fs_cmd})")
        result = self.remote_session.send_command(hugapge_fs_cmd)
        if result.stdout == "":
            remote_mount_path = "/mnt/huge"
            self.remote_session.send_command(f"mkdir -p {remote_mount_path}")
            self.remote_session.send_command(
                f"mount -t hugetlbfs nodev {remote_mount_path}"
            )

    def _supports_numa(self) -> bool:
        # the system supports numa if self._numa_nodes is non-empty and there are more
        # than one numa node (in the latter case it may actually support numa, but
        # there's no reason to do any numa specific configuration)
        return len(self._numa_nodes) > 1

    def _configure_huge_pages(
        self, amount: int, size: int, force_first_numa: bool
    ) -> None:
        self._logger.info("Configuring Hugepages.")
        hugepage_config_path = (
            f"/sys/kernel/mm/hugepages/hugepages-{size}kB/nr_hugepages"
        )
        if force_first_numa and self._supports_numa():
            # clear non-numa hugepages
            self.remote_session.send_command(
                f"echo 0 | sudo tee {hugepage_config_path}"
            )
            hugepage_config_path = (
                f"/sys/devices/system/node/node{self._numa_nodes[0]}/hugepages"
                f"/hugepages-{size}kB/nr_hugepages"
            )

        self.remote_session.send_command(
            f"echo {amount} | sudo tee {hugepage_config_path}"
        )
