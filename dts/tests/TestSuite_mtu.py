# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited
# Copyright(c) 2023-2024 University of New Hampshire
"""MTU update and jumbo frame forwarding test suite.

A suite of tests to ensures the consistency of jumbo and standard frame transmission within a DPDK
application. If a NIC receives a packet that is greater than its assigned MTU length, then that
packet should be dropped. Likewise, if a NIC receives a packet that is less than or equal to a
designated MTU length, the packet should be accepted.

The definition of MTU between individual vendors varies with a +/- difference of 9 bytes, at most.
To universally test MTU functionality, and not concern over individual vendor behavior, this test
suite compensates using a 9 byte upper and lower bound when testing a set MTU boundary.
"""

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from api.capabilities import (
    NicCapability,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from framework.test_suite import TestSuite, func_test

STANDARD_FRAME = 1518  # --max-pkt-len will subtract l2 information at a minimum of 18 bytes.
JUMBO_FRAME = 9018

STANDARD_MTU = 1500
JUMBO_MTU = 9000

IP_HEADER_LEN = 20
VENDOR_AGNOSTIC_PADDING = 9  # Used as a work around for varying MTU definitions between vendors.


@requires_nic_capability(NicCapability.PHYSICAL_FUNCTION)
class TestMtu(TestSuite):
    """DPDK PMD jumbo frames and MTU update test suite.

    Assess the expected behavior of frames greater than, less then, or equal to a designated MTU
    size in a DPDK application.

    Verify the behavior of both runtime MTU and pre-runtime MTU adjustments within DPDK
    applications. (Testpmd's CLI and runtime MTU adjustment options leverage different logical
    in components within ethdev to set a value).

    Test cases will verify that any frame greater than an assigned MTU are dropped and packets
    less than or equal to a designated MTU are forwarded and fully intact.
    """

    def set_up_suite(self) -> None:
        """Set up the test suite.

        Setup:
            Set traffic generator MTU lengths to a size greater than scope of all test cases.
        """
        self.topology.tg_port_egress.configure_mtu(JUMBO_MTU + 200)
        self.topology.tg_port_ingress.configure_mtu(JUMBO_MTU + 200)

    def _send_packet_and_verify(self, pkt_size: int, should_receive: bool) -> None:
        """Generate, send a packet, and assess its behavior based on a given packet size.

        Generates a packet based on a specified size and sends it to the SUT. The desired packet's
        payload size is calculated, and a string of arbrity size, containing a single character,
        is placed in the packet as payload. This method assesses whether or not it was forwarded,
        depending on the test case, and does so via a check of the previously-inserted packet
        payload.

        Args:
            pkt_size: Size of packet to be generated and sent.
            should_receive: Indicate whether the test case expects to receive the packet or not.
        """
        padding = pkt_size - IP_HEADER_LEN
        # Insert '    ' as placeholder 'CRC' error correction.
        packet = Ether() / Raw(load="    ") / IP(len=pkt_size) / Raw(load="X" * padding)
        received_packets = self.send_packet_and_capture(packet)
        found = any(
            ("X" * padding) in str(packets.load)
            for packets in received_packets
            if hasattr(packets, "load")
        )

        if should_receive:
            self.verify(found, "Did not receive packet.")
        else:
            self.verify(not found, "Received packet.")

    def _assess_mtu_boundary(self, testpmd_shell: TestPmd, mtu: int) -> None:
        """Sets the new MTU and verifies packets at the set boundary.

        Ensure that packets smaller than or equal to a set MTU will be received and packets larger
        will not.

        First, start testpmd and update the MTU. Then ensure the new value appears
        on port info for all ports. Next, start packet capturing and send 3 different lengths of
        packet and verify they are handled correctly:

        1. VENDOR_AGNOSTIC_PADDING units smaller than the MTU specified.
        2. Equal to the MTU specified.
        3. VENDOR_AGNOSTIC_PADDING units larger than the MTU specified (should be fragmented).

        Finally, stop packet capturing.

        Args:
            testpmd_shell: Active testpmd shell of a given test case.
            mtu: New Maximum Transmission Unit to be tested.
        """
        # Send 3 packets of different sizes (accounting for vendor inconsistencies).
        # 1. VENDOR_AGNOSTIC_PADDING units smaller than the MTU specified.
        # 2. Equal to the MTU specified.
        # 3. VENDOR_AGNOSTIC_PADDING units larger than the MTU specified.
        smaller_frame_size: int = mtu - VENDOR_AGNOSTIC_PADDING
        equal_frame_size: int = mtu
        larger_frame_size: int = mtu + VENDOR_AGNOSTIC_PADDING

        self._send_packet_and_verify(pkt_size=smaller_frame_size, should_receive=True)
        self._send_packet_and_verify(pkt_size=equal_frame_size, should_receive=True)

        current_mtu = testpmd_shell.show_port_info(0).mtu
        self.verify(current_mtu is not None, "Error grabbing testpmd MTU value.")
        if current_mtu and (
            current_mtu >= STANDARD_MTU + VENDOR_AGNOSTIC_PADDING and mtu == STANDARD_MTU
        ):
            self._send_packet_and_verify(pkt_size=larger_frame_size, should_receive=True)
        else:
            self._send_packet_and_verify(pkt_size=larger_frame_size, should_receive=False)

    @func_test
    def runtime_mtu_updating_and_forwarding(self) -> None:
        """Verify runtime MTU adjustments and assess packet forwarding behavior.

        Steps:
            * Start testpmd in a paired topology.
            * Set port MTU to 1500.
            * Send packets of 1491, 1500 and 1509 bytes.
                * Verify the first two packets are forwarded and the last is dropped.
            * Set port MTU to 2400.
            * Send packets of 1491, 1500 and 1509 bytes.
                * Verify all three packets are forwarded.
            * Send packets of 2391, 2400 and 2409 bytes.
                * Verify the first two packets are forwarded and the last is dropped.
            * Set port MTU to 4800.
            * Send packets of 1491, 1500 and 1509 bytes.
                * Verify all three packets are forwarded.
            * Send packets of 4791, 4800 and 4809 bytes.
                * Verify the first two packets are forwarded and the last is dropped.
            * Set port MTU to 9000.
            * Send packets of 1491, 1500 and 1509 bytes.
                * Verify all three packets are forwarded.
            * Send packets of 8991, 9000 and 9009 bytes.
                * Verify the first two packets are forwarded and the last is dropped.

        Verify:
            * Successful forwarding of packets via a search for an inserted payload.
              If the payload is found, the packet was transmitted successfully. Otherwise, the
              packet is considered dropped.
            * Standard MTU packets forward, in addition to packets within the limits of
              an MTU size set during runtime.
        """
        with TestPmd(tx_offloads=0x8000, mbuf_size=[JUMBO_MTU + 200]) as testpmd:
            testpmd.set_port_mtu_all(1500, verify=True)
            testpmd.start()
            self._assess_mtu_boundary(testpmd, 1500)
            testpmd.stop()

            testpmd.set_port_mtu_all(2400, verify=True)
            testpmd.start()
            self._assess_mtu_boundary(testpmd, 1500)
            self._assess_mtu_boundary(testpmd, 2400)
            testpmd.stop()

            testpmd.set_port_mtu_all(4800, verify=True)
            testpmd.start()
            self._assess_mtu_boundary(testpmd, 1500)
            self._assess_mtu_boundary(testpmd, 4800)
            testpmd.stop()

            testpmd.set_port_mtu_all(9000, verify=True)
            testpmd.start()
            self._assess_mtu_boundary(testpmd, 1500)
            self._assess_mtu_boundary(testpmd, 9000)
            testpmd.stop()

    @func_test
    def cli_mtu_forwarding_for_std_packets(self) -> None:
        """Assesses packet forwarding of standard MTU packets after pre-runtime MTU adjustments.

        Steps:
            * Start testpmd with MTU size of 1518 bytes, set pre-runtime.
            * Send packets of size 1491, 1500 and 1509 bytes.

        Verify:
            * First two packets are forwarded and the last is dropped.
            * Successful forwarding of packets via a search for an inserted payload.
              If the payload is found, the packet was transmitted successfully. Otherwise, the
              packet is considered dropped.
            * All packets are forwarded and the last is dropped after pre-runtime MTU modification.
        """
        with TestPmd(
            tx_offloads=0x8000,
            mbuf_size=[JUMBO_MTU + 200],
            mbcache=200,
            max_pkt_len=STANDARD_FRAME,
        ) as testpmd:
            testpmd.start()

            self._send_packet_and_verify(
                STANDARD_MTU - VENDOR_AGNOSTIC_PADDING, should_receive=True
            )
            self._send_packet_and_verify(STANDARD_MTU, should_receive=True)
            self._send_packet_and_verify(
                STANDARD_MTU + VENDOR_AGNOSTIC_PADDING, should_receive=False
            )

    @func_test
    def cli_jumbo_forwarding_for_jumbo_mtu(self) -> None:
        """Assess packet forwarding of packets within the bounds of a pre-runtime MTU adjustment.

        Steps:
            * Start testpmd with MTU size of 9018 bytes, set pre-runtime.
            * Send packets of size 8991, 9000 and 1509 bytes.

        Verify:
            * Successful forwarding of packets via a search for an inserted payload.
              If the payload is found, the packet was transmitted successfully. Otherwise, the
              packet is considered dropped.
            * All packets are forwarded after pre-runtime MTU modification.
        """
        with TestPmd(
            tx_offloads=0x8000,
            mbuf_size=[JUMBO_MTU + 200],
            mbcache=200,
            max_pkt_len=JUMBO_FRAME,
        ) as testpmd:
            testpmd.start()

            self._send_packet_and_verify(JUMBO_MTU - VENDOR_AGNOSTIC_PADDING, should_receive=True)
            self._send_packet_and_verify(JUMBO_MTU, should_receive=True)
            self._send_packet_and_verify(
                STANDARD_MTU + VENDOR_AGNOSTIC_PADDING, should_receive=True
            )

    @func_test
    def cli_mtu_std_packets_for_jumbo_mtu(self) -> None:
        """Assess boundary of jumbo MTU value set pre-runtime.

        Steps:
            * Start testpmd with MTU size of 9018 bytes, set pre-runtime.
            * Send a packets of size 8991, 9000 and 9009 bytes.

        Verify:
            * First two packets are forwarded and the last is dropped.
            * Successful forwarding of packets via a search for an inserted payload.
              If the payload is found, the packet was transmitted successfully. Otherwise, the
              packet is considered dropped.
            * Packets are forwarded and the last is dropped after pre-runtime
              MTU modification.
        """
        with TestPmd(
            tx_offloads=0x8000,
            mbuf_size=[JUMBO_MTU + 200],
            mbcache=200,
            max_pkt_len=JUMBO_FRAME,
        ) as testpmd:
            testpmd.start()

            self._send_packet_and_verify(JUMBO_MTU - VENDOR_AGNOSTIC_PADDING, should_receive=True)
            self._send_packet_and_verify(JUMBO_MTU, should_receive=True)
            self._send_packet_and_verify(JUMBO_MTU + VENDOR_AGNOSTIC_PADDING, should_receive=False)

    def tear_down_suite(self) -> None:
        """Tear down the test suite.

        Teardown:
            Set the MTU size of the traffic generator back to the standard 1518 byte size.
        """
        self.topology.tg_port_egress.configure_mtu(STANDARD_MTU)
        self.topology.tg_port_ingress.configure_mtu(STANDARD_MTU)
