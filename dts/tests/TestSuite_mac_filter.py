# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023-2024 University of New Hampshire
"""Mac address filtering test suite.

This test suite ensures proper and expected behavior of Allowlist filtering via mac
addresses on devices bound to the Poll Mode Driver. If a packet received on a device
contains a mac address not contained within its mac address pool, the packet should
be dropped. Alternatively, if a packet is received that contains a destination mac
within the devices address pool, the packet should be accepted and forwarded. This
behavior should remain consistent across all packets, namely those containing dot1q
tags or otherwise.

The following test suite assesses behaviors based on the aforementioned logic.
Additionally, testing is done within the PMD itself to ensure that the mac address
allow list is behaving as expected.
"""

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from api.capabilities import (
    NicCapability,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from framework.exception import InteractiveCommandExecutionError
from framework.test_suite import TestSuite, func_test


@requires_nic_capability(NicCapability.PHYSICAL_FUNCTION)
class TestMacFilter(TestSuite):
    """Mac address allowlist filtering test suite.

    Configure mac address filtering on a given port, and test the port's filtering behavior
    using both a given port's hardware address as well as dummy addresses. If a port accepts
    a packet that is not contained within its mac address allowlist, then a given test case
    fails. Alternatively, if a port drops a packet that is designated within its mac address
    allowlist, a given test case will fail.

    Moreover, a given port should demonstrate proper behavior when bound to the Poll Mode
    Driver. A port should not have a mac address allowlist that exceeds its designated size.
    A port's default hardware address should not be removed from its address pool, and invalid
    addresses should not be included in the allowlist. If a port abides by the above rules, the
    test case passes.
    """

    def _send_packet_and_verify(
        self,
        mac_address: str,
        should_receive: bool = True,
    ) -> None:
        """Generate, send, and verify a packet based on specified parameters.

        Test cases within this suite utilize this method to create, send, and verify
        packets based on criteria relating to the packet's destination mac address,
        and vlan tag. Packets are verified using an inserted payload. Assuming the test case
        expects to receive a specified packet, if the list of received packets contains this
        payload within any of its packets, the test case passes. Alternatively, if the designed
        packet should not be received, and the packet payload is received, then the test case
        fails. Each call with this method sends exactly one packet.

        Args:
            mac_address: The destination mac address of the packet being sent.
            add_vlan: If :data:'True', add a vlan tag to the packet being sent. The
                vlan tag will be :data:'2' if the packet should be received and
                :data:'1' if the packet should not be received but requires a vlan tag.
            should_receive: If :data:'True', assert whether or not the sent packet
                has been received. If :data:'False', assert that the send packet was not
                received. :data:'True' by default
        """
        packet = Ether() / IP() / Raw(load="X" * 22)
        packet.dst = mac_address
        received_packets = [
            packets
            for packets in self.send_packet_and_capture(packet)
            if hasattr(packets, "load") and "X" * 22 in str(packets.load)
        ]
        if should_receive:
            self.verify(
                len(received_packets) == 1,
                "Packet sent by test case should be forwarded and received.",
            )
        else:
            self.verify(
                len(received_packets) == 0,
                "Packet sent by test case should not be forwarded and received.",
            )

    @func_test
    def add_remove_mac_addresses(self) -> None:
        """Assess basic mac addressing filtering functionalities.

        This test case validates for proper behavior of mac address filtering with both
        a port's default, burned-in mac address, as well as additional mac addresses
        added to the PMD. Packets should either be received or not received depending on
        the properties applied to the PMD at any given time.

        Steps:
            * Start testpmd without promiscuous mode.
            * Send a packet with the port's default mac address. (Should receive)
            * Send a packet with fake mac address. (Should not receive)
            * Add fake mac address to the PMD's address pool.
            * Send a packet with the fake mac address to the PMD. (Should receive)
            * Remove the fake mac address from the PMD's address pool.
            * Send a packet with the fake mac address to the PMD. (Should not receive)

        Verify:
            * Packet with the port's default mac address is received.
            * Packet with a fake mac address is not received.
            * Packet with a fake mac address is received when being sent to the PMD.
            * Packet with a fake mac address is not received when being sent to the PMD.
        """
        with TestPmd() as testpmd:
            testpmd.set_promisc(0, enable=False)
            testpmd.start()
            mac_address = self.topology.sut_port_ingress.mac_address

            # Send a packet with NIC default mac address
            self._send_packet_and_verify(mac_address=mac_address, should_receive=True)
            # Send a packet with different mac address
            fake_address = "00:00:00:00:00:01"
            self._send_packet_and_verify(mac_address=fake_address, should_receive=False)

            # Add mac address to pool and rerun tests
            testpmd.set_mac_addr(0, mac_address=fake_address, add=True)
            self._send_packet_and_verify(mac_address=fake_address, should_receive=True)
            testpmd.set_mac_addr(0, mac_address=fake_address, add=False)
            self._send_packet_and_verify(mac_address=fake_address, should_receive=False)

    @func_test
    def invalid_address(self) -> None:
        """Assess the behavior of a NIC mac address pool while bound to the PMD.

        An assessment of a NIC's behavior when mounted to a PMD as it relates to mac addresses
        and address pooling. Devices should not be able to use invalid mac addresses, remove their
        built-in hardware address, or exceed their address pools.

        Steps:
            * Start testpmd.
            * Attempt to add an invalid mac address. (Should fail)
            * Attempt to remove the device's hardware address with no additional addresses in the
              address pool. (Should fail)
            * Add a fake mac address to the pool twice in succession. (Should not create any errors)
            * Attempt to remove the device's hardware address with other addresses in the address
              pool. (Should fail)
            * Determine the device's mac address pool size, and fill the pool with fake addresses.
            * Attempt to add another fake mac address, overloading the address pool. (Should fail)

        Verify:
            * It is not possible to add a invalid mac address.
            * It is not possible to remove the default mac address when there are no other
              addresses in the pool.
            * It is not possible to remove the default mac address when there are other
              addresses in the pool.
            * It is not possible to add another mac address.
        """
        with TestPmd() as testpmd:
            testpmd.start()
            mac_address = self.topology.sut_port_ingress.mac_address
            try:
                testpmd.set_mac_addr(0, "00:00:00:00:00:00", add=True)
                self.verify(False, "Invalid mac address added.")
            except InteractiveCommandExecutionError:
                pass
            try:
                testpmd.set_mac_addr(0, mac_address, add=False)
                self.verify(False, "Default mac address removed.")
            except InteractiveCommandExecutionError:
                pass
            # Should be no errors adding this twice
            testpmd.set_mac_addr(0, "1" + mac_address[1:], add=True)
            testpmd.set_mac_addr(0, "1" + mac_address[1:], add=True)
            # Double check to see if default mac address can be removed
            try:
                testpmd.set_mac_addr(0, mac_address, add=False)
                self.verify(False, "Default mac address removed.")
            except InteractiveCommandExecutionError:
                pass

            for i in range(testpmd.show_port_info(0).max_mac_addresses_num - 1):
                # A0 fake address based on the index 'i'.
                fake_address = str(hex(i)[2:].zfill(12))
                # Insert ':' characters every two indexes to create a fake mac address.
                fake_address = ":".join(
                    fake_address[x : x + 2] for x in range(0, len(fake_address), 2)
                )
                testpmd.set_mac_addr(0, fake_address, add=True, verify=False)
            try:
                testpmd.set_mac_addr(0, "E" + mac_address[1:], add=True)
                # We add an extra address to compensate for mac address pool inconsistencies.
                testpmd.set_mac_addr(0, "F" + mac_address[1:], add=True)
                self.verify(False, "Mac address limit exceeded.")
            except InteractiveCommandExecutionError:
                pass

    @requires_nic_capability(NicCapability.MCAST_FILTERING)
    @func_test
    def multicast_filter(self) -> None:
        """Assess basic multicast address filtering functionalities.

        Ensure that multicast filtering performs as intended when a given device is bound
        to the PMD.

        Steps:
            * Start testpmd without promiscuous mode.
            * Add a fake multicast address to the PMD's multicast address pool.
            * Send a packet with the fake multicast address to the PMD.
            * Remove the fake multicast address from the PMDs multicast address filter.
            * Send a packet with the fake multicast address to the PMD.

        Verify:
            * Packet sent with a fake multicast address is received.
            * Packet sent with a fake multicast address is not received.
        """
        with TestPmd() as testpmd:
            testpmd.start()
            testpmd.set_promisc(0, enable=False)
            multicast_address = "01:00:5E:00:00:00"

            testpmd.set_multicast_mac_addr(0, multi_addr=multicast_address, add=True)
            self._send_packet_and_verify(multicast_address, should_receive=True)

            # Remove multicast filter and verify the packet was not received.
            testpmd.set_multicast_mac_addr(0, multicast_address, add=False)
            self._send_packet_and_verify(multicast_address, should_receive=False)
