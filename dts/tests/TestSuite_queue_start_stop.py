# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""Rx/Tx queue start and stop functionality suite.

This suite tests the ability of the poll mode driver to start and
stop either the Rx or Tx queue (depending on the port) during runtime,
and verify that packets are not received when one is disabled.

Given a paired port topology, the Rx queue will be disabled on port 0,
and the Tx queue will be disabled on port 1.

"""

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from framework.remote_session.testpmd_shell import SimpleForwardingModes, TestPmdShell
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import NicCapability, TopologyType, requires


@requires(topology_type=TopologyType.two_links)
@requires(NicCapability.RUNTIME_RX_QUEUE_SETUP)
@requires(NicCapability.RUNTIME_TX_QUEUE_SETUP)
class TestQueueStartStop(TestSuite):
    """DPDK Queue start/stop test suite.

    Ensures Rx/Tx queue on a port can be disabled and enabled.
    Verifies packets are not received when either queue is disabled.
    The suite contains four test cases, two Rx queue start/stop and
    two Tx queue start/stop, which each disable the corresponding
    queue and verify that packets are not received/forwarded. There
    are two cases that verify deferred start mode produces the expected
    behavior in both the Rx and Tx queue.
    """

    def send_packet_and_verify(self, should_receive: bool = True) -> None:
        """Generate a packet, send to the DUT, and verify it is forwarded back.

        Args:
            should_receive: Indicate whether the packet should be received.
        """
        packet = Ether() / IP() / Raw(load="xxxxx")
        received = self.send_packet_and_capture(packet)
        contains_packet = any(
            packet.haslayer(Raw) and b"xxxxx" in packet.load for packet in received
        )
        self.verify(
            should_receive == contains_packet,
            f"Packet was {'dropped' if should_receive else 'received'}",
        )

    @func_test
    def rx_queue_start_stop(self) -> None:
        """Rx queue start stop test.

        Steps:
            Launch testpmd, stop Rx queue 0 on port 0.
            Stop testpmd, start Rx queue 0 on port 0, start testpmd.
        Verify:
            Send a packet on port 0 after Rx queue is stopped, ensure it is not received.
            Send a packet on port 0 after Rx queue is started, ensure it is received.
        """
        with TestPmdShell() as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.mac)
            testpmd.stop_port_queue(0, 0, True)
            testpmd.start()
            self.send_packet_and_verify(should_receive=False)
            testpmd.stop()
            testpmd.start_port_queue(0, 0, True)
            testpmd.start()
            self.send_packet_and_verify(should_receive=True)

    @func_test
    def tx_queue_start_stop(self) -> None:
        """Tx queue start stop test.

        Steps:
            Launch testpmd, stop Tx queue 0 on port 0.
            Stop testpmd, start Tx queue 0 on port 0, start testpmd.
        Verify:
            Send a packet on port 0 after Tx queue is stopped, ensure it is not received.
            Send a packet on port 0 after Tx queue is started, ensure it is received.
        """
        with TestPmdShell() as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.mac)
            testpmd.stop_port_queue(1, 0, False)
            testpmd.start()
            self.send_packet_and_verify(should_receive=False)
            testpmd.stop()
            testpmd.start_port_queue(1, 0, False)
            testpmd.start()
            self.send_packet_and_verify(should_receive=True)

    @func_test
    def rx_queue_deferred_start(self) -> None:
        """Rx queue deferred start stop test.

        Steps:
            Stop all ports, enable deferred start mode on port 0 Rx queue 0, start all ports.
            Launch testpmd, send a packet.
            Stop testpmd, start port 0 Rx queue 0.
            Start testpmd, send a packet.
        Verify:
            Send a packet on port 0 after deferred start is set, ensure it is not received.
            Send a packet on port 0 after Rx queue 0 is started, ensure it is received.
        """
        with TestPmdShell() as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.mac)
            testpmd.stop_all_ports()
            testpmd.set_queue_deferred_start(0, 0, True, True)
            testpmd.start_all_ports()
            testpmd.start()
            self.send_packet_and_verify(should_receive=False)
            testpmd.stop()
            testpmd.start_port_queue(0, 0, True)
            testpmd.start()
            self.send_packet_and_verify(should_receive=True)

    @func_test
    def tx_queue_deferred_start(self) -> None:
        """Tx queue start stop test.

        Steps:
            Stop all ports, enable deferred start mode on port 1 Tx queue 0, start all ports.
            Launch testpmd, send a packet.
            Stop testpmd, start port 1 Tx queue 0.
            Start testpmd, send a packet.
        Verify:
            Send a packet on port 1 after deferred start is set, ensure it is not received.
            Send a packet on port 1 after Tx queue 0 is started, ensure it is received.
        """
        with TestPmdShell() as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.mac)
            testpmd.stop_all_ports()
            testpmd.set_queue_deferred_start(1, 0, False, True)
            testpmd.start_all_ports()
            testpmd.start()
            self.send_packet_and_verify(should_receive=False)
            testpmd.stop()
            testpmd.start_port_queue(1, 0, False)
            testpmd.start()
            self.send_packet_and_verify(should_receive=True)
