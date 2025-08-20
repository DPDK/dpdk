# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""Dynamic configuration capabilities test suite.

This suite checks that it is possible to change the configuration of a port
dynamically. The Poll Mode Driver should be able to enable and disable
promiscuous mode on each port, as well as check the Rx and Tx packets of
each port. Promiscuous mode in networking passes all traffic a NIC receives
to the CPU, rather than just frames with matching MAC addresses. Each test
case sends a packet with a matching address, and one with an unknown address,
to ensure this behavior is shown.

If packets should be received and forwarded, or received and not forwarded,
depending on the configuration, the port info should match the expected behavior.
"""

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from framework.params.testpmd import SimpleForwardingModes
from framework.remote_session.testpmd_shell import NicCapability, TestPmdShell
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import TopologyType, requires


@requires(NicCapability.PHYSICAL_FUNCTION)
@requires(topology_type=TopologyType.two_links)
class TestDynamicConfig(TestSuite):
    """Dynamic config suite.

    Use the show port commands to see the MAC address and promisc mode status
    of the Rx port on the DUT. The suite will check the Rx and Tx packets
    of each port after configuring promiscuous, multicast, and default mode
    on the DUT to verify the expected behavior. It consists of four test cases:

    1. Default mode: verify packets are received and forwarded.
    2. Disable promiscuous mode: verify that packets are received
       only for the packet with destination address matching the port address.
    3. Disable promiscuous mode broadcast: verify that packets with destination
       MAC address not matching the port are received and not forwarded, and verify
       that broadcast packets are received and forwarded.
    4. Disable promiscuous mode multicast: verify that packets with destination
       MAC address not matching the port are received and not forwarded, and verify
       that multicast packets are received and forwarded.
    """

    def send_packet_and_verify(self, should_receive: bool, mac_address: str) -> None:
        """Generate, send and verify packets.

        Generate a packet and send to the DUT, verify that packet is forwarded from DUT to
        traffic generator if that behavior is expected.

        Args:
            should_receive: Indicate whether the packet should be received.
            mac_address: Destination MAC address to generate in packet.
        """
        packet = Ether(dst=mac_address) / IP() / Raw(load="xxxxx")
        received = self.send_packet_and_capture(packet)
        contains_packet = any(
            packet.haslayer(Raw) and b"xxxxx" in packet.load for packet in received
        )
        self.verify(
            should_receive == contains_packet,
            f"Packet was {'dropped' if should_receive else 'received'}",
        )

    def disable_promisc_setup(self, testpmd: TestPmdShell, port_id: int) -> TestPmdShell:
        """Sets up testpmd shell config for cases where promisc mode is disabled.

        Args:
            testpmd: Testpmd session that is being configured.
            port_id: Port number to disable promisc mode on.

        Returns:
            TestPmdShell: interactive testpmd shell object.
        """
        testpmd.start()
        testpmd.set_promisc(port=port_id, enable=False)
        testpmd.set_forward_mode(SimpleForwardingModes.io)
        return testpmd

    @func_test
    def default_mode(self) -> None:
        """Tests default configuration.

        Creates a testpmd shell, verifies that promiscuous mode is enabled by default,
        and sends two packets; one matching source MAC address and one unknown.
        Verifies that both are received.
        """
        with TestPmdShell() as testpmd:
            is_promisc = testpmd.show_port_info(0).is_promiscuous_mode_enabled
            self.verify(is_promisc, "Promiscuous mode was not enabled by default.")
            testpmd.start()
            mac = testpmd.show_port_info(0).mac_address
            # send a packet with Rx port mac address
            self.send_packet_and_verify(should_receive=True, mac_address=str(mac))
            # send a packet with mismatched mac address
            self.send_packet_and_verify(should_receive=True, mac_address="00:00:00:00:00:01")

    @func_test
    def disable_promisc(self) -> None:
        """Tests disabled promiscuous mode configuration.

        Creates an interactive testpmd shell, disables promiscuous mode,
        and sends two packets; one matching source MAC address and one unknown.
        Verifies that only the matching address packet is received.
        """
        with TestPmdShell() as testpmd:
            testpmd = self.disable_promisc_setup(testpmd=testpmd, port_id=0)
            mac = testpmd.show_port_info(0).mac_address
            self.send_packet_and_verify(should_receive=True, mac_address=str(mac))
            self.send_packet_and_verify(should_receive=False, mac_address="00:00:00:00:00:01")

    @func_test
    def disable_promisc_broadcast(self) -> None:
        """Tests broadcast reception with disabled promisc mode config.

        Creates an interactive testpmd shell, disables promiscuous mode,
        and sends two packets; one matching source MAC address and one broadcast.
        Verifies that both packets are received.
        """
        with TestPmdShell() as testpmd:
            testpmd = self.disable_promisc_setup(testpmd=testpmd, port_id=0)
            mac = testpmd.show_port_info(0).mac_address
            self.send_packet_and_verify(should_receive=True, mac_address=str(mac))
            self.send_packet_and_verify(should_receive=True, mac_address="ff:ff:ff:ff:ff:ff")

    @func_test
    def disable_promisc_multicast(self) -> None:
        """Tests allmulticast mode with disabled promisc config.

        Creates an interactive testpmd shell, disables promiscuous mode,
        and sends two packets; one matching source MAC address and one multicast.
        Verifies that the multicast packet is only received once allmulticast mode is enabled.
        """
        with TestPmdShell() as testpmd:
            testpmd = self.disable_promisc_setup(testpmd=testpmd, port_id=0)
            testpmd.set_multicast_all(on=False)
            # 01:00:5E:00:00:01 is the first of the multicast MAC range of addresses
            self.send_packet_and_verify(should_receive=False, mac_address="01:00:5E:00:00:01")
            testpmd.set_multicast_all(on=True)
            self.send_packet_and_verify(should_receive=True, mac_address="01:00:05E:00:00:01")
