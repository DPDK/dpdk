# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Testpmd interactive shell.

Typical usage example in a TestSuite::

    testpmd_shell = TestPmdShell(self.sut_node)
    devices = testpmd_shell.get_devices()
    for device in devices:
        print(device)
    testpmd_shell.close()
"""

import re
import time
from dataclasses import dataclass, field
from enum import Flag, auto
from pathlib import PurePath
from typing import ClassVar

from typing_extensions import Self, Unpack

from framework.exception import InteractiveCommandExecutionError
from framework.params.testpmd import SimpleForwardingModes, TestPmdParams
from framework.params.types import TestPmdParamsDict
from framework.parser import ParserFn, TextParser
from framework.remote_session.dpdk_shell import DPDKShell
from framework.settings import SETTINGS
from framework.testbed_model.cpu import LogicalCoreCount, LogicalCoreList
from framework.testbed_model.sut_node import SutNode
from framework.utils import StrEnum


class TestPmdDevice:
    """The data of a device that testpmd can recognize.

    Attributes:
        pci_address: The PCI address of the device.
    """

    pci_address: str

    def __init__(self, pci_address_line: str):
        """Initialize the device from the testpmd output line string.

        Args:
            pci_address_line: A line of testpmd output that contains a device.
        """
        self.pci_address = pci_address_line.strip().split(": ")[1].strip()

    def __str__(self) -> str:
        """The PCI address captures what the device is."""
        return self.pci_address


class VLANOffloadFlag(Flag):
    """Flag representing the VLAN offload settings of a NIC port."""

    #:
    STRIP = auto()
    #:
    FILTER = auto()
    #:
    EXTEND = auto()
    #:
    QINQ_STRIP = auto()

    @classmethod
    def from_str_dict(cls, d):
        """Makes an instance from a dict containing the flag member names with an "on" value.

        Args:
            d: A dictionary containing the flag members as keys and any string value.

        Returns:
            A new instance of the flag.
        """
        flag = cls(0)
        for name in cls.__members__:
            if d.get(name) == "on":
                flag |= cls[name]
        return flag

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find(
                r"VLAN offload:\s+"
                r"strip (?P<STRIP>on|off), "
                r"filter (?P<FILTER>on|off), "
                r"extend (?P<EXTEND>on|off), "
                r"qinq strip (?P<QINQ_STRIP>on|off)$",
                re.MULTILINE,
                named=True,
            ),
            cls.from_str_dict,
        )


class RSSOffloadTypesFlag(Flag):
    """Flag representing the RSS offload flow types supported by the NIC port."""

    #:
    ipv4 = auto()
    #:
    ipv4_frag = auto()
    #:
    ipv4_tcp = auto()
    #:
    ipv4_udp = auto()
    #:
    ipv4_sctp = auto()
    #:
    ipv4_other = auto()
    #:
    ipv6 = auto()
    #:
    ipv6_frag = auto()
    #:
    ipv6_tcp = auto()
    #:
    ipv6_udp = auto()
    #:
    ipv6_sctp = auto()
    #:
    ipv6_other = auto()
    #:
    l2_payload = auto()
    #:
    ipv6_ex = auto()
    #:
    ipv6_tcp_ex = auto()
    #:
    ipv6_udp_ex = auto()
    #:
    port = auto()
    #:
    vxlan = auto()
    #:
    geneve = auto()
    #:
    nvgre = auto()
    #:
    user_defined_22 = auto()
    #:
    gtpu = auto()
    #:
    eth = auto()
    #:
    s_vlan = auto()
    #:
    c_vlan = auto()
    #:
    esp = auto()
    #:
    ah = auto()
    #:
    l2tpv3 = auto()
    #:
    pfcp = auto()
    #:
    pppoe = auto()
    #:
    ecpri = auto()
    #:
    mpls = auto()
    #:
    ipv4_chksum = auto()
    #:
    l4_chksum = auto()
    #:
    l2tpv2 = auto()
    #:
    ipv6_flow_label = auto()
    #:
    user_defined_38 = auto()
    #:
    user_defined_39 = auto()
    #:
    user_defined_40 = auto()
    #:
    user_defined_41 = auto()
    #:
    user_defined_42 = auto()
    #:
    user_defined_43 = auto()
    #:
    user_defined_44 = auto()
    #:
    user_defined_45 = auto()
    #:
    user_defined_46 = auto()
    #:
    user_defined_47 = auto()
    #:
    user_defined_48 = auto()
    #:
    user_defined_49 = auto()
    #:
    user_defined_50 = auto()
    #:
    user_defined_51 = auto()
    #:
    l3_pre96 = auto()
    #:
    l3_pre64 = auto()
    #:
    l3_pre56 = auto()
    #:
    l3_pre48 = auto()
    #:
    l3_pre40 = auto()
    #:
    l3_pre32 = auto()
    #:
    l2_dst_only = auto()
    #:
    l2_src_only = auto()
    #:
    l4_dst_only = auto()
    #:
    l4_src_only = auto()
    #:
    l3_dst_only = auto()
    #:
    l3_src_only = auto()

    #:
    ip = ipv4 | ipv4_frag | ipv4_other | ipv6 | ipv6_frag | ipv6_other | ipv6_ex
    #:
    udp = ipv4_udp | ipv6_udp | ipv6_udp_ex
    #:
    tcp = ipv4_tcp | ipv6_tcp | ipv6_tcp_ex
    #:
    sctp = ipv4_sctp | ipv6_sctp
    #:
    tunnel = vxlan | geneve | nvgre
    #:
    vlan = s_vlan | c_vlan
    #:
    all = (
        eth
        | vlan
        | ip
        | tcp
        | udp
        | sctp
        | l2_payload
        | l2tpv3
        | esp
        | ah
        | pfcp
        | gtpu
        | ecpri
        | mpls
        | l2tpv2
    )

    @classmethod
    def from_list_string(cls, names: str) -> Self:
        """Makes a flag from a whitespace-separated list of names.

        Args:
            names: a whitespace-separated list containing the members of this flag.

        Returns:
            An instance of this flag.
        """
        flag = cls(0)
        for name in names.split():
            flag |= cls.from_str(name)
        return flag

    @classmethod
    def from_str(cls, name: str) -> Self:
        """Makes a flag matching the supplied name.

        Args:
            name: a valid member of this flag in text
        Returns:
            An instance of this flag.
        """
        member_name = name.strip().replace("-", "_")
        return cls[member_name]

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find(r"Supported RSS offload flow types:((?:\r?\n?  \S+)+)", re.MULTILINE),
            RSSOffloadTypesFlag.from_list_string,
        )


class DeviceCapabilitiesFlag(Flag):
    """Flag representing the device capabilities."""

    #: Device supports Rx queue setup after device started.
    RUNTIME_RX_QUEUE_SETUP = auto()
    #: Device supports Tx queue setup after device started.
    RUNTIME_TX_QUEUE_SETUP = auto()
    #: Device supports shared Rx queue among ports within Rx domain and switch domain.
    RXQ_SHARE = auto()
    #: Device supports keeping flow rules across restart.
    FLOW_RULE_KEEP = auto()
    #: Device supports keeping shared flow objects across restart.
    FLOW_SHARED_OBJECT_KEEP = auto()

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this flag from text.
        """
        return TextParser.wrap(
            TextParser.find_int(r"Device capabilities: (0x[A-Fa-f\d]+)"),
            cls,
        )


