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

import inspect
from collections import Counter
from collections.abc import Callable, Sequence
from enum import Enum, auto
from ipaddress import IPv4Interface, IPv6Interface, ip_interface
from typing import ClassVar, Protocol, TypeVar, Union, cast

from scapy.layers.inet import IP  # type: ignore[import-untyped]
from scapy.layers.l2 import Ether  # type: ignore[import-untyped]
from scapy.packet import Packet, Padding, raw  # type: ignore[import-untyped]

from framework.testbed_model.capability import TestProtocol
from framework.testbed_model.port import Port
from framework.testbed_model.sut_node import SutNode
from framework.testbed_model.tg_node import TGNode
from framework.testbed_model.topology import Topology
from framework.testbed_model.traffic_generator.capturing_traffic_generator import (
    PacketFilteringConfig,
)

from .exception import ConfigurationError, TestCaseVerifyError
from .logger import DTSLogger, get_dts_logger
from .utils import get_packet_summaries


class TestSuite(TestProtocol):
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
        topology: Topology,
    ):
        """Initialize the test suite testbed information and basic configuration.

        Find links between ports and set up default IP addresses to be used when
        configuring them.

        Args:
            sut_node: The SUT node where the test suite will run.
            tg_node: The TG node where the test suite will run.
            topology: The topology where the test suite will run.
        """
        self.sut_node = sut_node
        self.tg_node = tg_node
        self._logger = get_dts_logger(self.__class__.__name__)
        self._tg_port_egress = topology.tg_port_egress
        self._sut_port_ingress = topology.sut_port_ingress
        self._sut_port_egress = topology.sut_port_egress
        self._tg_port_ingress = topology.tg_port_ingress
        self._sut_ip_address_ingress = ip_interface("192.168.100.2/24")
        self._sut_ip_address_egress = ip_interface("192.168.101.2/24")
        self._tg_ip_address_egress = ip_interface("192.168.100.3/24")
        self._tg_ip_address_ingress = ip_interface("192.168.101.3/24")

    @classmethod
    def get_test_cases(
        cls, test_case_sublist: Sequence[str] | None = None
    ) -> tuple[set[type["TestCase"]], set[type["TestCase"]]]:
        """Filter `test_case_subset` from this class.

        Test cases are regular (or bound) methods decorated with :func:`func_test`
        or :func:`perf_test`.

        Args:
            test_case_sublist: Test case names to filter from this class.
                If empty or :data:`None`, return all test cases.

        Returns:
            The filtered test case functions. This method returns functions as opposed to methods,
            as methods are bound to instances and this method only has access to the class.

        Raises:
            ConfigurationError: If a test case from `test_case_subset` is not found.
        """

        def is_test_case(function: Callable) -> bool:
            if inspect.isfunction(function):
                # TestCase is not used at runtime, so we can't use isinstance() with `function`.
                # But function.test_type exists.
                if hasattr(function, "test_type"):
                    return isinstance(function.test_type, TestCaseType)
            return False

        if test_case_sublist is None:
            test_case_sublist = []

        # the copy is needed so that the condition "elif test_case_sublist" doesn't
        # change mid-cycle
        test_case_sublist_copy = list(test_case_sublist)
        func_test_cases = set()
        perf_test_cases = set()

        for test_case_name, test_case_function in inspect.getmembers(cls, is_test_case):
            if test_case_name in test_case_sublist_copy:
                # if test_case_sublist_copy is non-empty, remove the found test case
                # so that we can look at the remainder at the end
                test_case_sublist_copy.remove(test_case_name)
            elif test_case_sublist:
                # the original list not being empty means we're filtering test cases
                # since we didn't remove test_case_name in the previous branch,
                # it doesn't match the filter and we don't want to remove it
                continue

            match test_case_function.test_type:
                case TestCaseType.PERFORMANCE:
                    perf_test_cases.add(test_case_function)
                case TestCaseType.FUNCTIONAL:
                    func_test_cases.add(test_case_function)

        if test_case_sublist_copy:
            raise ConfigurationError(
                f"Test cases {test_case_sublist_copy} not found among functions of {cls.__name__}."
            )

        return func_test_cases, perf_test_cases

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
        return self.send_packets_and_capture(
            [packet],
            filter_config,
            duration,
        )

    def send_packets_and_capture(
        self,
        packets: list[Packet],
        filter_config: PacketFilteringConfig = PacketFilteringConfig(),
        duration: float = 1,
    ) -> list[Packet]:
        """Send and receive `packets` using the associated TG.

        Send `packets` through the appropriate interface and receive on the appropriate interface.
        Modify the packets with l3/l2 addresses corresponding to the testbed and desired traffic.

        Args:
            packets: The packets to send.
            filter_config: The filter to use when capturing packets.
            duration: Capture traffic for this amount of time after sending `packet`.

        Returns:
            A list of received packets.
        """
        packets = self._adjust_addresses(packets)
        return self.tg_node.send_packets_and_capture(
            packets,
            self._tg_port_egress,
            self._tg_port_ingress,
            filter_config,
            duration,
        )

    def send_packets(
        self,
        packets: list[Packet],
    ) -> None:
        """Send packets using the traffic generator and do not capture received traffic.

        Args:
            packets: Packets to send.
        """
        packets = self._adjust_addresses(packets)
        self.tg_node.send_packets(packets, self._tg_port_egress)

    def get_expected_packet(self, packet: Packet) -> Packet:
        """Inject the proper L2/L3 addresses into `packet`.

        Args:
            packet: The packet to modify.

        Returns:
            `packet` with injected L2/L3 addresses.
        """
        return self._adjust_addresses([packet], expected=True)[0]

    def _adjust_addresses(self, packets: list[Packet], expected: bool = False) -> list[Packet]:
        """L2 and L3 address additions in both directions.

        Packets in `packets` will be directly modified in this method. The returned list of packets
        however will be copies of the modified packets.

        Only missing addresses are added to packets, existing addresses will not be overridden. If
        any packet in `packets` has multiple IP layers (using GRE, for example) only the inner-most
        IP layer will have its addresses adjusted.

        Assumptions:
            Two links between SUT and TG, one link is TG -> SUT, the other SUT -> TG.

        Args:
            packets: The packets to modify.
            expected: If :data:`True`, the direction is SUT -> TG,
                otherwise the direction is TG -> SUT.

        Returns:
            A list containing copies of all packets in `packets` after modification.
        """
        ret_packets = []
        for packet in packets:
            # update l2 addresses
            # If `expected` is :data:`True`, the packet enters the TG from SUT, otherwise the
            # packet leaves the TG towards the SUT.

            # The fields parameter of a packet does not include fields of the payload, so this can
            # only be the Ether src/dst.
            if "src" not in packet.fields:
                packet.src = (
                    self._sut_port_egress.mac_address
                    if expected
                    else self._tg_port_egress.mac_address
                )
            if "dst" not in packet.fields:
                packet.dst = (
                    self._tg_port_ingress.mac_address
                    if expected
                    else self._sut_port_ingress.mac_address
                )

            # update l3 addresses
            # The packet is routed from TG egress to TG ingress regardless of whether it is
            # expected or not.
            num_ip_layers = packet.layers().count(IP)
            if num_ip_layers > 0:
                # Update the last IP layer if there are multiple (the framework should be modifying
                # the packet address instead of the tunnel address if there is one).
                l3_to_use = packet.getlayer(IP, num_ip_layers)
                if "src" not in l3_to_use.fields:
                    l3_to_use.src = self._tg_ip_address_egress.ip.exploded

                if "dst" not in l3_to_use.fields:
                    l3_to_use.dst = self._tg_ip_address_ingress.ip.exploded
            ret_packets.append(Ether(packet.build()))

        return ret_packets

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

    def match_all_packets(
        self, expected_packets: list[Packet], received_packets: list[Packet]
    ) -> None:
        """Matches all the expected packets against the received ones.

        Matching is performed by counting down the occurrences in a dictionary which keys are the
        raw packet bytes. No deep packet comparison is performed. All the unexpected packets (noise)
        are automatically ignored.

        Args:
            expected_packets: The packets we are expecting to receive.
            received_packets: All the packets that were received.

        Raises:
            TestCaseVerifyError: if and not all the `expected_packets` were found in
                `received_packets`.
        """
        expected_packets_counters = Counter(map(raw, expected_packets))
        received_packets_counters = Counter(map(raw, received_packets))
        # The number of expected packets is subtracted by the number of received packets, ignoring
        # any unexpected packets and capping at zero.
        missing_packets_counters = expected_packets_counters - received_packets_counters
        missing_packets_count = missing_packets_counters.total()
        self._logger.debug(
            f"match_all_packets: expected {len(expected_packets)}, "
            f"received {len(received_packets)}, missing {missing_packets_count}"
        )

        if missing_packets_count != 0:
            self._fail_test_case_verify(
                f"Not all packets were received, expected {len(expected_packets)} "
                f"but {missing_packets_count} were missing."
            )

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


