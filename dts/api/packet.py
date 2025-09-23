# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Packet utilities for test suites.

The module provides helpers for:
    * Packet sending and verification,
    * Packet adjustments and modification.

Example:
    .. code:: python

        from scapy.layers.inet import IP
        from scapy.layers.l2 import Ether
        from api.packet import send_packet_and_capture, get_expected_packet, match_all_packets

        pkt = Ether()/IP()/b"payload"
        received = send_packet_and_capture(pkt)
        expected = get_expected_packet(pkt)
        match_all_packets([expected], received)
"""

from collections import Counter
from typing import cast

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Packet, Padding, raw

from api.test import fail, log_debug
from framework.context import get_ctx
from framework.exception import InternalError
from framework.testbed_model.traffic_generator.capturing_traffic_generator import (
    PacketFilteringConfig,
)
from framework.utils import get_packet_summaries


def send_packet_and_capture(
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
    return send_packets_and_capture(
        [packet],
        filter_config,
        duration,
    )


def send_packets_and_capture(
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
    from framework.context import get_ctx
    from framework.testbed_model.traffic_generator.capturing_traffic_generator import (
        CapturingTrafficGenerator,
    )

    assert isinstance(
        get_ctx().tg, CapturingTrafficGenerator
    ), "Cannot capture with a non-capturing traffic generator"
    tg: CapturingTrafficGenerator = cast(CapturingTrafficGenerator, get_ctx().tg)
    # TODO: implement @requires for types of traffic generator
    packets = adjust_addresses(packets)
    return tg.send_packets_and_capture(
        packets,
        get_ctx().topology.tg_port_egress,
        get_ctx().topology.tg_port_ingress,
        filter_config,
        duration,
    )


def send_packets(
    packets: list[Packet],
) -> None:
    """Send packets using the traffic generator and do not capture received traffic.

    Args:
        packets: Packets to send.
    """
    packets = adjust_addresses(packets)
    get_ctx().tg.send_packets(packets, get_ctx().topology.tg_port_egress)


def get_expected_packets(
    packets: list[Packet],
    sent_from_tg: bool = False,
) -> list[Packet]:
    """Inject the proper L2/L3 addresses into `packets`.

    Inject the L2/L3 addresses expected at the receiving end of the traffic generator.

    Args:
        packets: The packets to modify.
        sent_from_tg: If :data:`True` packet was sent from the TG.

    Returns:
        `packets` with injected L2/L3 addresses.
    """
    return adjust_addresses(packets, not sent_from_tg)


def get_expected_packet(
    packet: Packet,
    sent_from_tg: bool = False,
) -> Packet:
    """Inject the proper L2/L3 addresses into `packet`.

    Inject the L2/L3 addresses expected at the receiving end of the traffic generator.

    Args:
        packet: The packet to modify.
        sent_from_tg: If :data:`True` packet was sent from the TG.

    Returns:
        `packet` with injected L2/L3 addresses.
    """
    return get_expected_packets([packet], sent_from_tg)[0]


def adjust_addresses(packets: list[Packet], expected: bool = False) -> list[Packet]:
    """L2 and L3 address additions in both directions.

    Copies of `packets` will be made, modified and returned in this method.

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

    Raises:
        InternalError: If no tests are running.
    """
    from framework.test_suite import TestSuite

    if get_ctx().local.current_test_suite is None:
        raise InternalError("No current test suite, tests aren't running?")
    current_test_suite: TestSuite = cast(TestSuite, get_ctx().local.current_test_suite)
    return current_test_suite._adjust_addresses(packets, expected)


def match_all_packets(
    expected_packets: list[Packet],
    received_packets: list[Packet],
    verify: bool = True,
) -> bool:
    """Matches all the expected packets against the received ones.

    Matching is performed by counting down the occurrences in a dictionary which keys are the
    raw packet bytes. No deep packet comparison is performed. All the unexpected packets (noise)
    are automatically ignored.

    Args:
        expected_packets: The packets we are expecting to receive.
        received_packets: All the packets that were received.
        verify: If :data:`True`, and there are missing packets an exception will be raised.

    Raises:
        TestCaseVerifyError: if and not all the `expected_packets` were found in
            `received_packets`.

    Returns:
        :data:`True` If there are no missing packets.
    """
    expected_packets_counters = Counter(map(raw, expected_packets))
    received_packets_counters = Counter(map(raw, received_packets))
    # The number of expected packets is subtracted by the number of received packets, ignoring
    # any unexpected packets and capping at zero.
    missing_packets_counters = expected_packets_counters - received_packets_counters
    missing_packets_count = missing_packets_counters.total()
    log_debug(
        f"match_all_packets: expected {len(expected_packets)}, "
        f"received {len(received_packets)}, missing {missing_packets_count}"
    )

    if missing_packets_count != 0:
        if verify:
            fail(
                f"Not all packets were received, expected {len(expected_packets)} "
                f"but {missing_packets_count} were missing."
            )
        return False

    return True


def verify_packets(expected_packet: Packet, received_packets: list[Packet]) -> None:
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
        if _compare_packets(expected_packet, received_packet):
            break
    else:
        log_debug(
            f"The expected packet {expected_packet.summary()} "
            f"not found among received {get_packet_summaries(received_packets)}"
        )
        fail("An expected packet not found among received packets.")


def _compare_packets(expected_packet: Packet, received_packet: Packet) -> bool:
    log_debug(f"Comparing packets: \n{expected_packet.summary()}\n{received_packet.summary()}")

    l3 = IP in expected_packet.layers()
    log_debug("Found l3 layer")

    received_payload = received_packet
    expected_payload = expected_packet
    while received_payload and expected_payload:
        log_debug("Comparing payloads:")
        log_debug(f"Received: {received_payload}")
        log_debug(f"Expected: {expected_payload}")
        if type(received_payload) is type(expected_payload):
            log_debug("The layers are the same.")
            if type(received_payload) is Ether:
                if not _verify_l2_frame(received_payload, l3):
                    return False
            elif type(received_payload) is IP:
                assert type(expected_payload) is IP
                if not _verify_l3_packet(received_payload, expected_payload):
                    return False
        else:
            # Different layers => different packets
            return False
        received_payload = received_payload.payload
        expected_payload = expected_payload.payload

    if expected_payload:
        log_debug(f"The expected packet did not contain {expected_payload}.")
        return False
    if received_payload and received_payload.__class__ != Padding:
        log_debug("The received payload had extra layers which were not padding.")
        return False
    return True


def _verify_l2_frame(received_packet: Ether, contains_l3: bool) -> bool:
    """Verify the L2 frame of `received_packet`.

    Args:
        received_packet: The received L2 frame to verify.
        contains_l3: If :data:`True`, the packet contains an L3 layer.
    """
    log_debug("Looking at the Ether layer.")
    log_debug(
        f"Comparing received dst mac '{received_packet.dst}' "
        f"with expected '{get_ctx().topology.tg_port_ingress.mac_address}'."
    )
    if received_packet.dst != get_ctx().topology.tg_port_ingress.mac_address:
        return False

    expected_src_mac = get_ctx().topology.tg_port_egress.mac_address
    if contains_l3:
        expected_src_mac = get_ctx().topology.sut_port_egress.mac_address
    log_debug(
        f"Comparing received src mac '{received_packet.src}' "
        f"with expected '{expected_src_mac}'."
    )
    if received_packet.src != expected_src_mac:
        return False

    return True


def _verify_l3_packet(received_packet: IP, expected_packet: IP) -> bool:
    log_debug("Looking at the IP layer.")
    if received_packet.src != expected_packet.src or received_packet.dst != expected_packet.dst:
        return False
    return True
