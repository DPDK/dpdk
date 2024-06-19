# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Features common to all test suites.

The module defines the :class:`TestSuite` class which doesn't contain any test cases, and as such
must be extended by subclasses which add test cases. The :class:`TestSuite` contains the basics
needed by subclasses:

    * Testbed (SUT, TG) configuration,
    * Packet sending and verification,
    * Test case verification.
"""

from ipaddress import IPv4Interface, IPv6Interface, ip_interface
from typing import ClassVar, Union

from scapy.layers.inet import IP  # type: ignore[import-untyped]
from scapy.layers.l2 import Ether  # type: ignore[import-untyped]
from scapy.packet import Packet, Padding  # type: ignore[import-untyped]

from framework.testbed_model.port import Port, PortLink
from framework.testbed_model.sut_node import SutNode
from framework.testbed_model.tg_node import TGNode
from framework.testbed_model.traffic_generator.capturing_traffic_generator import (
    PacketFilteringConfig,
)

from .exception import TestCaseVerifyError
from .logger import DTSLogger, get_dts_logger
from .utils import get_packet_summaries


class TestSuite:
    """The base class with building blocks needed by most test cases.

        * Test suite setup/cleanup methods to override,
        * Test case setup/cleanup methods to override,
        * Test case verification,
        * Testbed configuration,
        * Traffic sending and verification.

    Test cases are implemented by subclasses. Test cases are all methods starting with ``test_``,
    further divided into performance test cases (starting with ``test_perf_``)
    and functional test cases (all other test cases).

    By default, all test cases will be executed. A list of testcase names may be specified
    in the YAML test run configuration file and in the :option:`--test-suite` command line argument
    or in the :envvar:`DTS_TESTCASES` environment variable to filter which test cases to run.
    The union of both lists will be used. Any unknown test cases from the latter lists
    will be silently ignored.

    The methods named ``[set_up|tear_down]_[suite|test_case]`` should be overridden in subclasses
    if the appropriate test suite/test case fixtures are needed.

    The test suite is aware of the testbed (the SUT and TG) it's running on. From this, it can
    properly choose the IP addresses and other configuration that must be tailored to the testbed.

    Attributes:
        sut_node: The SUT node where the test suite is running.
        tg_node: The TG node where the test suite is running.
    """

    sut_node: SutNode
    tg_node: TGNode
    #: Whether the test suite is blocking. A failure of a blocking test suite
    #: will block the execution of all subsequent test suites in the current build target.
    is_blocking: ClassVar[bool] = False
    _logger: DTSLogger
    _port_links: list[PortLink]
    _sut_port_ingress: Port
    _sut_port_egress: Port
    _sut_ip_address_ingress: Union[IPv4Interface, IPv6Interface]
    _sut_ip_address_egress: Union[IPv4Interface, IPv6Interface]
    _tg_port_ingress: Port
    _tg_port_egress: Port
    _tg_ip_address_ingress: Union[IPv4Interface, IPv6Interface]
    _tg_ip_address_egress: Union[IPv4Interface, IPv6Interface]

    def __init__(
        self,
        sut_node: SutNode,
        tg_node: TGNode,
    ):
        """Initialize the test suite testbed information and basic configuration.

        Find links between ports and set up default IP addresses to be used when
        configuring them.

        Args:
            sut_node: The SUT node where the test suite will run.
            tg_node: The TG node where the test suite will run.
        """
        self.sut_node = sut_node
        self.tg_node = tg_node
        self._logger = get_dts_logger(self.__class__.__name__)
        self._port_links = []
        self._process_links()
        self._sut_port_ingress, self._tg_port_egress = (
            self._port_links[0].sut_port,
            self._port_links[0].tg_port,
        )
        self._sut_port_egress, self._tg_port_ingress = (
            self._port_links[1].sut_port,
            self._port_links[1].tg_port,
        )
        self._sut_ip_address_ingress = ip_interface("192.168.100.2/24")
        self._sut_ip_address_egress = ip_interface("192.168.101.2/24")
        self._tg_ip_address_egress = ip_interface("192.168.100.3/24")
        self._tg_ip_address_ingress = ip_interface("192.168.101.3/24")

    def _process_links(self) -> None:
        """Construct links between SUT and TG ports."""
        for sut_port in self.sut_node.ports:
            for tg_port in self.tg_node.ports:
                if (sut_port.identifier, sut_port.peer) == (
                    tg_port.peer,
                    tg_port.identifier,
                ):
                    self._port_links.append(PortLink(sut_port=sut_port, tg_port=tg_port))

    def set_up_suite(self) -> None:
        """Set up test fixtures common to all test cases.

        This is done before any test case has been run.
        """

    def tear_down_suite(self) -> None:
        """Tear down the previously created test fixtures common to all test cases.

        This is done after all test have been run.
        """

    def set_up_test_case(self) -> None:
        """Set up test fixtures before each test case.

        This is done before *each* test case.
        """

    def tear_down_test_case(self) -> None:
        """Tear down the previously created test fixtures after each test case.

        This is done after *each* test case.
        """

    def configure_testbed_ipv4(self, restore: bool = False) -> None:
        """Configure IPv4 addresses on all testbed ports.

        The configured ports are:

        * SUT ingress port,
        * SUT egress port,
        * TG ingress port,
        * TG egress port.

        Args:
            restore: If :data:`True`, will remove the configuration instead.
        """
        delete = True if restore else False
        enable = False if restore else True
        self._configure_ipv4_forwarding(enable)
        self.sut_node.configure_port_ip_address(
            self._sut_ip_address_egress, self._sut_port_egress, delete
        )
        self.sut_node.configure_port_state(self._sut_port_egress, enable)
        self.sut_node.configure_port_ip_address(
            self._sut_ip_address_ingress, self._sut_port_ingress, delete
        )
        self.sut_node.configure_port_state(self._sut_port_ingress, enable)
        self.tg_node.configure_port_ip_address(
            self._tg_ip_address_ingress, self._tg_port_ingress, delete
        )
        self.tg_node.configure_port_state(self._tg_port_ingress, enable)
        self.tg_node.configure_port_ip_address(
            self._tg_ip_address_egress, self._tg_port_egress, delete
        )
        self.tg_node.configure_port_state(self._tg_port_egress, enable)

    def _configure_ipv4_forwarding(self, enable: bool) -> None:
        self.sut_node.configure_ipv4_forwarding(enable)

    def send_packet_and_capture(
        self,
        packet: Packet,
        filter_config: PacketFilteringConfig = PacketFilteringConfig(),
        duration: float = 1,
    ) -> list[Packet]:
        """Send and receive `packet` using the associated TG.

        Send `packet` through the appropriate interface and receive on the appropriate interface.
        Modify the packet with l3/l2 addresses corresponding to the testbed and desired traffic.

        Args:
            packet: The packet to send.
            filter_config: The filter to use when capturing packets.
            duration: Capture traffic for this amount of time after sending `packet`.

        Returns:
            A list of received packets.
        """
        packet = self._adjust_addresses(packet)
        return self.tg_node.send_packet_and_capture(
            packet,
            self._tg_port_egress,
            self._tg_port_ingress,
            filter_config,
            duration,
        )

    def get_expected_packet(self, packet: Packet) -> Packet:
        """Inject the proper L2/L3 addresses into `packet`.

        Args:
            packet: The packet to modify.

        Returns:
            `packet` with injected L2/L3 addresses.
        """
        return self._adjust_addresses(packet, expected=True)

    def _adjust_addresses(self, packet: Packet, expected: bool = False) -> Packet:
        """L2 and L3 address additions in both directions.

        Assumptions:
            Two links between SUT and TG, one link is TG -> SUT, the other SUT -> TG.

        Args:
            packet: The packet to modify.
            expected: If :data:`True`, the direction is SUT -> TG,
                otherwise the direction is TG -> SUT.
        """
        if expected:
            # The packet enters the TG from SUT
            # update l2 addresses
            packet.src = self._sut_port_egress.mac_address
            packet.dst = self._tg_port_ingress.mac_address

            # The packet is routed from TG egress to TG ingress
            # update l3 addresses
            packet.payload.src = self._tg_ip_address_egress.ip.exploded
            packet.payload.dst = self._tg_ip_address_ingress.ip.exploded
        else:
            # The packet leaves TG towards SUT
            # update l2 addresses
            packet.src = self._tg_port_egress.mac_address
            packet.dst = self._sut_port_ingress.mac_address

            # The packet is routed from TG egress to TG ingress
            # update l3 addresses
            packet.payload.src = self._tg_ip_address_egress.ip.exploded
            packet.payload.dst = self._tg_ip_address_ingress.ip.exploded

        return Ether(packet.build())

    def verify(self, condition: bool, failure_description: str) -> None:
        """Verify `condition` and handle failures.

        When `condition` is :data:`False`, raise an exception and log the last 10 commands
        executed on both the SUT and TG.

        Args:
            condition: The condition to check.
            failure_description: A short description of the failure
                that will be stored in the raised exception.

        Raises:
            TestCaseVerifyError: `condition` is :data:`False`.
        """
        if not condition:
            self._fail_test_case_verify(failure_description)

    def _fail_test_case_verify(self, failure_description: str) -> None:
        self._logger.debug("A test case failed, showing the last 10 commands executed on SUT:")
        for command_res in self.sut_node.main_session.remote_session.history[-10:]:
            self._logger.debug(command_res.command)
        self._logger.debug("A test case failed, showing the last 10 commands executed on TG:")
        for command_res in self.tg_node.main_session.remote_session.history[-10:]:
            self._logger.debug(command_res.command)
        raise TestCaseVerifyError(failure_description)

    def verify_packets(self, expected_packet: Packet, received_packets: list[Packet]) -> None:
        """Verify that `expected_packet` has been received.

        Go through `received_packets` and check that `expected_packet` is among them.
        If not, raise an exception and log the last 10 commands
        executed on both the SUT and TG.

        Args:
            expected_packet: The packet we're expecting to receive.
            received_packets: The packets where we're looking for `expected_packet`.

        Raises:
            TestCaseVerifyError: `expected_packet` is not among `received_packets`.
        """
        for received_packet in received_packets:
            if self._compare_packets(expected_packet, received_packet):
                break
        else:
            self._logger.debug(
                f"The expected packet {get_packet_summaries(expected_packet)} "
                f"not found among received {get_packet_summaries(received_packets)}"
            )
            self._fail_test_case_verify("An expected packet not found among received packets.")

    def _compare_packets(self, expected_packet: Packet, received_packet: Packet) -> bool:
        self._logger.debug(
            f"Comparing packets: \n{expected_packet.summary()}\n{received_packet.summary()}"
        )

        l3 = IP in expected_packet.layers()
        self._logger.debug("Found l3 layer")

        received_payload = received_packet
        expected_payload = expected_packet
        while received_payload and expected_payload:
            self._logger.debug("Comparing payloads:")
            self._logger.debug(f"Received: {received_payload}")
            self._logger.debug(f"Expected: {expected_payload}")
            if received_payload.__class__ == expected_payload.__class__:
                self._logger.debug("The layers are the same.")
                if received_payload.__class__ == Ether:
                    if not self._verify_l2_frame(received_payload, l3):
                        return False
                elif received_payload.__class__ == IP:
                    if not self._verify_l3_packet(received_payload, expected_payload):
                        return False
            else:
                # Different layers => different packets
                return False
            received_payload = received_payload.payload
            expected_payload = expected_payload.payload

        if expected_payload:
            self._logger.debug(f"The expected packet did not contain {expected_payload}.")
            return False
        if received_payload and received_payload.__class__ != Padding:
            self._logger.debug("The received payload had extra layers which were not padding.")
            return False
        return True

    def _verify_l2_frame(self, received_packet: Ether, l3: bool) -> bool:
        self._logger.debug("Looking at the Ether layer.")
        self._logger.debug(
            f"Comparing received dst mac '{received_packet.dst}' "
            f"with expected '{self._tg_port_ingress.mac_address}'."
        )
        if received_packet.dst != self._tg_port_ingress.mac_address:
            return False

        expected_src_mac = self._tg_port_egress.mac_address
        if l3:
            expected_src_mac = self._sut_port_egress.mac_address
        self._logger.debug(
            f"Comparing received src mac '{received_packet.src}' "
            f"with expected '{expected_src_mac}'."
        )
        if received_packet.src != expected_src_mac:
            return False

        return True

    def _verify_l3_packet(self, received_packet: IP, expected_packet: IP) -> bool:
        self._logger.debug("Looking at the IP layer.")
        if received_packet.src != expected_packet.src or received_packet.dst != expected_packet.dst:
            return False
        return True
