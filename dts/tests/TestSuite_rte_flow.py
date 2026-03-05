# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""RTE Flow testing suite.

This suite verifies a range of flow rules built using patterns
and actions from the RTE Flow API. It would be impossible to cover
every valid flow rule, but this suite aims to test the most
important and common functionalities across PMDs.

"""

from dataclasses import dataclass
from itertools import product
from typing import Any, Callable, Optional

from scapy.layers.inet import ICMP, IP, TCP, UDP
from scapy.layers.inet6 import IPv6
from scapy.layers.l2 import ARP, Dot1Q, Ether
from scapy.layers.sctp import SCTP
from scapy.packet import Packet, Raw

from api.capabilities import NicCapability, requires_nic_capability
from api.packet import send_packet_and_capture
from api.test import fail, log, verify
from api.testpmd import TestPmd
from api.testpmd.types import FlowRule
from framework.exception import (
    InteractiveCommandExecutionError,
    SkippedTestException,
    TestCaseVerifyError,
)
from framework.test_suite import TestSuite, func_test


@dataclass
class PatternField:
    """Specification for a single matchable field within a protocol layer."""

    scapy_field: str
    pattern_field: str
    test_parameters: list[Any]


@dataclass
class Protocol:
    """Complete specification for a protocol layer."""

    name: str
    scapy_class: type[Packet]
    pattern_name: str
    fields: list[PatternField]

    def build_scapy_layer(self, field_values: dict[str, Any]) -> Packet:
        """Construct a Scapy layer with the given field values."""
        return self.scapy_class(**field_values)


@dataclass
class Action:
    """Specification for a flow action."""

    name: str
    action_format: str
    verification_type: str
    param_builder: Callable[[Any], dict[str, Any]]

    def build_action_string(self, value: Any = None) -> str:
        """Generate the action string for a flow rule."""
        if value is not None and "{value}" in self.action_format:
            return self.action_format.format(value=value)
        return self.action_format

    def build_verification_params(self, value: Any = None) -> dict[str, Any]:
        """Generate verification parameters for this action."""
        return self.param_builder(value)


@dataclass
class FlowTest:
    """A complete flow test ready for execution."""

    flow_rule: FlowRule
    packet: Packet
    verification_type: str
    verification_params: dict[str, Any]
    description: str = ""


@dataclass
class FlowTestResult:
    """Result of a single test case execution."""

    description: str
    passed: bool
    failure_reason: str = ""
    flow_rule_pattern: str = ""
    skipped: bool = False
    sent_packet: Optional["Packet"] = None


@dataclass
class ModifyTest:
    """A single hardcoded modify action test case."""

    description: str
    pattern: str
    action: str
    packet: Packet
    expected_ipv4_dst: str | None = None
    expected_mac_dst: str | None = None


@dataclass
class JumpTest:
    """A single hardcoded jump action test case."""

    description: str
    group0_pattern: str
    group1_pattern: str
    group1_action: str
    matching_packet: Packet
    non_matching_packet: Packet
    target_queue: int


PROTOCOLS: dict[str, Protocol] = {
    "eth": Protocol(
        name="eth",
        scapy_class=Ether,
        pattern_name="eth",
        fields=[
            PatternField("src", "src", ["02:00:00:00:00:00"]),
            PatternField("dst", "dst", ["02:00:00:00:00:02"]),
        ],
    ),
    "ipv4": Protocol(
        name="ipv4",
        scapy_class=IP,
        pattern_name="ipv4",
        fields=[
            PatternField("src", "src", ["192.168.1.1"]),
            PatternField("dst", "dst", ["192.168.1.2"]),
            PatternField("ttl", "ttl", [64, 128]),
            PatternField("tos", "tos", [0, 4]),
        ],
    ),
    "ipv6": Protocol(
        name="ipv6",
        scapy_class=IPv6,
        pattern_name="ipv6",
        fields=[
            PatternField("src", "src", ["2001:db8::1"]),
            PatternField("dst", "dst", ["2001:db8::2"]),
            PatternField("tc", "tc", [0, 4]),
            PatternField("hlim", "hop", [64, 128]),
        ],
    ),
    "tcp": Protocol(
        name="tcp",
        scapy_class=TCP,
        pattern_name="tcp",
        fields=[
            PatternField("sport", "src", [1234, 8080]),
            PatternField("dport", "dst", [80, 443]),
            PatternField("flags", "flags", [2, 16]),
        ],
    ),
    "udp": Protocol(
        name="udp",
        scapy_class=UDP,
        pattern_name="udp",
        fields=[
            PatternField("sport", "src", [5000]),
            PatternField("dport", "dst", [53, 123]),
        ],
    ),
    "vlan": Protocol(
        name="vlan",
        scapy_class=Dot1Q,
        pattern_name="vlan",
        fields=[
            PatternField("vlan", "vid", [100, 200]),
            PatternField("prio", "pcp", [0, 7]),
        ],
    ),
    "icmp": Protocol(
        name="icmp",
        scapy_class=ICMP,
        pattern_name="icmp",
        fields=[
            PatternField("type", "type", [8, 0]),
            PatternField("code", "code", [0]),
            PatternField("id", "ident", [0, 1234]),
            PatternField("seq", "seq", [0, 1]),
        ],
    ),
    "sctp": Protocol(
        name="sctp",
        scapy_class=SCTP,
        pattern_name="sctp",
        fields=[
            PatternField("sport", "src", [2905, 3868]),
            PatternField("dport", "dst", [2905, 3868]),
            PatternField("tag", "tag", [1, 12346]),
        ],
    ),
    "arp": Protocol(
        name="arp",
        scapy_class=ARP,
        pattern_name="arp_eth_ipv4",
        fields=[
            PatternField("psrc", "spa", ["192.168.1.1"]),
            PatternField("pdst", "tpa", ["192.168.1.2"]),
            PatternField("op", "opcode", [1, 2]),
        ],
    ),
}


ACTIONS: dict[str, Action] = {
    "queue": Action(
        name="queue",
        action_format="queue index {value}",
        verification_type="queue",
        param_builder=lambda queue_id: {"queue_id": queue_id},
    ),
    "drop": Action(
        name="drop",
        action_format="drop",
        verification_type="drop",
        param_builder=lambda _: {},
    ),
}

PROTOCOL_STACKS = [
    [("eth", True)],
    [("eth", False), ("ipv4", True)],
    [("eth", False), ("ipv4", True), ("tcp", True)],
    [("eth", False), ("ipv4", True), ("udp", True)],
    [("eth", False), ("ipv4", True), ("icmp", True)],
    [("eth", False), ("ipv4", True), ("sctp", True)],
    [("eth", False), ("ipv6", True)],
    [("eth", False), ("ipv6", True), ("tcp", True)],
    [("eth", False), ("ipv6", True), ("udp", True)],
    [("eth", False), ("ipv6", True), ("sctp", True)],
    [("eth", False), ("vlan", True)],
    [("eth", False), ("vlan", True), ("ipv4", True)],
    [("eth", False), ("vlan", False), ("ipv4", True), ("tcp", True)],
    [("eth", False), ("vlan", False), ("ipv4", True), ("udp", True)],
    [("eth", False), ("vlan", False), ("ipv4", True), ("sctp", True)],
    [("eth", False), ("vlan", True), ("ipv6", True)],
    [("eth", False), ("vlan", False), ("ipv6", True), ("tcp", True)],
    [("eth", False), ("vlan", False), ("ipv6", True), ("udp", True)],
    [("eth", False), ("arp", True)],
]


class FlowTestGenerator:
    """Generates test cases by combining patterns and actions."""

    def __init__(self, protocols: dict[str, Protocol], actions: dict[str, Action]):
        """Initialize the generator with protocol and action specifications."""
        self.protocols = protocols
        self.actions = actions

    def _build_multi_layer_packet(
        self,
        protocol_stack: list[str],
        all_field_values: dict[str, dict[str, Any]],
        add_payload: bool = True,
    ) -> Packet:
        """Build a packet from multiple protocol layers.

        Raises:
            ValueError: If protocol_stack is empty.
        """
        packet: Optional["Packet"] = None

        for protocol_name in protocol_stack:
            protocol_spec = self.protocols[protocol_name]
            values = all_field_values.get(protocol_name, {})
            layer = protocol_spec.build_scapy_layer(values)

            if packet is None:
                packet = layer
            else:
                packet = packet / layer

        if add_payload:
            packet = packet / Raw(load="X" * 5)

        if packet is None:
            raise ValueError("Protocol stack cannot be empty.")

        return packet

    def generate(
        self,
        protocol_stack: list[tuple[str, bool]],
        action_name: str,
        action_value: Any = None,
        group_id: int = 0,
    ) -> list[FlowTest]:
        """Generate test cases for patterns matching fields across multiple protocols.

        Args:
            protocol_stack: List of (protocol_name, test_fields) tuples.
                If test_fields is True, iterate through field combinations.
                If False, include protocol in pattern as wildcard match only.
            action_name: Name of the action to apply.
            action_value: Optional value for parameterized actions.
            group_id: Flow group ID.

        Returns:
            List of FlowTest objects ready for execution.
        """
        action_spec = self.actions[action_name]

        wildcard_protocols = [name for name, test_fields in protocol_stack if not test_fields]
        field_test_protocols = [name for name, test_fields in protocol_stack if test_fields]
        all_protocol_names = [name for name, _ in protocol_stack]

        wildcard_pattern_parts = [self.protocols[name].pattern_name for name in wildcard_protocols]

        if not field_test_protocols:
            pattern = " / ".join(wildcard_pattern_parts)
            flow_rule = FlowRule(
                direction="ingress",
                pattern=[pattern],
                actions=[action_spec.build_action_string(action_value)],
                group_id=group_id,
            )
            packet = self._build_multi_layer_packet(all_protocol_names, {}, add_payload=True)

            return [
                FlowTest(
                    flow_rule=flow_rule,
                    packet=packet,
                    verification_type=action_spec.verification_type,
                    verification_params=action_spec.build_verification_params(action_value),
                    description=" / ".join(wildcard_pattern_parts) + f" -> {action_spec.name}",
                )
            ]

        protocol_field_specs = []
        for protocol_name in field_test_protocols:
            protocol_spec = self.protocols[protocol_name]
            protocol_field_specs.append([(protocol_spec, f) for f in protocol_spec.fields])

        test_cases = []

        for field_combo in product(*protocol_field_specs):
            max_vals = max(len(f_spec.test_parameters) for _, f_spec in field_combo)

            for i in range(max_vals):
                field_pattern_parts = []
                all_field_values: dict[str, dict[str, Any]] = {}
                desc_parts = []

                for protocol_spec, field_spec in field_combo:
                    val = field_spec.test_parameters[i % len(field_spec.test_parameters)]

                    field_pattern_parts.append(
                        f"{protocol_spec.pattern_name} {field_spec.pattern_field} is {val}"
                    )

                    if protocol_spec.name not in all_field_values:
                        all_field_values[protocol_spec.name] = {}
                    all_field_values[protocol_spec.name][field_spec.scapy_field] = val

                    desc_parts.append(f"{protocol_spec.name}[{field_spec.scapy_field}={val}]")

                full_pattern = " / ".join(wildcard_pattern_parts + field_pattern_parts)

                flow_rule = FlowRule(
                    direction="ingress",
                    pattern=[full_pattern],
                    actions=[action_spec.build_action_string(action_value)],
                    group_id=group_id,
                )

                packet = self._build_multi_layer_packet(
                    all_protocol_names, all_field_values, add_payload=True
                )

                wildcard_desc = " / ".join(wildcard_pattern_parts)
                field_desc = " / ".join(desc_parts)
                full_desc = f"{wildcard_desc} / {field_desc}" if wildcard_desc else field_desc

                test_cases.append(
                    FlowTest(
                        flow_rule=flow_rule,
                        packet=packet,
                        verification_type=action_spec.verification_type,
                        verification_params=action_spec.build_verification_params(action_value),
                        description=full_desc + f" -> {action_spec.name}",
                    )
                )

        return test_cases


@requires_nic_capability(NicCapability.FLOW_CTRL)
class TestRteFlow(TestSuite):
    """RTE Flow test suite.

    This suite consists of 4 test cases:
    1. Queue Action: Verifies queue actions with multi-protocol patterns
    2. Drop Action: Verifies drop actions with multi-protocol patterns
    3. Modify Field Action: Verifies modify_field actions with hardcoded patterns
    4. Jump Action: Verifies jump action between flow groups

    """

    def set_up_suite(self) -> None:
        """Initialize the test generator and result tracking."""
        self.generator = FlowTestGenerator(PROTOCOLS, ACTIONS)
        self.test_suite_results: list[FlowTestResult] = []
        self.test_case_results: list[FlowTestResult] = []

    def _get_modify_test_cases(self) -> list[ModifyTest]:
        """Build and return the hardcoded modify action test cases.

        Constructed at call time rather than module level to avoid
        Scapy packet construction during doc generation imports.
        """
        return [
            ModifyTest(
                description="ipv4[src=192.168.1.1] -> set_ipv4_dst",
                pattern="eth / ipv4 src is 192.168.1.1",
                action="set_ipv4_dst ipv4_addr 192.168.2.1 / queue index 0",
                packet=(
                    Ether() / IP(src="192.168.1.1", dst="10.0.0.1") / UDP() / Raw(load="X" * 5)
                ),
                expected_ipv4_dst="192.168.2.1",
            ),
            ModifyTest(
                description="ipv4[dst=192.168.1.2] -> set_ipv4_dst",
                pattern="eth / ipv4 dst is 192.168.1.2",
                action="set_ipv4_dst ipv4_addr 192.168.2.1 / queue index 0",
                packet=(
                    Ether() / IP(src="10.0.0.1", dst="192.168.1.2") / UDP() / Raw(load="X" * 5)
                ),
                expected_ipv4_dst="192.168.2.1",
            ),
            ModifyTest(
                description="ipv4[ttl=64] -> set_ipv4_dst",
                pattern="eth / ipv4 ttl is 64",
                action="set_ipv4_dst ipv4_addr 192.168.2.1 / queue index 0",
                packet=Ether() / IP(ttl=64) / UDP() / Raw(load="X" * 5),
                expected_ipv4_dst="192.168.2.1",
            ),
            ModifyTest(
                description="ipv4[ttl=128] -> set_ipv4_dst",
                pattern="eth / ipv4 ttl is 128",
                action="set_ipv4_dst ipv4_addr 192.168.2.1 / queue index 0",
                packet=Ether() / IP(ttl=128) / UDP() / Raw(load="X" * 5),
                expected_ipv4_dst="192.168.2.1",
            ),
            ModifyTest(
                description="ipv4[tos=0] -> set_ipv4_dst",
                pattern="eth / ipv4 tos is 0",
                action="set_ipv4_dst ipv4_addr 192.168.2.1 / queue index 0",
                packet=Ether() / IP(tos=0) / UDP() / Raw(load="X" * 5),
                expected_ipv4_dst="192.168.2.1",
            ),
            ModifyTest(
                description="ipv4[tos=4] -> set_ipv4_dst",
                pattern="eth / ipv4 tos is 4",
                action="set_ipv4_dst ipv4_addr 192.168.2.1 / queue index 0",
                packet=Ether() / IP(tos=4) / UDP() / Raw(load="X" * 5),
                expected_ipv4_dst="192.168.2.1",
            ),
            ModifyTest(
                description="eth[src=02:00:00:00:00:00] -> set_mac_dst",
                pattern="eth src is 02:00:00:00:00:00",
                action="set_mac_dst mac_addr 02:00:00:00:00:02 / queue index 0",
                packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:01") / Raw(load="X" * 5)
                ),
                expected_mac_dst="02:00:00:00:00:02",
            ),
            ModifyTest(
                description="eth[dst=02:00:00:00:00:02] -> set_mac_dst",
                pattern="eth dst is 02:00:00:00:00:02",
                action="set_mac_dst mac_addr 02:00:00:00:00:02 / queue index 0",
                packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02") / Raw(load="X" * 5)
                ),
                expected_mac_dst="02:00:00:00:00:02",
            ),
        ]

    def _get_jump_test_cases(self) -> list[JumpTest]:
        """Build and return the hardcoded jump action test cases.

        Constructed at call time rather than module level to avoid
        Scapy packet construction during doc generation imports.
        """
        return [
            JumpTest(
                description=(
                    "eth[src=02:00:00:00:00:00] -> jump -> ipv4[dst=192.168.1.2] -> queue 2"
                ),
                group0_pattern="eth src is 02:00:00:00:00:00",
                group1_pattern="ipv4 dst is 192.168.1.2",
                group1_action="queue index 2",
                matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="192.168.1.1", dst="192.168.1.2")
                    / UDP(sport=5000, dport=53)
                    / Raw(load="X" * 5)
                ),
                non_matching_packet=(
                    Ether(src="02:00:00:00:00:01", dst="02:00:00:00:00:02")
                    / IP(src="192.168.1.1", dst="192.168.1.2")
                    / UDP(sport=5000, dport=53)
                    / Raw(load="X" * 5)
                ),
                target_queue=2,
            ),
            JumpTest(
                description=(
                    "eth[dst=02:00:00:00:00:02] -> jump -> ipv4[src=192.168.1.1] -> queue 3"
                ),
                group0_pattern="eth dst is 02:00:00:00:00:02",
                group1_pattern="ipv4 src is 192.168.1.1",
                group1_action="queue index 3",
                matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="192.168.1.1", dst="192.168.1.2")
                    / UDP(sport=5000, dport=53)
                    / Raw(load="X" * 5)
                ),
                non_matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:03")
                    / IP(src="192.168.1.1", dst="192.168.1.2")
                    / UDP(sport=5000, dport=53)
                    / Raw(load="X" * 5)
                ),
                target_queue=3,
            ),
            JumpTest(
                description="ipv4[src=10.0.0.1] -> jump -> tcp[dst=443] -> queue 1",
                group0_pattern="eth / ipv4 src is 10.0.0.1",
                group1_pattern="ipv4 / tcp dst is 443",
                group1_action="queue index 1",
                matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="10.0.0.1", dst="10.0.0.2")
                    / TCP(sport=12345, dport=443)
                    / Raw(load="X" * 5)
                ),
                non_matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="10.0.0.99", dst="10.0.0.2")
                    / TCP(sport=12345, dport=443)
                    / Raw(load="X" * 5)
                ),
                target_queue=1,
            ),
            JumpTest(
                description="ipv4[dst=172.16.0.1] -> jump -> udp[dst=123] -> queue 2",
                group0_pattern="eth / ipv4 dst is 172.16.0.1",
                group1_pattern="ipv4 / udp dst is 123",
                group1_action="queue index 2",
                matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="172.16.0.100", dst="172.16.0.1")
                    / UDP(sport=5000, dport=123)
                    / Raw(load="X" * 5)
                ),
                non_matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="172.16.0.100", dst="172.16.0.99")
                    / UDP(sport=5000, dport=123)
                    / Raw(load="X" * 5)
                ),
                target_queue=2,
            ),
            JumpTest(
                description="eth[src=02:00:00:00:00:00] -> jump -> udp[dst=53] -> queue 3",
                group0_pattern="eth src is 02:00:00:00:00:00",
                group1_pattern="ipv4 / udp dst is 53",
                group1_action="queue index 3",
                matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="192.168.1.1", dst="192.168.1.2")
                    / UDP(sport=5000, dport=53)
                    / Raw(load="X" * 5)
                ),
                non_matching_packet=(
                    Ether(src="02:00:00:00:00:01", dst="02:00:00:00:00:02")
                    / IP(src="192.168.1.1", dst="192.168.1.2")
                    / UDP(sport=5000, dport=53)
                    / Raw(load="X" * 5)
                ),
                target_queue=3,
            ),
            JumpTest(
                description="ipv4[tos=4] -> jump -> tcp[dst=80] -> queue 1",
                group0_pattern="eth / ipv4 tos is 4",
                group1_pattern="ipv4 / tcp dst is 80",
                group1_action="queue index 1",
                matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="10.0.0.1", dst="10.0.0.2", tos=4)
                    / TCP(sport=54321, dport=80)
                    / Raw(load="X" * 5)
                ),
                non_matching_packet=(
                    Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:02")
                    / IP(src="10.0.0.1", dst="10.0.0.2", tos=0)
                    / TCP(sport=54321, dport=80)
                    / Raw(load="X" * 5)
                ),
                target_queue=1,
            ),
        ]

    def _validate_and_create_flow(
        self,
        rule: FlowRule,
        description: str,
        testpmd: TestPmd,
        port_id: int = 0,
        sent_packet: Optional["Packet"] = None,
    ) -> int | None:
        """Validate and create a flow rule, recording results.

        Args:
            rule: The flow rule to validate and create.
            description: Description for the test result.
            testpmd: Active TestPmd session.
            port_id: Port ID for flow operations.
            sent_packet: Optional packet associated with this test.

        Returns:
            Flow ID if successful, None if validation or creation failed.
        """
        result = FlowTestResult(
            description=description,
            passed=False,
            flow_rule_pattern=" / ".join(rule.pattern),
            sent_packet=sent_packet,
        )

        is_valid = testpmd.flow_validate(flow_rule=rule, port_id=port_id)
        if not is_valid:
            result.skipped = True
            result.failure_reason = "Flow rule failed validation"
            self.test_suite_results.append(result)
            self.test_case_results.append(result)
            return None

        try:
            flow_id = testpmd.flow_create(flow_rule=rule, port_id=port_id)
        except InteractiveCommandExecutionError:
            result.failure_reason = "Hardware validated but failed to create flow rule"
            self.test_suite_results.append(result)
            self.test_case_results.append(result)
            return None

        result.passed = True
        self.test_suite_results.append(result)
        self.test_case_results.append(result)
        return flow_id

    def _record_failure(self, reason: str) -> None:
        """Record a verification failure in both result lists."""
        self.test_suite_results[-1].passed = False
        self.test_suite_results[-1].failure_reason = reason
        self.test_case_results[-1].passed = False
        self.test_case_results[-1].failure_reason = reason

    def _record_skip(self, reason: str) -> None:
        """Record a skipped test in both result lists."""
        self.test_suite_results[-1].passed = False
        self.test_suite_results[-1].skipped = True
        self.test_suite_results[-1].failure_reason = f"Skipped: {reason}"
        self.test_case_results[-1].passed = False
        self.test_case_results[-1].skipped = True
        self.test_case_results[-1].failure_reason = f"Skipped: {reason}"

    def _verify_basic_transmission(self, action_type: str) -> None:
        """Verify that non-matching packets are unaffected by flow rules.

        Creates a flow rule for the specified action, then sends a packet that
        should NOT match the rule to confirm:
        - For 'drop': non-matching packets ARE received (not dropped)
        - For 'queue': non-matching packets are NOT steered to the target queue
        - For 'modify': non-matching packets arrive unmodified

        This ensures flow rules only affect matching traffic before
        running the actual action tests.

        Args:
            action_type: The action being tested ('drop', 'queue', 'modify').
        """
        non_matching_packet = (
            Ether(src="02:00:00:00:00:00", dst="02:00:00:00:00:01")
            / IP(src="192.168.100.1", dst="192.168.100.2")
            / UDP(sport=9999, dport=9998)
            / Raw(load="X" * 5)
        )

        with TestPmd(rx_queues=4, tx_queues=4) as testpmd:
            if action_type == "drop":
                rule = FlowRule(
                    direction="ingress",
                    pattern=["eth / ipv4 src is 192.168.1.1 / udp dst is 53"],
                    actions=["drop"],
                )
                flow_id = self._validate_and_create_flow(
                    rule,
                    f"Basic transmission check ({action_type})",
                    testpmd,
                    sent_packet=non_matching_packet,
                )
                if flow_id is None:
                    return

                testpmd.start()
                received = send_packet_and_capture(non_matching_packet)
                testpmd.stop()
                contains_packet = any(
                    p.haslayer(Raw) and b"XXXXX" in bytes(p[Raw].load) for p in received
                )
                testpmd.flow_delete(flow_id, port_id=0)
                verify(
                    contains_packet,
                    "Basic transmission check failed: non-matching packet dropped",
                )

            elif action_type == "queue":
                rule = FlowRule(
                    direction="ingress",
                    pattern=[
                        "eth src is aa:bb:cc:dd:ee:ff / ipv4 src is 10.255.255.254 "
                        "dst is 10.255.255.253 / udp src is 12345 dst is 54321"
                    ],
                    actions=["queue index 3"],
                )
                flow_id = self._validate_and_create_flow(
                    rule,
                    f"Basic transmission check ({action_type})",
                    testpmd,
                    sent_packet=non_matching_packet,
                )
                if flow_id is None:
                    return

                testpmd.set_verbose(level=8)
                testpmd.start()
                send_packet_and_capture(non_matching_packet)
                verbose_output = testpmd.extract_verbose_output(testpmd.stop())
                received_on_target = any(p.queue_id == 3 for p in verbose_output)
                testpmd.flow_delete(flow_id, port_id=0)
                verify(
                    not received_on_target,
                    "Basic transmission check failed: non-matching packet steered to queue 3",
                )

            elif action_type == "modify":
                rule = FlowRule(
                    direction="ingress",
                    pattern=["eth / ipv4 src is 192.168.1.1 / udp dst is 53"],
                    actions=["set_ipv4_dst ipv4_addr 192.168.2.1 / queue index 0"],
                )
                flow_id = self._validate_and_create_flow(
                    rule,
                    f"Basic transmission check ({action_type})",
                    testpmd,
                    sent_packet=non_matching_packet,
                )
                if flow_id is None:
                    return

                testpmd.start()
                received = send_packet_and_capture(non_matching_packet)
                testpmd.stop()
                testpmd.flow_delete(flow_id, port_id=0)

                test_packet = next(
                    (p for p in received if p.haslayer(Raw) and b"XXXXX" in bytes(p[Raw].load)),
                    None,
                )
                if (
                    test_packet is not None
                    and non_matching_packet is not None
                    and IP in test_packet
                    and IP in non_matching_packet
                ):
                    verify(
                        test_packet[IP].dst == non_matching_packet[IP].dst,
                        "Basic transmission check failed: non-matching packet was modified",
                    )

        log(f"Basic transmission check passed for '{action_type}' action")

    def _verify_queue(self, packet: Packet, queue_id: int, testpmd: TestPmd, **kwargs: Any) -> None:
        """Verify packet is received on the expected queue."""
        send_packet_and_capture(packet)
        verbose_output = testpmd.extract_verbose_output(testpmd.stop())
        received_on_queue = any(p.queue_id == queue_id for p in verbose_output)
        verify(received_on_queue, f"Packet not received on queue {queue_id}")

    def _verify_drop(self, packet: Packet, **kwargs: Any) -> None:
        """Verify packet is dropped."""
        received = send_packet_and_capture(packet)
        contains_packet = any(p.haslayer(Raw) and b"XXXXX" in bytes(p[Raw].load) for p in received)
        verify(not contains_packet, "Packet was not dropped")

    def _run_tests(
        self,
        test_cases: list[FlowTest],
        port_id: int = 0,
    ) -> None:
        """Execute a sequence of generated test cases."""
        with TestPmd(rx_queues=4, tx_queues=4) as testpmd:
            for test_case in test_cases:
                log(f"Testing: {test_case.description}")

                flow_id = self._validate_and_create_flow(
                    test_case.flow_rule,
                    test_case.description,
                    testpmd,
                    port_id,
                    sent_packet=test_case.packet,
                )
                if flow_id is None:
                    continue

                try:
                    if test_case.verification_type == "queue":
                        testpmd.set_verbose(level=8)

                    testpmd.start()

                    verification_method = getattr(self, f"_verify_{test_case.verification_type}")
                    verification_method(
                        packet=test_case.packet,
                        testpmd=testpmd,
                        **test_case.verification_params,
                    )

                    testpmd.stop()

                except SkippedTestException as e:
                    self._record_skip(str(e))

                except TestCaseVerifyError as e:
                    self._record_failure(str(e))

                finally:
                    testpmd.flow_delete(flow_id, port_id=port_id)

    def _log_test_suite_summary(self) -> None:
        """Log a summary of all test results."""
        if not self.test_suite_results:
            return

        passed_rules = [r for r in self.test_suite_results if r.passed]
        skipped_rules = [r for r in self.test_suite_results if r.skipped]
        failed_rules = [r for r in self.test_suite_results if not r.passed and not r.skipped]

        log(f"Total tests run: {len(self.test_suite_results)}")
        log(f"Passed: {len(passed_rules)}")
        log(f"Skipped: {len(skipped_rules)}")
        log(f"Failed: {len(failed_rules)}")

        if passed_rules:
            log("\nPASSED RULES:")
            for result in passed_rules:
                log(f"  {result.description}")
                log(f"    Sent Packet: {result.sent_packet}")

        if skipped_rules:
            log("\nSKIPPED RULES:")
            for result in skipped_rules:
                log(f"  {result.description}")
                log(f"    Pattern: {result.flow_rule_pattern}")
                log(f"    Reason: {result.failure_reason}")
                log(f"    Sent Packet: {result.sent_packet}")

        if failed_rules:
            log("\nFAILED RULES:")
            for result in failed_rules:
                log(f"  {result.description}")
                log(f"    Pattern: {result.flow_rule_pattern}")
                log(f"    Reason: {result.failure_reason}")
                log(f"    Sent Packet: {result.sent_packet}")

    def _check_test_case_failures(self) -> None:
        """Fail the test case if any flow rules failed creation or verification."""
        failures = [r for r in self.test_case_results if not r.passed and not r.skipped]
        self.test_case_results = []

        if failures:
            details = "\n".join(
                f"\t{r.description}\n"
                f"\t  Pattern: {r.flow_rule_pattern}\n"
                f"\t  Reason: {r.failure_reason}\n"
                f"\t  Sent Packet: {r.sent_packet}"
                for r in failures
            )
            fail(f"Flow rules failed:\n{details}")

    # -------------------- Generator Test Cases --------------------

    @func_test
    def queue_action(self) -> None:
        """Validate flow rules with queue actions and multi-protocol patterns.

        Steps:
            * Run basic transmission check to verify baseline packet reception.
            * Create a list of packets to test, with a corresponding flow list.
            * Launch testpmd.
            * Create first flow rule in flow list.
            * Send first packet in packet list, capture verbose output.
            * Delete flow rule, repeat for all flows/packets.

        Verify:
            * Each packet is received on the appropriate queue.
        """
        self._verify_basic_transmission("queue")
        for stack in PROTOCOL_STACKS:
            test_cases = self.generator.generate(
                protocol_stack=stack,
                action_name="queue",
                action_value=2,
            )
            self._run_tests(test_cases)
        self._check_test_case_failures()

    @func_test
    def drop_action(self) -> None:
        """Validate flow rules with drop actions and multi-protocol patterns.

        Steps:
            * Run basic transmission check to verify packets are received without drop rules.
            * Create a list of packets to test, with a corresponding flow list.
            * Launch testpmd.
            * Create first flow rule in flow list.
            * Send first packet in packet list, capture verbose output.
            * Delete flow rule, repeat for all flows/packets.

        Verify:
            * Packet is dropped.
        """
        self._verify_basic_transmission("drop")
        for stack in PROTOCOL_STACKS:
            test_cases = self.generator.generate(
                protocol_stack=stack,
                action_name="drop",
            )
            self._run_tests(test_cases)
        self._check_test_case_failures()

    # -------------------- Hard Coded Test Cases --------------------

    @func_test
    def modify_field_action(self) -> None:
        """Validate flow rules with modify_field actions and hardcoded patterns.

        Steps:
            * Run basic transmission check to verify packets arrive unmodified.
            * For each test case in MODIFY_TEST_CASES, create the flow rule.
            * Send the corresponding packet and capture the received packet.
            * Verify the expected field was modified correctly.
            * Delete the flow rule and repeat for all test cases.

        Verify:
            * Each packet is modified correctly according to its action.
        """
        port_id = 0
        self._verify_basic_transmission("modify")

        with TestPmd(rx_queues=4, tx_queues=4) as testpmd:
            for test_case in self._get_modify_test_cases():
                log(f"Testing: {test_case.description}")

                rule = FlowRule(
                    direction="ingress",
                    pattern=[test_case.pattern],
                    actions=[test_case.action],
                )

                flow_id = self._validate_and_create_flow(
                    rule,
                    test_case.description,
                    testpmd,
                    port_id,
                    sent_packet=test_case.packet,
                )
                if flow_id is None:
                    continue

                try:
                    testpmd.start()
                    received = send_packet_and_capture(test_case.packet)
                    testpmd.stop()

                    test_packet = next(
                        (p for p in received if p.haslayer(Raw) and b"XXXXX" in bytes(p[Raw].load)),
                        None,
                    )

                    verify(
                        test_packet is not None,
                        f"{test_case.description}: Packet not received",
                    )

                    if (
                        test_packet
                        and test_case.expected_ipv4_dst is not None
                        and IP in test_packet
                    ):
                        verify(
                            test_packet[IP].dst == test_case.expected_ipv4_dst,
                            f"{test_case.description}: IPv4 dst mismatch: "
                            f"expected {test_case.expected_ipv4_dst}, "
                            f"got {test_packet[IP].dst}",
                        )
                    if (
                        test_packet
                        and test_case.expected_mac_dst is not None
                        and Ether in test_packet
                    ):
                        verify(
                            test_packet[Ether].dst == test_case.expected_mac_dst,
                            f"{test_case.description}: MAC dst mismatch: "
                            f"expected {test_case.expected_mac_dst}, "
                            f"got {test_packet[Ether].dst}",
                        )

                except SkippedTestException as e:
                    self._record_skip(str(e))

                except TestCaseVerifyError as e:
                    self._record_failure(str(e))

                finally:
                    testpmd.flow_delete(flow_id, port_id=port_id)

        self._check_test_case_failures()

    @func_test
    def jump_action(self) -> None:
        """Validate flow rules with jump action between groups.

        The jump action redirects matched packets from one flow group to another.
        Only flow rules in group 0 are guaranteed to be matched against initially;
        subsequent groups can only be reached via jump actions.

        For each test case in JUMP_TEST_CASES, this creates a two-stage pipeline:
        - Group 0: Match on a pattern, jump to group 1
        - Group 1: Match on a second pattern, forward to a specific queue

        Steps:
            * Launch testpmd with multiple queues.
            * For each test case, create group 0 rule (match + jump) and group 1 rule
              (match + queue).
            * Send matching packet and verify it arrives on the target queue.
            * Send non-matching packet and verify it does not reach the target queue.
            * Delete both flow rules and repeat for all test cases.

        Verify:
            * Packet matching both rules is received on the target queue.
            * Packet not matching group 0 rule does not reach the target queue.
        """
        port_id = 0

        with TestPmd(rx_queues=4, tx_queues=4) as testpmd:
            for test_case in self._get_jump_test_cases():
                log(f"Testing: {test_case.description}")

                jump_rule = FlowRule(
                    direction="ingress",
                    group_id=0,
                    pattern=[test_case.group0_pattern],
                    actions=["jump group 1"],
                )

                queue_rule = FlowRule(
                    direction="ingress",
                    group_id=1,
                    pattern=[test_case.group1_pattern],
                    actions=[test_case.group1_action],
                )

                jump_flow_id = self._validate_and_create_flow(
                    jump_rule,
                    f"{test_case.description} (group 0 -> jump)",
                    testpmd,
                    port_id,
                    sent_packet=test_case.matching_packet,
                )
                if jump_flow_id is None:
                    continue

                queue_flow_id = self._validate_and_create_flow(
                    queue_rule,
                    f"{test_case.description} (group 1 -> queue)",
                    testpmd,
                    port_id,
                    sent_packet=test_case.matching_packet,
                )
                if queue_flow_id is None:
                    testpmd.flow_delete(jump_flow_id, port_id=port_id)
                    continue

                try:
                    testpmd.set_verbose(level=8)
                    testpmd.start()

                    send_packet_and_capture(test_case.matching_packet)
                    verbose_output = testpmd.extract_verbose_output(testpmd.stop())
                    received_on_target = any(
                        p.queue_id == test_case.target_queue for p in verbose_output
                    )
                    verify(
                        received_on_target,
                        f"{test_case.description}: Matching packet not received on "
                        f"queue {test_case.target_queue} after jump",
                    )

                    testpmd.start()
                    send_packet_and_capture(test_case.non_matching_packet)
                    verbose_output = testpmd.extract_verbose_output(testpmd.stop())
                    non_matching_on_target = any(
                        p.queue_id == test_case.target_queue for p in verbose_output
                    )
                    verify(
                        not non_matching_on_target,
                        f"{test_case.description}: Non-matching packet incorrectly "
                        f"received on queue {test_case.target_queue}",
                    )

                except SkippedTestException as e:
                    self._record_skip(str(e))

                except TestCaseVerifyError as e:
                    self._record_failure(str(e))

                finally:
                    testpmd.flow_delete(queue_flow_id, port_id=port_id)
                    testpmd.flow_delete(jump_flow_id, port_id=port_id)

        self._check_test_case_failures()

    def tear_down_suite(self) -> None:
        """Log test summary at the end of the suite."""
        self._log_test_suite_summary()