#: The generic type for a method of an instance of TestSuite
TestSuiteMethodType = TypeVar("TestSuiteMethodType", bound=Callable[[TestSuite], None])


class TestCaseType(Enum):
    """The types of test cases."""

    #:
    FUNCTIONAL = auto()
    #:
    PERFORMANCE = auto()


class TestCase(TestProtocol, Protocol[TestSuiteMethodType]):
    """Definition of the test case type for static type checking purposes.

    The type is applied to test case functions through a decorator, which casts the decorated
    test case function to :class:`TestCase` and sets common variables.
    """

    #:
    test_type: ClassVar[TestCaseType]
    #: necessary for mypy so that it can treat this class as the function it's shadowing
    __call__: TestSuiteMethodType

    @classmethod
    def make_decorator(
        cls, test_case_type: TestCaseType
    ) -> Callable[[TestSuiteMethodType], type["TestCase"]]:
        """Create a decorator for test suites.

        The decorator casts the decorated function as :class:`TestCase`,
        sets it as `test_case_type`
        and initializes common variables defined in :class:`RequiresCapabilities`.

        Args:
            test_case_type: Either a functional or performance test case.

        Returns:
            The decorator of a functional or performance test case.
        """

        def _decorator(func: TestSuiteMethodType) -> type[TestCase]:
            test_case = cast(type[TestCase], func)
            test_case.skip = cls.skip
            test_case.skip_reason = cls.skip_reason
            test_case.required_capabilities = set()
            test_case.topology_type = cls.topology_type
            test_case.topology_type.add_to_required(test_case)
            test_case.test_type = test_case_type
            return test_case

        return _decorator


#: The decorator for functional test cases.
func_test: Callable = TestCase.make_decorator(TestCaseType.FUNCTIONAL)
#: The decorator for performance test cases.
perf_test: Callable = TestCase.make_decorator(TestCaseType.PERFORMANCE)
