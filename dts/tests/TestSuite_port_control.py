# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire
"""Port Control Testing Suite.

This test suite serves to show that ports within testpmd support basic configuration functions.
Things such as starting a port, stopping a port, and closing a port should all be supported by the
device. Additionally, after each of these configuration steps (outside of closing the port) it
should still be possible to start the port again and verify that the port is able to forward a
large amount of packets (100 are sent in the test cases).
"""

from scapy.layers.l2 import Ether
from scapy.packet import Packet, Raw

from api.capabilities import (
    LinkTopology,
    NicCapability,
    requires_link_topology,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from api.testpmd.config import SimpleForwardingModes
from framework.test_suite import TestSuite, func_test


@requires_nic_capability(NicCapability.PHYSICAL_FUNCTION)
@requires_link_topology(LinkTopology.TWO_LINKS)
class TestPortControl(TestSuite):
    """DPDK Port Control Testing Suite."""

    def send_packets_and_verify(self) -> None:
        """Send 100 packets and verify that all packets were forwarded back.

        Packets sent are identical and are all ethernet frames with a payload of 30 "X" characters.
        This payload is used to differentiate noise on the wire from packets sent by this
        framework.
        """
        payload = "X" * 30
        num_pakts = 100
        send_p = Ether() / Raw(payload.encode("utf-8"))
        recv_pakts: list[Packet] = []
        for _ in range(int(num_pakts / 25)):
            recv_pakts += self.send_packets_and_capture([send_p] * 25)
        recv_pakts += self.send_packets_and_capture([send_p] * (num_pakts % 25))
        recv_pakts = [
            p
            for p in recv_pakts
            if
            (
                # Remove padding from the bytes.
                hasattr(p, "load") and p.load.decode("utf-8").replace("\x00", "") == payload
            )
        ]
        self.verify(
            len(recv_pakts) == num_pakts,
            f"Received {len(recv_pakts)} packets when {num_pakts} were expected.",
        )

    @func_test
    def start_ports(self) -> None:
        """Start all ports and send a small number of packets.

        Steps:
            Start all ports
            Start forwarding in MAC mode
            Send 100 generic packets to the SUT

        Verify:
            Check that all the packets sent are sniffed on the TG receive port.
        """
        with TestPmd(forward_mode=SimpleForwardingModes.mac) as testpmd:
            testpmd.start_all_ports()
            testpmd.start()
            self.send_packets_and_verify()

    @func_test
    def stop_ports(self) -> None:
        """Stop all ports, then start all ports, amd then send a small number of packets.

        Steps:
            Stop all ports
            Start all ports
            Start forwarding in MAC mode
            Send 100 generic packets to the SUT

        Verify:
            Check that stopping the testpmd ports brings down their links
            Check that all the packets sent are sniffed on the TG receive port.
        """
        with TestPmd(forward_mode=SimpleForwardingModes.mac) as testpmd:
            testpmd.stop_all_ports()
            self.verify(
                all(not p.is_link_up for p in testpmd.show_port_info_all()),
                "Failed to stop all ports.",
            )
            testpmd.start()
            self.send_packets_and_verify()

    @func_test
    def close_ports(self) -> None:
        """Close all the ports via testpmd.

        Steps:
            Close all the testpmd ports

        Verify:
            Check that testpmd no longer reports having any ports
        """
        with TestPmd() as testpmd:
            testpmd.close_all_ports()
            self.verify(
                len(testpmd.show_port_info_all()) == 0, "Failed to close all ports in testpmd."
            )
