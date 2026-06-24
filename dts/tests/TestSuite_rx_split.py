# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 NVIDIA Corporation & Affiliates

"""Rx split test suite.

Test configuring a packet split on Rx,
and discarding some segments (selective Rx) at NIC level.
"""

from collections.abc import Callable
from typing import Any

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Packet, Raw

from api.capabilities import (
    NicCapability,
    requires_nic_capability,
)
from api.packet import adjust_addresses, send_packet_and_capture
from api.test import fail, verify
from api.testpmd import TestPmd
from api.testpmd.config import SimpleForwardingModes
from api.testpmd.types import RxOffloadCapability, TxOffloadCapability
from framework.exception import InteractiveCommandExecutionError
from framework.test_suite import TestSuite, func_test

PAYLOAD = bytes(range(256))
ETHER_HDR_LEN = len(Ether(dst="00:00:00:00:00:00"))
IP_HDR_LEN = len(IP())
ETHER_IP_HDR_LEN = ETHER_HDR_LEN + IP_HDR_LEN
ETHER_MIN_FRAME_LEN = 60


@requires_nic_capability(NicCapability.PORT_RX_OFFLOAD_BUFFER_SPLIT)
@requires_nic_capability(NicCapability.SELECTIVE_RX)
class TestRxSplit(TestSuite):
    """Rx split test suite.

    Configure testpmd with various Rx segment offset/length combinations
    and verify that only the requested portions of the packet are received
    and forwarded.
    """

    def _create_testpmd(self, **kwargs: Any) -> TestPmd:
        """Create a TestPmd instance with defaults overridden by kwargs."""
        defaults: dict[str, Any] = {
            "forward_mode": SimpleForwardingModes.io,
            "rx_offloads": RxOffloadCapability.BUFFER_SPLIT | RxOffloadCapability.SCATTER,
        }
        return TestPmd(**{**defaults, **kwargs})

    def _build_packet(self) -> Packet:
        """Build a test packet with an incrementing byte pattern payload."""
        packet = Ether() / IP() / Raw(load=PAYLOAD)
        return adjust_addresses([packet])[0]

    def _start_and_verify(
        self,
        testpmd: TestPmd,
        expected: Callable[[bytes], bytes] | bytes | int | None = None,
    ) -> None:
        """Start testpmd, send the default packet, and verify received bytes."""
        testpmd.start()
        packet = self._build_packet()
        raw = bytes(packet)
        if expected is None:
            raw_expected = raw
        elif isinstance(expected, int):
            raw_expected = raw[:expected]
        elif isinstance(expected, bytes):
            raw_expected = expected
        else:
            raw_expected = expected(raw)
        self._send_and_verify(testpmd, packet, raw_expected)

    def _send_and_verify(self, testpmd: TestPmd, tg_packet: Packet, expected: bytes) -> None:
        """Clear stats, send a packet, and verify received content and stats.

        Args:
            testpmd: The running testpmd instance.
            tg_packet: The packet to send by Scapy on the TG.
            expected: Expected raw bytes received by testpmd on the SUT.
        """
        testpmd.clear_port_stats_all(verify=False)

        # TG send, SUT receive and forward back, then TG capture
        sut_len = len(expected)
        capture_len = max(sut_len, ETHER_MIN_FRAME_LEN)
        received = send_packet_and_capture(tg_packet)
        verify(
            len(received) > 0,
            "Did not receive any packets.",
        )

        recv_bytes = bytes(received[0])
        verify(
            len(recv_bytes) == capture_len,
            f"Expected packet length {capture_len}, got {len(recv_bytes)}.",
        )
        verify(
            recv_bytes[:sut_len] == expected,
            "Received packet content does not match expected bytes.",
        )

        all_stats, _ = testpmd.show_port_stats_all()
        total_rx_packets = sum(s.rx_packets for s in all_stats)
        total_rx_bytes = sum(s.rx_bytes for s in all_stats)
        verify(
            total_rx_packets == 1,
            f"Expected 1 Rx packet, got {total_rx_packets}.",
        )
        verify(
            total_rx_bytes == sut_len,
            f"Expected {sut_len} Rx bytes, got {total_rx_bytes}.",
        )

    def _verify_port_start_fails(self, testpmd: TestPmd, message: str) -> None:
        """Verify that starting ports fails, then drain testpmd output."""
        try:
            testpmd.start_all_ports()
            fail(message)
        except InteractiveCommandExecutionError:
            testpmd.stop(verify=False)

    @func_test
    def selective_rx_headers(self) -> None:
        """Keep only the Ethernet + IP headers, discard the rest.

        Steps:
            Start testpmd with rxpkts, mbuf-size and buffer split enabled.
            Configure the payload discard segment with length 0 (rest).
            Send an Ether/IP/payload packet.

        Verify:
            Received packet has Ether + IP headers only.
            Port stats show expected rx_packets and rx_bytes.
        """
        with self._create_testpmd(
            rx_segments_length=[ETHER_IP_HDR_LEN, 0],
            mbuf_size=[256, 0],
        ) as testpmd:
            self._start_and_verify(testpmd, ETHER_IP_HDR_LEN)

    @func_test
    def selective_rx_headers_discard_length(self) -> None:
        """Keep only the Ethernet + IP headers, discard the remaining length.

        Steps:
            Start testpmd with rxpkts, mbuf-size and buffer split enabled.
            Configure the payload discard segment with an explicit length.
            Send an Ether/IP/payload packet.

        Verify:
            Received packet has Ether + IP headers only.
            Port stats show expected rx_packets and rx_bytes.
        """
        with self._create_testpmd(
            rx_segments_length=[ETHER_IP_HDR_LEN, len(PAYLOAD)],
            mbuf_size=[256, 0],
        ) as testpmd:
            self._start_and_verify(testpmd, ETHER_IP_HDR_LEN)

    @func_test
    def selective_rx_payload_only(self) -> None:
        """Skip the Ethernet + IP headers, keep only the payload.

        Steps:
            Start testpmd with rxpkts, mbuf-size and buffer split enabled.
            Send an Ether/IP/payload packet.

        Verify:
            Received packet is matching the original payload.
            Port stats show expected rx_packets and rx_bytes.
        """
        with self._create_testpmd(
            rx_segments_length=[ETHER_IP_HDR_LEN, len(PAYLOAD)],
            mbuf_size=[0, 512],
        ) as testpmd:
            self._start_and_verify(testpmd, PAYLOAD)

    @func_test
    def selective_rx_two_segments(self) -> None:
        """Keep the IP header and the middle of the payload, skip the rest.

        Steps:
            Start testpmd with rxpkts, mbuf-size, buffer split
            and multi-segment Tx enabled.
            Send an Ether/IP/payload packet.

        Verify:
            Received packet is matching the IP header and middle of payload.
            Port stats show expected rx_packets and rx_bytes.
        """
        payload_offset = 100
        payload_length = 100
        with self._create_testpmd(
            tx_offloads=TxOffloadCapability.MULTI_SEGS,
            rx_segments_length=[ETHER_HDR_LEN, IP_HDR_LEN, payload_offset, payload_length, 0],
            mbuf_size=[0, 256, 0, 256, 0],
        ) as testpmd:

            def expected(packet: bytes) -> bytes:
                payload_start = ETHER_IP_HDR_LEN + payload_offset
                return (
                    packet[ETHER_HDR_LEN:ETHER_IP_HDR_LEN]
                    + packet[payload_start : payload_start + payload_length]
                )

            self._start_and_verify(testpmd, expected)

    @func_test
    def selective_rx_runtime_config(self) -> None:
        """Configure selective Rx at runtime after initial startup.

        Steps:
            Start testpmd with two mbuf-size pools but no rxpkts/rxhdrs.
            Stop ports, configure buffer split offload, set rxpkts, restart ports.
            Send an Ether/IP/payload packet.

        Verify:
            Initial startup succeeds without error.
            Received packet has Ether + IP headers only after runtime config.
            Port stats show expected rx_packets and rx_bytes.
        """
        with self._create_testpmd(
            mbuf_size=[512, 0],
        ) as testpmd:
            self._start_and_verify(testpmd)
            testpmd.stop()
            testpmd.stop_all_ports()
            testpmd.send_command("port config 0 rx_offload buffer_split on")
            testpmd.send_command(f"set rxpkts {ETHER_IP_HDR_LEN},0")
            self._start_and_verify(testpmd, ETHER_IP_HDR_LEN)

    @func_test
    def selective_rx_no_offload(self) -> None:
        """Configure selective Rx with buffer split disabled.

        Steps:
            Start testpmd with rxpkts, mbuf-size including a discard segment,
            buffer split disabled, and device start disabled.
            Attempt to start ports.

        Verify:
            Queue configuration fails.
        """
        with self._create_testpmd(
            rx_offloads=None,
            rx_segments_length=[ETHER_IP_HDR_LEN, 0],
            mbuf_size=[256, 0],
            disable_device_start=True,
        ) as testpmd:
            self._verify_port_start_fails(
                testpmd,
                "Expected configuration to fail with buffer split disabled.",
            )

    @func_test
    def selective_rx_segment_exceeds_mbuf(self) -> None:
        """Configure selective Rx with segment length exceeding mbuf capacity.

        Steps:
            Start testpmd with rxpkts larger than mbuf-size,
            buffer split enabled, and device start disabled.
            Attempt to start ports.

        Verify:
            Queue configuration fails.
        """
        with self._create_testpmd(
            rx_segments_length=[4096, 0],
            mbuf_size=[128, 0],
            disable_device_start=True,
        ) as testpmd:
            self._verify_port_start_fails(
                testpmd,
                "Expected configuration to fail with segment > mbuf size.",
            )

    @func_test
    def selective_rx_all_discard(self) -> None:
        """Configure selective Rx with only discard segment.

        Steps:
            Start testpmd with only discard segment,
            buffer split enabled, and device start disabled.
            Attempt to start ports.

        Verify:
            Queue configuration fails.
        """
        with self._create_testpmd(
            rx_segments_length=[0],
            mbuf_size=[0],
            disable_device_start=True,
        ) as testpmd:
            self._verify_port_start_fails(
                testpmd,
                "Expected configuration to fail with only discard segment.",
            )