class DeviceErrorHandlingMode(StrEnum):
    """Enum representing the device error handling mode."""

    #:
    none = auto()
    #:
    passive = auto()
    #:
    proactive = auto()
    #:
    unknown = auto()

    @classmethod
    def make_parser(cls) -> ParserFn:
        """Makes a parser function.

        Returns:
            ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a
                parser function that makes an instance of this enum from text.
        """
        return TextParser.wrap(TextParser.find(r"Device error handling mode: (\w+)"), cls)


def make_device_private_info_parser() -> ParserFn:
    """Device private information parser.

    Ensures that we are not parsing invalid device private info output.

    Returns:
        ParserFn: A dictionary for the `dataclasses.field` metadata argument containing a parser
            function that parses the device private info from the TestPmd port info output.
    """

    def _validate(info: str):
        info = info.strip()
        if info == "none" or info.startswith("Invalid file") or info.startswith("Failed to dump"):
            return None
        return info

    return TextParser.wrap(TextParser.find(r"Device private info:\s+([\s\S]+)"), _validate)


@dataclass
class TestPmdPort(TextParser):
    """Dataclass representing the result of testpmd's ``show port info`` command."""

    #:
    id: int = field(metadata=TextParser.find_int(r"Infos for port (\d+)\b"))
    #:
    device_name: str = field(metadata=TextParser.find(r"Device name: ([^\r\n]+)"))
    #:
    driver_name: str = field(metadata=TextParser.find(r"Driver name: ([^\r\n]+)"))
    #:
    socket_id: int = field(metadata=TextParser.find_int(r"Connect to socket: (\d+)"))
    #:
    is_link_up: bool = field(metadata=TextParser.find("Link status: up"))
    #:
    link_speed: str = field(metadata=TextParser.find(r"Link speed: ([^\r\n]+)"))
    #:
    is_link_full_duplex: bool = field(metadata=TextParser.find("Link duplex: full-duplex"))
    #:
    is_link_autonegotiated: bool = field(metadata=TextParser.find("Autoneg status: On"))
    #:
    is_promiscuous_mode_enabled: bool = field(metadata=TextParser.find("Promiscuous mode: enabled"))
    #:
    is_allmulticast_mode_enabled: bool = field(
        metadata=TextParser.find("Allmulticast mode: enabled")
    )
    #: Maximum number of MAC addresses
    max_mac_addresses_num: int = field(
        metadata=TextParser.find_int(r"Maximum number of MAC addresses: (\d+)")
    )
    #: Maximum configurable length of RX packet
    max_hash_mac_addresses_num: int = field(
        metadata=TextParser.find_int(r"Maximum number of MAC addresses of hash filtering: (\d+)")
    )
    #: Minimum size of RX buffer
    min_rx_bufsize: int = field(metadata=TextParser.find_int(r"Minimum size of RX buffer: (\d+)"))
    #: Maximum configurable length of RX packet
    max_rx_packet_length: int = field(
        metadata=TextParser.find_int(r"Maximum configurable length of RX packet: (\d+)")
    )
    #: Maximum configurable size of LRO aggregated packet
    max_lro_packet_size: int = field(
        metadata=TextParser.find_int(r"Maximum configurable size of LRO aggregated packet: (\d+)")
    )

    #: Current number of RX queues
    rx_queues_num: int = field(metadata=TextParser.find_int(r"Current number of RX queues: (\d+)"))
    #: Max possible RX queues
    max_rx_queues_num: int = field(metadata=TextParser.find_int(r"Max possible RX queues: (\d+)"))
    #: Max possible number of RXDs per queue
    max_queue_rxd_num: int = field(
        metadata=TextParser.find_int(r"Max possible number of RXDs per queue: (\d+)")
    )
    #: Min possible number of RXDs per queue
    min_queue_rxd_num: int = field(
        metadata=TextParser.find_int(r"Min possible number of RXDs per queue: (\d+)")
    )
    #: RXDs number alignment
    rxd_alignment_num: int = field(metadata=TextParser.find_int(r"RXDs number alignment: (\d+)"))

    #: Current number of TX queues
    tx_queues_num: int = field(metadata=TextParser.find_int(r"Current number of TX queues: (\d+)"))
    #: Max possible TX queues
    max_tx_queues_num: int = field(metadata=TextParser.find_int(r"Max possible TX queues: (\d+)"))
    #: Max possible number of TXDs per queue
    max_queue_txd_num: int = field(
        metadata=TextParser.find_int(r"Max possible number of TXDs per queue: (\d+)")
    )
    #: Min possible number of TXDs per queue
    min_queue_txd_num: int = field(
        metadata=TextParser.find_int(r"Min possible number of TXDs per queue: (\d+)")
    )
    #: TXDs number alignment
    txd_alignment_num: int = field(metadata=TextParser.find_int(r"TXDs number alignment: (\d+)"))
    #: Max segment number per packet
    max_packet_segment_num: int = field(
        metadata=TextParser.find_int(r"Max segment number per packet: (\d+)")
    )
    #: Max segment number per MTU/TSO
    max_mtu_segment_num: int = field(
        metadata=TextParser.find_int(r"Max segment number per MTU\/TSO: (\d+)")
    )

    #:
    device_capabilities: DeviceCapabilitiesFlag = field(
        metadata=DeviceCapabilitiesFlag.make_parser(),
    )
    #:
    device_error_handling_mode: DeviceErrorHandlingMode = field(
        metadata=DeviceErrorHandlingMode.make_parser()
    )
    #:
    device_private_info: str | None = field(
        default=None,
        metadata=make_device_private_info_parser(),
    )

    #:
    hash_key_size: int | None = field(
        default=None, metadata=TextParser.find_int(r"Hash key size in bytes: (\d+)")
    )
    #:
    redirection_table_size: int | None = field(
        default=None, metadata=TextParser.find_int(r"Redirection table size: (\d+)")
    )
    #:
    supported_rss_offload_flow_types: RSSOffloadTypesFlag = field(
        default=RSSOffloadTypesFlag(0), metadata=RSSOffloadTypesFlag.make_parser()
    )

    #:
    mac_address: str | None = field(
        default=None, metadata=TextParser.find(r"MAC address: ([A-Fa-f0-9:]+)")
    )
    #:
    fw_version: str | None = field(
        default=None, metadata=TextParser.find(r"Firmware-version: ([^\r\n]+)")
    )
    #:
    dev_args: str | None = field(default=None, metadata=TextParser.find(r"Devargs: ([^\r\n]+)"))
    #: Socket id of the memory allocation
    mem_alloc_socket_id: int | None = field(
        default=None,
        metadata=TextParser.find_int(r"memory allocation on the socket: (\d+)"),
    )
    #:
    mtu: int | None = field(default=None, metadata=TextParser.find_int(r"MTU: (\d+)"))

    #:
    vlan_offload: VLANOffloadFlag | None = field(
        default=None,
        metadata=VLANOffloadFlag.make_parser(),
    )

    #: Maximum size of RX buffer
    max_rx_bufsize: int | None = field(
        default=None, metadata=TextParser.find_int(r"Maximum size of RX buffer: (\d+)")
    )
    #: Maximum number of VFs
    max_vfs_num: int | None = field(
        default=None, metadata=TextParser.find_int(r"Maximum number of VFs: (\d+)")
    )
    #: Maximum number of VMDq pools
    max_vmdq_pools_num: int | None = field(
        default=None, metadata=TextParser.find_int(r"Maximum number of VMDq pools: (\d+)")
    )

    #:
    switch_name: str | None = field(
        default=None, metadata=TextParser.find(r"Switch name: ([\r\n]+)")
    )
    #:
    switch_domain_id: int | None = field(
        default=None, metadata=TextParser.find_int(r"Switch domain Id: (\d+)")
    )
    #:
    switch_port_id: int | None = field(
        default=None, metadata=TextParser.find_int(r"Switch Port Id: (\d+)")
    )
    #:
    switch_rx_domain: int | None = field(
        default=None, metadata=TextParser.find_int(r"Switch Rx domain: (\d+)")
    )


