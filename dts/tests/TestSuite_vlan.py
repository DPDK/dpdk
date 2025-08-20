# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""Test the support of VLAN Offload Features by Poll Mode Drivers.

This test suite verifies that VLAN filtering, stripping, and header insertion all
function as expected. When a VLAN ID is in the filter list, all packets with that
ID should be forwarded and all others should be dropped. While stripping is enabled,
packets sent with a VLAN ID should have the ID removed and then be forwarded.
Additionally, when header insertion is enabled packets without a
VLAN ID should have a specified ID inserted and then be forwarded.

"""

from scapy.layers.l2 import Dot1Q, Ether
from scapy.packet import Raw

from framework.remote_session.testpmd_shell import SimpleForwardingModes, TestPmdShell
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import NicCapability, TopologyType, requires


@requires(NicCapability.RX_OFFLOAD_VLAN_FILTER)
@requires(topology_type=TopologyType.two_links)
class TestVlan(TestSuite):
    """DPDK VLAN test suite.

    Ensures VLAN packet reception, stripping, and insertion on the Poll Mode Driver
    when the appropriate conditions are met. The suite contains four test cases:

    1. VLAN reception no stripping - verifies that a VLAN packet with a tag
    within the filter list is received.
    2. VLAN reception stripping - verifies that a VLAN packet with a tag
    within the filter list is received without the VLAN tag.
    3. VLAN no reception - verifies that a VLAN packet with a tag not within
    the filter list is dropped.
    4. VLAN insertion - verifies that a non VLAN packet is received with a VLAN
    tag when insertion is enabled.
    """

    def send_vlan_packet_and_verify(self, should_receive: bool, strip: bool, vlan_id: int) -> None:
        """Generate a VLAN packet, send and verify packet with same payload is received on the dut.

        Args:
            should_receive: Indicate whether the packet should be successfully received.
            strip: If :data:`False`, will verify received packets match the given VLAN ID,
                otherwise verifies that the received packet has no VLAN ID
                (as it has been stripped off.)
            vlan_id: Expected VLAN ID.
        """
        packet = Ether() / Dot1Q(vlan=vlan_id) / Raw(load="xxxxx")
        received_packets = self.send_packet_and_capture(packet)
        test_packet = None
        for packet in received_packets:
            if hasattr(packet, "load") and b"xxxxx" in packet.load:
                test_packet = packet
                break
        if should_receive:
            self.verify(
                test_packet is not None,
                "Packet was dropped when it should have been received",
            )
            if test_packet is not None:
                if strip:
                    self.verify(
                        not test_packet.haslayer(Dot1Q),
                        "VLAN tag was not stripped successfully",
                    )
                else:
                    self.verify(
                        test_packet.vlan == vlan_id,
                        "The received tag did not match the expected tag",
                    )
        else:
            self.verify(
                test_packet is None,
                "Packet was received when it should have been dropped",
            )

    def send_packet_and_verify_insertion(self, expected_id: int) -> None:
        """Generate a packet with no VLAN tag, send and verify on the dut.

        Args:
            expected_id: The VLAN id that is being inserted through tx_offload configuration.
        """
        packet = Ether() / Raw(load="xxxxx")
        received_packets = self.send_packet_and_capture(packet)
        test_packet = None
        for packet in received_packets:
            if hasattr(packet, "load") and b"xxxxx" in packet.load:
                test_packet = packet
                break
        self.verify(
            test_packet is not None,
            "Packet was dropped when it should have been received",
        )
        if test_packet is not None:
            self.verify(
                test_packet.haslayer(Dot1Q) == 1,
                "The received packet did not have a VLAN tag",
            )
            self.verify(
                test_packet.vlan == expected_id,
                "The received tag did not match the expected tag",
            )

    def vlan_setup(self, testpmd: TestPmdShell, port_id: int, filtered_id: int) -> None:
        """Setup method for all test cases.

        Args:
            testpmd: Testpmd shell session to send commands to.
            port_id: Number of port to use for setup.
            filtered_id: ID to be added to the VLAN filter list.
        """
        testpmd.set_forward_mode(SimpleForwardingModes.mac)
        testpmd.set_promisc(port_id, False)
        testpmd.set_vlan_filter(port=port_id, enable=True)
        testpmd.rx_vlan(vlan=filtered_id, port=port_id, add=True)

    @func_test
    def vlan_receipt_no_stripping(self) -> None:
        """Verify packets are received with their VLAN IDs when stripping is disabled.

        Test:
            Create an interactive testpmd shell and verify a VLAN packet.
        """
        with TestPmdShell() as testpmd:
            self.vlan_setup(testpmd=testpmd, port_id=0, filtered_id=1)
            testpmd.start()
            self.send_vlan_packet_and_verify(True, strip=False, vlan_id=1)

    @requires(NicCapability.RX_OFFLOAD_VLAN_STRIP)
    @func_test
    def vlan_receipt_stripping(self) -> None:
        """Ensure VLAN packet received with no tag when receipts and header stripping are enabled.

        Test:
            Create an interactive testpmd shell and verify a VLAN packet.
        """
        with TestPmdShell() as testpmd:
            self.vlan_setup(testpmd=testpmd, port_id=0, filtered_id=1)
            testpmd.set_vlan_strip(port=0, enable=True)
            testpmd.start()
            self.send_vlan_packet_and_verify(should_receive=True, strip=True, vlan_id=1)

    @func_test
    def vlan_no_receipt(self) -> None:
        """Ensure VLAN packet dropped when filter is on and sent tag not in the filter list.

        Test:
            Create an interactive testpmd shell and verify a VLAN packet.
        """
        with TestPmdShell() as testpmd:
            self.vlan_setup(testpmd=testpmd, port_id=0, filtered_id=1)
            testpmd.start()
            self.send_vlan_packet_and_verify(should_receive=False, strip=False, vlan_id=2)

    @func_test
    def vlan_header_insertion(self) -> None:
        """Ensure that VLAN packet is received with the correct inserted VLAN tag.

        Test:
            Create an interactive testpmd shell and verify a non-VLAN packet.
        """
        with TestPmdShell() as testpmd:
            testpmd.set_forward_mode(SimpleForwardingModes.mac)
            testpmd.set_promisc(port=0, enable=False)
            testpmd.stop_all_ports()
            testpmd.tx_vlan_set(port=1, enable=True, vlan=51)
            testpmd.start_all_ports()
            testpmd.start()
            self.send_packet_and_verify_insertion(expected_id=51)
