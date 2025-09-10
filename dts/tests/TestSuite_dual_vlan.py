# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""Dual VLAN functionality testing suite.

The main objective of this test suite is to ensure that standard VLAN functions such as stripping
and filtering both still carry out their expected behavior in the presence of a packet which
contains two VLAN headers. These functions should carry out said behavior not just in isolation,
but also when other VLAN functions are configured on the same port. In addition to this, the
priority attributes of VLAN headers should be unchanged in the case of multiple VLAN headers
existing on a single packet, and a packet with only a single VLAN header should be able to have one
additional VLAN inserted into it.
"""

from enum import Flag, auto
from typing import ClassVar

from scapy.layers.l2 import Dot1Q, Ether
from scapy.packet import Packet, Raw

from api.testpmd import TestPmd
from api.testpmd.config import SimpleForwardingModes
from framework.test_suite import TestSuite, func_test


class TestDualVlan(TestSuite):
    """DPDK Dual VLAN test suite.

    This suite tests the behavior of VLAN functions and properties in the presence of two VLAN
    headers. All VLAN functions which are tested in this suite are specified using the inner class
    :class:`TestCaseOptions` and should have cases for configuring them in
    :meth:`configure_testpmd` as well as cases for testing their behavior in
    :meth:`verify_vlan_functions`. Every combination of VLAN functions being enabled should be
    tested. Additionally, attributes of VLAN headers, such as priority, are tested to ensure they
    are not modified in the case of two VLAN headers.
    """

    class TestCaseOptions(Flag):
        """Flag for specifying which VLAN functions to configure.

        These flags are specific to configuring this testcase and are used to
        create a matrix of all configuration combinations.
        """

        #:
        VLAN_STRIP = auto()
        #:
        VLAN_FILTER_INNER = auto()
        #:
        VLAN_FILTER_OUTER = auto()

    #: ID to set on inner VLAN tags.
    inner_vlan_tag: ClassVar[int] = 2
    #: ID to set on outer VLAN tags.
    outer_vlan_tag: ClassVar[int] = 1
    #: ID to use when inserting VLAN tags.
    vlan_insert_tag: ClassVar[int] = 3
    #:
    rx_port: ClassVar[int] = 0
    #:
    tx_port: ClassVar[int] = 1

    def _is_relevant_packet(self, pkt: Packet) -> bool:
        """Check if a packet was sent by functions in this suite.

        All functions in this test suite send packets with a payload that is packed with 20 "X"
        characters. This method, therefore, can determine if the packet was sent by this test suite
        by just checking to see if this payload exists on the received packet.

        Args:
            pkt: Packet to check for relevancy.

        Returns:
            :data:`True` if the packet contains the expected payload, :data:`False` otherwise.
        """
        return hasattr(pkt, "load") and "X" * 20 in str(pkt.load)

    def _pkt_payload_contains_layers(self, pkt: Packet, *expected_layers: Packet) -> bool:
        """Verify that the payload of the packet matches `expected_layers`.

        The layers in the payload of `pkt` must match the type and the user-defined fields of the
        layers in `expected_layers` in order.

        Args:
            pkt: Packet to check the payload of.
            *expected_layers: Layers expected to be in the payload of `pkt`.

        Returns:
            :data:`True` if the payload of `pkt` matches the layers in `expected_layers` in order,
            :data:`False` otherwise.
        """
        current_pkt_layer = pkt.payload
        ret = True
        for layer in expected_layers:
            ret &= isinstance(current_pkt_layer, type(layer))
            if not ret:
                break
            for field, val in layer.fields.items():
                ret &= (
                    hasattr(current_pkt_layer, field) and getattr(current_pkt_layer, field) == val
                )
            current_pkt_layer = current_pkt_layer.payload
        return ret

    def _verify_vlan_functions(self, send_packet: Packet, options: TestCaseOptions) -> None:
        """Send packet and verify the received packet has the expected structure.

        If VLAN_STRIP is in `options`, the outer VLAN tag should be stripped from the packet.

        Args:
            send_packet: Packet to send for testing.
            options: Flag which defines the currents configured settings in testpmd.
        """
        recv = self.send_packet_and_capture(send_packet)
        recv = list(filter(self._is_relevant_packet, recv))
        expected_layers: list[Packet] = []

        if self.TestCaseOptions.VLAN_STRIP not in options:
            expected_layers.append(Dot1Q(vlan=self.outer_vlan_tag))
        expected_layers.append(Dot1Q(vlan=self.inner_vlan_tag))

        self.verify(
            len(recv) > 0,
            f"Expected to receive packet with the payload {expected_layers} but got nothing.",
        )

        for pkt in recv:
            self.verify(
                self._pkt_payload_contains_layers(pkt, *expected_layers),
                f"Received packet ({pkt.summary()}) did not match the expected sequence of layers "
                f"{expected_layers} with options {options}.",
            )

    def _configure_testpmd(self, shell: TestPmd, options: TestCaseOptions, add: bool) -> None:
        """Configure VLAN functions in testpmd based on `options`.

        Args:
            shell: Testpmd session to configure the settings on.
            options: Settings to modify in `shell`.
            add: If :data:`True`, turn the settings in `options` on, otherwise turn them off.
        """
        if (
            self.TestCaseOptions.VLAN_FILTER_INNER in options
            or self.TestCaseOptions.VLAN_FILTER_OUTER in options
        ):
            if add:
                # If we are adding a filter, filtering has to be enabled first
                shell.set_vlan_filter(self.rx_port, True)

            if self.TestCaseOptions.VLAN_FILTER_INNER in options:
                shell.rx_vlan(self.inner_vlan_tag, self.rx_port, add)
            if self.TestCaseOptions.VLAN_FILTER_OUTER in options:
                shell.rx_vlan(self.outer_vlan_tag, self.rx_port, add)

            if not add:
                # If we are removing filters then we need to remove the filters before we can
                # disable filtering.
                shell.set_vlan_filter(self.rx_port, False)
        if self.TestCaseOptions.VLAN_STRIP in options:
            shell.set_vlan_strip(self.rx_port, add)

    @func_test
    def insert_second_vlan(self) -> None:
        """Test that a packet with a single VLAN can have an additional one inserted into it.

        Steps:
            * Set VLAN tag on the tx port.
            * Create a packet with a VLAN tag.
            * Send and receive the packet.

        Verify:
            * Packets are received.
            * Packet contains two VLAN tags.
        """
        with TestPmd(forward_mode=SimpleForwardingModes.mac) as testpmd:
            testpmd.tx_vlan_set(port=self.tx_port, enable=True, vlan=self.vlan_insert_tag)
            testpmd.start()
            recv = self.send_packet_and_capture(
                Ether() / Dot1Q(vlan=self.outer_vlan_tag) / Raw(b"X" * 20)
            )
            self.verify(len(recv) > 0, "Did not receive any packets when testing VLAN insertion.")
            self.verify(
                any(
                    self._is_relevant_packet(p)
                    and self._pkt_payload_contains_layers(
                        p, Dot1Q(vlan=self.vlan_insert_tag), Dot1Q(vlan=self.outer_vlan_tag)
                    )
                    for p in recv
                ),
                "Packet was unable to insert a second VLAN tag.",
            )

    @func_test
    def all_vlan_functions(self) -> None:
        """Test that all combinations of :class:`TestCaseOptions` behave as expected.

        Steps:
            * Send packets with VLAN functions disabled.
            * Send packets with a set of combinations of VLAN functions enabled.
            * Disable VLAN function to allow for a clean environment for the next test.

        Verify:
            * Packet with two VLANs is unchanged without the VLAN modification functions enabled.
            * VLAN functions work as expected.
        """
        send_pkt = (
            Ether()
            / Dot1Q(vlan=self.outer_vlan_tag)
            / Dot1Q(vlan=self.inner_vlan_tag)
            / Raw(b"X" * 20)
        )
        with TestPmd(forward_mode=SimpleForwardingModes.mac) as testpmd:
            testpmd.start()
            recv = self.send_packet_and_capture(send_pkt)
            self.verify(len(recv) > 0, "Unmodified packet was not received.")
            self.verify(
                any(
                    self._is_relevant_packet(p)
                    and self._pkt_payload_contains_layers(
                        p, Dot1Q(vlan=self.outer_vlan_tag), Dot1Q(vlan=self.inner_vlan_tag)
                    )
                    for p in recv
                ),
                "Packet was modified without any VLAN functions applied.",
            )
            testpmd.stop()
            for i in range(2 ** len(self.TestCaseOptions)):
                options = self.TestCaseOptions(i)
                self._configure_testpmd(testpmd, options, True)
                testpmd.start()
                self._verify_vlan_functions(send_pkt, options)
                testpmd.stop()
                self._configure_testpmd(testpmd, options, False)

    @func_test
    def maintains_priority(self) -> None:
        """Test that priorities of multiple VLAN tags are preserved by the PMD.

        Steps:
            * Create packets with VLAN tags with priorities.
            * Send and receive packets.

        Verify:
            * Packets are received.
            * Priorities are unchanged.
        """
        pkt = (
            Ether()
            / Dot1Q(vlan=self.outer_vlan_tag, prio=1)
            / Dot1Q(vlan=self.inner_vlan_tag, prio=2)
            / Raw(b"X" * 20)
        )
        with TestPmd(forward_mode=SimpleForwardingModes.mac) as testpmd:
            testpmd.start()
            recv = self.send_packet_and_capture(pkt)
            self.verify(len(recv) > 0, "Did not receive any packets when testing VLAN priority.")
            self.verify(
                any(
                    self._is_relevant_packet(p)
                    and self._pkt_payload_contains_layers(
                        p,
                        Dot1Q(vlan=self.outer_vlan_tag, prio=1),
                        Dot1Q(vlan=self.inner_vlan_tag, prio=2),
                    )
                    for p in recv
                ),
                "VLAN headers did not maintain their priorities.",
            )
