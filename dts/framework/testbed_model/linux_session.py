# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

"""Linux OS translator.

Translate OS-unaware calls into Linux calls/utilities. Most of Linux distributions are mostly
compliant with POSIX standards, so this module only implements the parts that aren't.
This intermediate module implements the common parts of mostly POSIX compliant distributions.
"""

import json
from collections.abc import Iterable
from functools import cached_property
from pathlib import PurePath
from typing import TypedDict

from typing_extensions import NotRequired

from framework.exception import ConfigurationError, InternalError, RemoteCommandExecutionError
from framework.testbed_model.os_session import PortInfo
from framework.utils import expand_range

from .cpu import LogicalCore
from .port import Port
from .posix_session import PosixSession


class LshwConfigurationOutput(TypedDict):
    """The relevant parts of ``lshw``'s ``configuration`` section."""

    #:
    driver: str
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
        command = command.replace(r"'", r"\'")
        return f"sudo -- sh -c '{command}'"

    def get_remote_cpus(self) -> list[LogicalCore]:
        """Overrides :meth:`~.os_session.OSSession.get_remote_cpus`."""
        cpu_info = self.send_command("lscpu -p=CPU,CORE,SOCKET,NODE|grep -v \\#").stdout
        lcores = []
        for cpu_line in cpu_info.splitlines():
            lcore, core, socket, node = map(int, cpu_line.split(","))
            lcores.append(LogicalCore(lcore, core, socket, node))
        return lcores

    def get_dpdk_file_prefix(self, dpdk_prefix: str) -> str:
        """Overrides :meth:`~.os_session.OSSession.get_dpdk_file_prefix`."""
        return dpdk_prefix

    def setup_hugepages(self, number_of: int, hugepage_size: int, force_first_numa: bool) -> None:
        """Overrides :meth:`~.os_session.OSSession.setup_hugepages`.

        Raises:
            ConfigurationError: If the given `hugepage_size` is not supported by the OS.
        """
        self._logger.info("Getting Hugepage information.")
        if (
            f"hugepages-{hugepage_size}kB"
            not in self.send_command("ls /sys/kernel/mm/hugepages").stdout
        ):
            raise ConfigurationError("hugepage size not supported by operating system")
        hugepages_total = self._get_hugepages_total(hugepage_size)
        self._numa_nodes = self._get_numa_nodes()

        if force_first_numa or hugepages_total < number_of:
            # when forcing numa, we need to clear existing hugepages regardless
            # of size, so they can be moved to the first numa node
            self._configure_huge_pages(number_of, hugepage_size, force_first_numa)
        else:
            self._logger.info("Hugepages already configured.")
        self._mount_huge_pages()

    def _get_hugepages_total(self, hugepage_size: int) -> int:
        hugepages_total = self.send_command(
            f"cat /sys/kernel/mm/hugepages/hugepages-{hugepage_size}kB/nr_hugepages"
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
        self.send_command(f"umount $({hugapge_fs_cmd})", privileged=True)
        result = self.send_command(hugapge_fs_cmd)
        if result.stdout == "":
            remote_mount_path = "/mnt/huge"
            self.send_command(f"mkdir -p {remote_mount_path}", privileged=True)
            self.send_command(f"mount -t hugetlbfs nodev {remote_mount_path}", privileged=True)

    def _supports_numa(self) -> bool:
        # the system supports numa if self._numa_nodes is non-empty and there are more
        # than one numa node (in the latter case it may actually support numa, but
        # there's no reason to do any numa specific configuration)
        return len(self._numa_nodes) > 1

    def _configure_huge_pages(self, number_of: int, size: int, force_first_numa: bool) -> None:
        self._logger.info("Configuring Hugepages.")
        hugepage_config_path = f"/sys/kernel/mm/hugepages/hugepages-{size}kB/nr_hugepages"
        if force_first_numa and self._supports_numa():
            # clear non-numa hugepages
            self.send_command(f"echo 0 | tee {hugepage_config_path}", privileged=True)
            hugepage_config_path = (
                f"/sys/devices/system/node/node{self._numa_nodes[0]}/hugepages"
                f"/hugepages-{size}kB/nr_hugepages"
            )

        self.send_command(f"echo {number_of} | tee {hugepage_config_path}", privileged=True)

    def get_port_info(self, pci_address: str) -> PortInfo:
        """Overrides :meth:`~.os_session.OSSession.get_port_info`.

        Raises:
            ConfigurationError: If the port could not be found.
        """
        bus_info = f"pci@{pci_address}"
        port = next(port for port in self._lshw_net_info if port.get("businfo") == bus_info)
        if port is None:
            raise ConfigurationError(f"Port {pci_address} could not be found on the node.")

        logical_name = port.get("logicalname", "")
        mac_address = port.get("serial", "")

        configuration = port.get("configuration", {})
        driver = configuration.get("driver", "")
        is_link_up = configuration.get("link", "down") == "up"

        return PortInfo(mac_address, logical_name, driver, is_link_up)

    def bind_ports_to_driver(self, ports: list[Port], driver_name: str) -> None:
        """Overrides :meth:`~.os_session.OSSession.bind_ports_to_driver`.

        The :attr:`~.devbind_script_path` property must be setup in order to call this method.
        """
        ports_pci_addrs = " ".join(port.pci for port in ports)

        self.send_command(
            f"{self.devbind_script_path} -b {driver_name} --force {ports_pci_addrs}",
            privileged=True,
            verify=True,
        )

        del self._lshw_net_info

    def bring_up_link(self, ports: Iterable[Port]) -> None:
        """Overrides :meth:`~.os_session.OSSession.bring_up_link`."""
        for port in ports:
            self.send_command(
                f"ip link set dev {port.logical_name} up", privileged=True, verify=True
            )

        del self._lshw_net_info

    @cached_property
    def devbind_script_path(self) -> PurePath:
        """The path to the dpdk-devbind.py script on the node.

        Needs to be manually assigned first in order to be used.

        Raises:
            InternalError: If accessed before environment setup.
        """
        raise InternalError("Accessed devbind script path before setup.")

    @cached_property
    def _lshw_net_info(self) -> list[LshwOutput]:
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

    def configure_port_mtu(self, mtu: int, port: Port) -> None:
        """Overrides :meth:`~.os_session.OSSession.configure_port_mtu`."""
        self.send_command(
            f"ip link set dev {port.logical_name} mtu {mtu}",
            privileged=True,
            verify=True,
        )

    def configure_ipv4_forwarding(self, enable: bool) -> None:
        """Overrides :meth:`~.os_session.OSSession.configure_ipv4_forwarding`."""
        state = 1 if enable else 0
        self.send_command(f"sysctl -w net.ipv4.ip_forward={state}", privileged=True)
