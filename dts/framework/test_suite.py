# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""
Base class for creating DTS test cases.
"""

import importlib
import inspect
import re
from ipaddress import IPv4Interface, IPv6Interface, ip_interface
from types import MethodType
from typing import Union

from scapy.layers.inet import IP  # type: ignore[import]
from scapy.layers.l2 import Ether  # type: ignore[import]
from scapy.packet import Packet, Padding  # type: ignore[import]

from .exception import (
    BlockingTestSuiteError,
    ConfigurationError,
    SSHTimeoutError,
    TestCaseVerifyError,
)
from .logger import DTSLOG, getLogger
from .settings import SETTINGS
from .test_result import BuildTargetResult, Result, TestCaseResult, TestSuiteResult
from .testbed_model import SutNode, TGNode
from .testbed_model.hw.port import Port, PortLink
from .utils import get_packet_summaries


class TestSuite(object):
    """
    The base TestSuite class provides methods for handling basic flow of a test suite:
    * test case filtering and collection
    * test suite setup/cleanup
    * test setup/cleanup
    * test case execution
    * error handling and results storage
    Test cases are implemented by derived classes. Test cases are all methods
    starting with test_, further divided into performance test cases
    (starting with test_perf_) and functional test cases (all other test cases).
    By default, all test cases will be executed. A list of testcase str names
    may be specified in conf.yaml or on the command line
    to filter which test cases to run.
    The methods named [set_up|tear_down]_[suite|test_case] should be overridden
    in derived classes if the appropriate suite/test case fixtures are needed.
    """

    sut_node: SutNode
    is_blocking = False
    _logger: DTSLOG
    _test_cases_to_run: list[str]
    _func: bool
    _result: TestSuiteResult
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
        test_cases: list[str],
        func: bool,
        build_target_result: BuildTargetResult,
    ):
        self.sut_node = sut_node
        self.tg_node = tg_node
        self._logger = getLogger(self.__class__.__name__)
        self._test_cases_to_run = test_cases
        self._test_cases_to_run.extend(SETTINGS.test_cases)
        self._func = func
        self._result = build_target_result.add_test_suite(self.__class__.__name__)
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
        for sut_port in self.sut_node.ports:
            for tg_port in self.tg_node.ports:
                if (sut_port.identifier, sut_port.peer) == (
                    tg_port.peer,
                    tg_port.identifier,
                ):
                    self._port_links.append(
                        PortLink(sut_port=sut_port, tg_port=tg_port)
                    )

    def set_up_suite(self) -> None:
        """
        Set up test fixtures common to all test cases; this is done before
        any test case is run.
        """

    def tear_down_suite(self) -> None:
        """
        Tear down the previously created test fixtures common to all test cases.
        """

    def set_up_test_case(self) -> None:
        """
        Set up test fixtures before each test case.
        """

    def tear_down_test_case(self) -> None:
        """
        Tear down the previously created test fixtures after each test case.
        """

    def configure_testbed_ipv4(self, restore: bool = False) -> None:
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
        self, packet: Packet, duration: float = 1
    ) -> list[Packet]:
        """
        Send a packet through the appropriate interface and
        receive on the appropriate interface.
        Modify the packet with l3/l2 addresses corresponding
        to the testbed and desired traffic.
        """
        packet = self._adjust_addresses(packet)
        return self.tg_node.send_packet_and_capture(
            packet, self._tg_port_egress, self._tg_port_ingress, duration
        )

    def get_expected_packet(self, packet: Packet) -> Packet:
        return self._adjust_addresses(packet, expected=True)

    def _adjust_addresses(self, packet: Packet, expected: bool = False) -> Packet:
        """
        Assumptions:
            Two links between SUT and TG, one link is TG -> SUT,
            the other SUT -> TG.
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
        if not condition:
            self._fail_test_case_verify(failure_description)

    def _fail_test_case_verify(self, failure_description: str) -> None:
        self._logger.debug(
            "A test case failed, showing the last 10 commands executed on SUT:"
        )
        for command_res in self.sut_node.main_session.remote_session.history[-10:]:
            self._logger.debug(command_res.command)
        self._logger.debug(
            "A test case failed, showing the last 10 commands executed on TG:"
        )
        for command_res in self.tg_node.main_session.remote_session.history[-10:]:
            self._logger.debug(command_res.command)
        raise TestCaseVerifyError(failure_description)

    def verify_packets(
        self, expected_packet: Packet, received_packets: list[Packet]
    ) -> None:
        for received_packet in received_packets:
            if self._compare_packets(expected_packet, received_packet):
                break
        else:
            self._logger.debug(
                f"The expected packet {get_packet_summaries(expected_packet)} "
                f"not found among received {get_packet_summaries(received_packets)}"
            )
            self._fail_test_case_verify(
                "An expected packet not found among received packets."
            )

    def _compare_packets(
        self, expected_packet: Packet, received_packet: Packet
    ) -> bool:
        self._logger.debug(
            "Comparing packets: \n"
            f"{expected_packet.summary()}\n"
            f"{received_packet.summary()}"
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
            self._logger.debug(
                f"The expected packet did not contain {expected_payload}."
            )
            return False
        if received_payload and received_payload.__class__ != Padding:
            self._logger.debug(
                "The received payload had extra layers which were not padding."
            )
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
        if (
            received_packet.src != expected_packet.src
            or received_packet.dst != expected_packet.dst
        ):
            return False
        return True

    def run(self) -> None:
        """
        Setup, execute and teardown the whole suite.
        Suite execution consists of running all test cases scheduled to be executed.
        A test cast run consists of setup, execution and teardown of said test case.
        """
        test_suite_name = self.__class__.__name__

        try:
            self._logger.info(f"Starting test suite setup: {test_suite_name}")
            self.set_up_suite()
            self._result.update_setup(Result.PASS)
            self._logger.info(f"Test suite setup successful: {test_suite_name}")
        except Exception as e:
            self._logger.exception(f"Test suite setup ERROR: {test_suite_name}")
            self._result.update_setup(Result.ERROR, e)

        else:
            self._execute_test_suite()

        finally:
            try:
                self.tear_down_suite()
                self.sut_node.kill_cleanup_dpdk_apps()
                self._result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception(f"Test suite teardown ERROR: {test_suite_name}")
                self._logger.warning(
                    f"Test suite '{test_suite_name}' teardown failed, "
                    f"the next test suite may be affected."
                )
                self._result.update_setup(Result.ERROR, e)
            if len(self._result.get_errors()) > 0 and self.is_blocking:
                raise BlockingTestSuiteError(test_suite_name)

    def _execute_test_suite(self) -> None:
        """
        Execute all test cases scheduled to be executed in this suite.
        """
        if self._func:
            for test_case_method in self._get_functional_test_cases():
                test_case_name = test_case_method.__name__
                test_case_result = self._result.add_test_case(test_case_name)
                all_attempts = SETTINGS.re_run + 1
                attempt_nr = 1
                self._run_test_case(test_case_method, test_case_result)
                while not test_case_result and attempt_nr < all_attempts:
                    attempt_nr += 1
                    self._logger.info(
                        f"Re-running FAILED test case '{test_case_name}'. "
                        f"Attempt number {attempt_nr} out of {all_attempts}."
                    )
                    self._run_test_case(test_case_method, test_case_result)

    def _get_functional_test_cases(self) -> list[MethodType]:
        """
        Get all functional test cases.
        """
        return self._get_test_cases(r"test_(?!perf_)")

    def _get_test_cases(self, test_case_regex: str) -> list[MethodType]:
        """
        Return a list of test cases matching test_case_regex.
        """
        self._logger.debug(f"Searching for test cases in {self.__class__.__name__}.")
        filtered_test_cases = []
        for test_case_name, test_case in inspect.getmembers(self, inspect.ismethod):
            if self._should_be_executed(test_case_name, test_case_regex):
                filtered_test_cases.append(test_case)
        cases_str = ", ".join((x.__name__ for x in filtered_test_cases))
        self._logger.debug(
            f"Found test cases '{cases_str}' in {self.__class__.__name__}."
        )
        return filtered_test_cases

    def _should_be_executed(self, test_case_name: str, test_case_regex: str) -> bool:
        """
        Check whether the test case should be executed.
        """
        match = bool(re.match(test_case_regex, test_case_name))
        if self._test_cases_to_run:
            return match and test_case_name in self._test_cases_to_run

        return match

    def _run_test_case(
        self, test_case_method: MethodType, test_case_result: TestCaseResult
    ) -> None:
        """
        Setup, execute and teardown a test case in this suite.
        Exceptions are caught and recorded in logs and results.
        """
        test_case_name = test_case_method.__name__

        try:
            # run set_up function for each case
            self.set_up_test_case()
            test_case_result.update_setup(Result.PASS)
        except SSHTimeoutError as e:
            self._logger.exception(f"Test case setup FAILED: {test_case_name}")
            test_case_result.update_setup(Result.FAIL, e)
        except Exception as e:
            self._logger.exception(f"Test case setup ERROR: {test_case_name}")
            test_case_result.update_setup(Result.ERROR, e)

        else:
            # run test case if setup was successful
            self._execute_test_case(test_case_method, test_case_result)

        finally:
            try:
                self.tear_down_test_case()
                test_case_result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception(f"Test case teardown ERROR: {test_case_name}")
                self._logger.warning(
                    f"Test case '{test_case_name}' teardown failed, "
                    f"the next test case may be affected."
                )
                test_case_result.update_teardown(Result.ERROR, e)
                test_case_result.update(Result.ERROR)

    def _execute_test_case(
        self, test_case_method: MethodType, test_case_result: TestCaseResult
    ) -> None:
        """
        Execute one test case and handle failures.
        """
        test_case_name = test_case_method.__name__
        try:
            self._logger.info(f"Starting test case execution: {test_case_name}")
            test_case_method()
            test_case_result.update(Result.PASS)
            self._logger.info(f"Test case execution PASSED: {test_case_name}")

        except TestCaseVerifyError as e:
            self._logger.exception(f"Test case execution FAILED: {test_case_name}")
            test_case_result.update(Result.FAIL, e)
        except Exception as e:
            self._logger.exception(f"Test case execution ERROR: {test_case_name}")
            test_case_result.update(Result.ERROR, e)
        except KeyboardInterrupt:
            self._logger.error(
                f"Test case execution INTERRUPTED by user: {test_case_name}"
            )
            test_case_result.update(Result.SKIP)
            raise KeyboardInterrupt("Stop DTS")


def get_test_suites(testsuite_module_path: str) -> list[type[TestSuite]]:
    def is_test_suite(object) -> bool:
        try:
            if issubclass(object, TestSuite) and object is not TestSuite:
                return True
        except TypeError:
            return False
        return False

    try:
        testcase_module = importlib.import_module(testsuite_module_path)
    except ModuleNotFoundError as e:
        raise ConfigurationError(
            f"Test suite '{testsuite_module_path}' not found."
        ) from e
    return [
        test_suite_class
        for _, test_suite_class in inspect.getmembers(testcase_module, is_test_suite)
    ]
