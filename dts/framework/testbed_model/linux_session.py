# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

"""Linux OS translator.

Translate OS-unaware calls into Linux calls/utilities. Most of Linux distributions are mostly
compliant with POSIX standards, so this module only implements the parts that aren't.
This intermediate module implements the common parts of mostly POSIX compliant distributions.
"""

import json
from ipaddress import IPv4Interface, IPv6Interface
from typing import TypedDict, Union

from typing_extensions import NotRequired

from framework.exception import RemoteCommandExecutionError
from framework.utils import expand_range

from .cpu import LogicalCore
from .port import Port
from .posix_session import PosixSession


class LshwConfigurationOutput(TypedDict):
    """The relevant parts of ``lshw``'s ``configuration`` section."""

    #:
    link: str


class LshwOutput(TypedDict):
    """A model of the relevant information from ``lshw``'s json output.

    Example:
        ::

            {
            ...
            "businfo" : "pci@0000:08:00.0",
            "logicalname" : "enp8s0",
            "version" : "00",
            "serial" : "52:54:00:59:e1:ac",
            ...
            "configuration" : {
              ...
              "link" : "yes",
              ...
            },
            ...
    """

    #:
    businfo: str
    #:
    logicalname: NotRequired[str]
    #:
    serial: NotRequired[str]
    #:
    configuration: LshwConfigurationOutput


class LinuxSession(PosixSession):
    """The implementation of non-Posix compliant parts of Linux."""

    @staticmethod
    def _get_privileged_command(command: str) -> str:
        return f"sudo -- sh -c '{command}'"

    def get_remote_cpus(self, use_first_core: bool) -> list[LogicalCore]:
        """Overrides :meth:`~.os_session.OSSession.get_remote_cpus`."""
        cpu_info = self.send_command("lscpu -p=CPU,CORE,SOCKET,NODE|grep -v \\#").stdout
        lcores = []
        for cpu_line in cpu_info.splitlines():
            lcore, core, socket, node = map(int, cpu_line.split(","))
            if core == 0 and socket == 0 and not use_first_core:
                self._logger.info("Not using the first physical core.")
                continue
            lcores.append(LogicalCore(lcore, core, socket, node))
        return lcores

    def get_dpdk_file_prefix(self, dpdk_prefix: str) -> str:
        """Overrides :meth:`~.os_session.OSSession.get_dpdk_file_prefix`."""
        return dpdk_prefix

    def setup_hugepages(self, hugepage_count: int, force_first_numa: bool) -> None:
        """Overrides :meth:`~.os_session.OSSession.setup_hugepages`."""
        self._logger.info("Getting Hugepage information.")
        hugepage_size = self._get_hugepage_size()
        hugepages_total = self._get_hugepages_total()
        self._numa_nodes = self._get_numa_nodes()

        if force_first_numa or hugepages_total != hugepage_count:
            # when forcing numa, we need to clear existing hugepages regardless
            # of size, so they can be moved to the first numa node
            self._configure_huge_pages(hugepage_count, hugepage_size, force_first_numa)
        else:
            self._logger.info("Hugepages already configured.")
        self._mount_huge_pages()

    def _get_hugepage_size(self) -> int:
        hugepage_size = self.send_command("awk '/Hugepagesize/ {print $2}' /proc/meminfo").stdout
        return int(hugepage_size)

    def _get_hugepages_total(self) -> int:
        hugepages_total = self.send_command(
            "awk '/HugePages_Total/ { print $2 }' /proc/meminfo"
        ).stdout
        return int(hugepages_total)

    def _get_numa_nodes(self) -> list[int]:
        try:
            numa_count = self.send_command(
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
        self.send_command(f"umount $({hugapge_fs_cmd})")
        result = self.send_command(hugapge_fs_cmd)
        if result.stdout == "":
            remote_mount_path = "/mnt/huge"
            self.send_command(f"mkdir -p {remote_mount_path}")
            self.send_command(f"mount -t hugetlbfs nodev {remote_mount_path}")

    def _supports_numa(self) -> bool:
        # the system supports numa if self._numa_nodes is non-empty and there are more
        # than one numa node (in the latter case it may actually support numa, but
        # there's no reason to do any numa specific configuration)
        return len(self._numa_nodes) > 1

    def _configure_huge_pages(self, amount: int, size: int, force_first_numa: bool) -> None:
        self._logger.info("Configuring Hugepages.")
        hugepage_config_path = f"/sys/kernel/mm/hugepages/hugepages-{size}kB/nr_hugepages"
        if force_first_numa and self._supports_numa():
            # clear non-numa hugepages
            self.send_command(f"echo 0 | tee {hugepage_config_path}", privileged=True)
            hugepage_config_path = (
                f"/sys/devices/system/node/node{self._numa_nodes[0]}/hugepages"
                f"/hugepages-{size}kB/nr_hugepages"
            )

        self.send_command(f"echo {amount} | tee {hugepage_config_path}", privileged=True)

    def update_ports(self, ports: list[Port]) -> None:
        """Overrides :meth:`~.os_session.OSSession.update_ports`."""
        self._logger.debug("Gathering port info.")
        for port in ports:
            assert port.node == self.name, "Attempted to gather port info on the wrong node"

        port_info_list = self._get_lshw_info()
        for port in ports:
            for port_info in port_info_list:
                if f"pci@{port.pci}" == port_info.get("businfo"):
                    self._update_port_attr(port, port_info.get("logicalname"), "logical_name")
                    self._update_port_attr(port, port_info.get("serial"), "mac_address")
                    port_info_list.remove(port_info)
                    break
            else:
                self._logger.warning(f"No port at pci address {port.pci} found.")

    def _get_lshw_info(self) -> list[LshwOutput]:
        output = self.send_command("lshw -quiet -json -C network", verify=True)
        return json.loads(output.stdout)

    def _update_port_attr(self, port: Port, attr_value: str | None, attr_name: str) -> None:
        if attr_value:
            setattr(port, attr_name, attr_value)
            self._logger.debug(f"Found '{attr_name}' of port {port.pci}: '{attr_value}'.")
        else:
            self._logger.warning(
                f"Attempted to get '{attr_name}' of port {port.pci}, but it doesn't exist."
            )

    def configure_port_state(self, port: Port, enable: bool) -> None:
        """Overrides :meth:`~.os_session.OSSession.configure_port_state`."""
        state = "up" if enable else "down"
        self.send_command(f"ip link set dev {port.logical_name} {state}", privileged=True)

    def configure_port_ip_address(
        self,
        address: Union[IPv4Interface, IPv6Interface],
        port: Port,
        delete: bool,
    ) -> None:
        """Overrides :meth:`~.os_session.OSSession.configure_port_ip_address`."""
        command = "del" if delete else "add"
        self.send_command(
            f"ip address {command} {address} dev {port.logical_name}",
            privileged=True,
            verify=True,
        )

    def configure_ipv4_forwarding(self, enable: bool) -> None:
        """Overrides :meth:`~.os_session.OSSession.configure_ipv4_forwarding`."""
        state = 1 if enable else 0
        self.send_command(f"sysctl -w net.ipv4.ip_forward={state}", privileged=True)
