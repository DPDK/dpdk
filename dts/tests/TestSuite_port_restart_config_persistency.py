# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Port config persistency test suite.

Changes configuration of ports and verifies that the configuration persists after a
port is restarted.
"""

from dataclasses import asdict

from api.capabilities import (
    NicCapability,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from api.testpmd.types import TestPmdPortFlowCtrl
from framework.test_suite import TestSuite, func_test

ALTERNATIVE_MTU: int = 800
STANDARD_MTU: int = 1500
ALTERNATIVE_MAC_ADDRESS: str = "42:A6:B7:9E:B4:81"


class TestPortRestartConfigPersistency(TestSuite):
    """Port config persistency test suite."""

    def _restart_port_and_verify(self, id: int, testpmd: TestPmd, changed_value: str) -> None:
        """Fetch port config, restart and verify persistency."""
        testpmd.start_all_ports()
        testpmd.wait_link_status_up(port_id=id, timeout=10)

        port_info_before = testpmd.show_port_info(id)
        all_info_before = asdict(port_info_before)

        flow_info_before = testpmd.show_port_flow_info(id)

        if flow_info_before:
            all_info_before.update(asdict(flow_info_before))

        testpmd.stop_all_ports()
        testpmd.start_all_ports()
        testpmd.wait_link_status_up(port_id=id, timeout=10)

        port_info_after = testpmd.show_port_info(id)
        all_info_after = asdict(port_info_after)

        flow_info_after = testpmd.show_port_flow_info(id)
        if flow_info_after:
            all_info_after.update(asdict(flow_info_after))

        self.verify(
            all_info_before == all_info_after,
            f"Port configuration for {changed_value} was not retained through port restart.",
        )
        testpmd.stop_all_ports()

    @func_test
    def port_configuration_persistence(self) -> None:
        """Port restart configuration persistency test.

        Steps:
            * For each port set the port MTU, VLAN filter, mac address, and promiscuous mode.
            * Save port config and restart port.

        Verify:
            * The configuration persists after the port is restarted.
        """
        with TestPmd(disable_device_start=True) as testpmd:
            for port_id, _ in enumerate(self.topology.sut_ports):
                testpmd.set_port_mtu(port_id=port_id, mtu=STANDARD_MTU, verify=True)
                self._restart_port_and_verify(port_id, testpmd, "MTU")

                testpmd.set_port_mtu(port_id=port_id, mtu=ALTERNATIVE_MTU, verify=True)
                self._restart_port_and_verify(port_id, testpmd, "MTU")

                testpmd.set_vlan_filter(port=port_id, enable=True, verify=True)
                self._restart_port_and_verify(port_id, testpmd, "VLAN filter")

                testpmd.set_mac_address(
                    port=port_id, mac_address=ALTERNATIVE_MAC_ADDRESS, verify=True
                )
                self._restart_port_and_verify(port_id, testpmd, "MAC address")

                testpmd.set_promisc(port=port_id, enable=True, verify=True)
                self._restart_port_and_verify(port_id, testpmd, "promiscuous mode")

    @requires_nic_capability(NicCapability.FLOW_CTRL)
    @func_test
    def flow_ctrl_port_configuration_persistence(self) -> None:
        """Flow control port configuration persistency test.

        Steps:
            * For each port enable flow control for RX and TX individually.

        Verify:
            * The configuration persists after the port is restarted.
        """
        with TestPmd(disable_device_start=True) as testpmd:
            for port_id, _ in enumerate(self.topology.sut_ports):
                flow_ctrl = TestPmdPortFlowCtrl(rx=True)
                testpmd.set_flow_control(port=port_id, flow_ctrl=flow_ctrl)
                self._restart_port_and_verify(port_id, testpmd, "flow_ctrl")

                flow_ctrl = TestPmdPortFlowCtrl(tx=True)
                testpmd.set_flow_control(port=port_id, flow_ctrl=flow_ctrl)
                self._restart_port_and_verify(port_id, testpmd, "flow_ctrl")