@dataclass
class TestPmdPortStats(TextParser):
    """Port statistics."""

    #:
    port_id: int = field(metadata=TextParser.find_int(r"NIC statistics for port (\d+)"))

    #:
    rx_packets: int = field(metadata=TextParser.find_int(r"RX-packets:\s+(\d+)"))
    #:
    rx_missed: int = field(metadata=TextParser.find_int(r"RX-missed:\s+(\d+)"))
    #:
    rx_bytes: int = field(metadata=TextParser.find_int(r"RX-bytes:\s+(\d+)"))
    #:
    rx_errors: int = field(metadata=TextParser.find_int(r"RX-errors:\s+(\d+)"))
    #:
    rx_nombuf: int = field(metadata=TextParser.find_int(r"RX-nombuf:\s+(\d+)"))

    #:
    tx_packets: int = field(metadata=TextParser.find_int(r"TX-packets:\s+(\d+)"))
    #:
    tx_errors: int = field(metadata=TextParser.find_int(r"TX-errors:\s+(\d+)"))
    #:
    tx_bytes: int = field(metadata=TextParser.find_int(r"TX-bytes:\s+(\d+)"))

    #:
    rx_pps: int = field(metadata=TextParser.find_int(r"Rx-pps:\s+(\d+)"))
    #:
    rx_bps: int = field(metadata=TextParser.find_int(r"Rx-bps:\s+(\d+)"))

    #:
    tx_pps: int = field(metadata=TextParser.find_int(r"Tx-pps:\s+(\d+)"))
    #:
    tx_bps: int = field(metadata=TextParser.find_int(r"Tx-bps:\s+(\d+)"))


class TestPmdShell(DPDKShell):
    """Testpmd interactive shell.

    The testpmd shell users should never use
    the :meth:`~.interactive_shell.InteractiveShell.send_command` method directly, but rather
    call specialized methods. If there isn't one that satisfies a need, it should be added.
    """

    _app_params: TestPmdParams

    #: The path to the testpmd executable.
    path: ClassVar[PurePath] = PurePath("app", "dpdk-testpmd")

    #: The testpmd's prompt.
    _default_prompt: ClassVar[str] = "testpmd>"

    #: This forces the prompt to appear after sending a command.
    _command_extra_chars: ClassVar[str] = "\n"

    def __init__(
        self,
        node: SutNode,
        privileged: bool = True,
        timeout: float = SETTINGS.timeout,
        lcore_filter_specifier: LogicalCoreCount | LogicalCoreList = LogicalCoreCount(),
        ascending_cores: bool = True,
        append_prefix_timestamp: bool = True,
        name: str | None = None,
        **app_params: Unpack[TestPmdParamsDict],
    ) -> None:
        """Overrides :meth:`~.dpdk_shell.DPDKShell.__init__`. Changes app_params to kwargs."""
        super().__init__(
            node,
            privileged,
            timeout,
            lcore_filter_specifier,
            ascending_cores,
            append_prefix_timestamp,
            TestPmdParams(**app_params),
            name,
        )

    def start(self, verify: bool = True) -> None:
        """Start packet forwarding with the current configuration.

        Args:
            verify: If :data:`True` , a second start command will be sent in an attempt to verify
                packet forwarding started as expected.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and forwarding fails to
                start or ports fail to come up.
        """
        self.send_command("start")
        if verify:
            # If forwarding was already started, sending "start" again should tell us
            start_cmd_output = self.send_command("start")
            if "Packet forwarding already started" not in start_cmd_output:
                self._logger.debug(f"Failed to start packet forwarding: \n{start_cmd_output}")
                raise InteractiveCommandExecutionError("Testpmd failed to start packet forwarding.")

            number_of_ports = len(self._app_params.ports or [])
            for port_id in range(number_of_ports):
                if not self.wait_link_status_up(port_id):
                    raise InteractiveCommandExecutionError(
                        "Not all ports came up after starting packet forwarding in testpmd."
                    )

    def stop(self, verify: bool = True) -> None:
        """Stop packet forwarding.

        Args:
            verify: If :data:`True` , the output of the stop command is scanned to verify that
                forwarding was stopped successfully or not started. If neither is found, it is
                considered an error.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the command to stop
                forwarding results in an error.
        """
        stop_cmd_output = self.send_command("stop")
        if verify:
            if (
                "Done." not in stop_cmd_output
                and "Packet forwarding not started" not in stop_cmd_output
            ):
                self._logger.debug(f"Failed to stop packet forwarding: \n{stop_cmd_output}")
                raise InteractiveCommandExecutionError("Testpmd failed to stop packet forwarding.")

    def get_devices(self) -> list[TestPmdDevice]:
        """Get a list of device names that are known to testpmd.

        Uses the device info listed in testpmd and then parses the output.

        Returns:
            A list of devices.
        """
        dev_info: str = self.send_command("show device info all")
        dev_list: list[TestPmdDevice] = []
        for line in dev_info.split("\n"):
            if "device name:" in line.lower():
                dev_list.append(TestPmdDevice(line))
        return dev_list

    def wait_link_status_up(self, port_id: int, timeout=SETTINGS.timeout) -> bool:
        """Wait until the link status on the given port is "up".

        Arguments:
            port_id: Port to check the link status on.
            timeout: Time to wait for the link to come up. The default value for this
                argument may be modified using the :option:`--timeout` command-line argument
                or the :envvar:`DTS_TIMEOUT` environment variable.

        Returns:
            Whether the link came up in time or not.
        """
        time_to_stop = time.time() + timeout
        port_info: str = ""
        while time.time() < time_to_stop:
            port_info = self.send_command(f"show port info {port_id}")
            if "Link status: up" in port_info:
                break
            time.sleep(0.5)
        else:
            self._logger.error(f"The link for port {port_id} did not come up in the given timeout.")
        return "Link status: up" in port_info

    def set_forward_mode(self, mode: SimpleForwardingModes, verify: bool = True):
        """Set packet forwarding mode.

        Args:
            mode: The forwarding mode to use.
            verify: If :data:`True` the output of the command will be scanned in an attempt to
                verify that the forwarding mode was set to `mode` properly.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the forwarding mode
                fails to update.
        """
        set_fwd_output = self.send_command(f"set fwd {mode.value}")
        if f"Set {mode.value} packet forwarding mode" not in set_fwd_output:
            self._logger.debug(f"Failed to set fwd mode to {mode.value}:\n{set_fwd_output}")
            raise InteractiveCommandExecutionError(
                f"Test pmd failed to set fwd mode to {mode.value}"
            )

    def show_port_info_all(self) -> list[TestPmdPort]:
        """Returns the information of all the ports.

        Returns:
            list[TestPmdPort]: A list containing all the ports information as `TestPmdPort`.
        """
        output = self.send_command("show port info all")

        # Sample output of the "all" command looks like:
        #
        # <start>
        #
        #   ********************* Infos for port 0 *********************
        #   Key: value
        #
        #   ********************* Infos for port 1 *********************
        #   Key: value
        # <end>
        #
        # Takes advantage of the double new line in between ports as end delimiter. But we need to
        # artificially add a new line at the end to pick up the last port. Because commands are
        # executed on a pseudo-terminal created by paramiko on the remote node, lines end with CRLF.
        # Therefore we also need to take the carriage return into account.
        iter = re.finditer(r"\*{21}.*?[\r\n]{4}", output + "\r\n", re.S)
        return [TestPmdPort.parse(block.group(0)) for block in iter]

    def show_port_info(self, port_id: int) -> TestPmdPort:
        """Returns the given port information.

        Args:
            port_id: The port ID to gather information for.

        Raises:
            InteractiveCommandExecutionError: If `port_id` is invalid.

        Returns:
            TestPmdPort: An instance of `TestPmdPort` containing the given port's information.
        """
        output = self.send_command(f"show port info {port_id}", skip_first_line=True)
        if output.startswith("Invalid port"):
            raise InteractiveCommandExecutionError("invalid port given")

        return TestPmdPort.parse(output)

    def show_port_stats_all(self) -> list[TestPmdPortStats]:
        """Returns the statistics of all the ports.

        Returns:
            list[TestPmdPortStats]: A list containing all the ports stats as `TestPmdPortStats`.
        """
        output = self.send_command("show port stats all")

        # Sample output of the "all" command looks like:
        #
        #   ########### NIC statistics for port 0 ###########
        #   values...
        #   #################################################
        #
        #   ########### NIC statistics for port 1 ###########
        #   values...
        #   #################################################
        #
        iter = re.finditer(r"(^  #*.+#*$[^#]+)^  #*\r$", output, re.MULTILINE)
        return [TestPmdPortStats.parse(block.group(1)) for block in iter]

    def show_port_stats(self, port_id: int) -> TestPmdPortStats:
        """Returns the given port statistics.

        Args:
            port_id: The port ID to gather information for.

        Raises:
            InteractiveCommandExecutionError: If `port_id` is invalid.

        Returns:
            TestPmdPortStats: An instance of `TestPmdPortStats` containing the given port's stats.
        """
        output = self.send_command(f"show port stats {port_id}", skip_first_line=True)
        if output.startswith("Invalid port"):
            raise InteractiveCommandExecutionError("invalid port given")

        return TestPmdPortStats.parse(output)

    def _close(self) -> None:
        """Overrides :meth:`~.interactive_shell.close`."""
        self.stop()
        self.send_command("quit", "Bye...")
        return super()._close()
