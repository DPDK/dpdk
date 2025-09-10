# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""The DPDK device blocklisting test suite.

This testing suite ensures tests the port blocklisting functionality of testpmd.
"""

from api.capabilities import (
    LinkTopology,
    requires_link_topology,
)
from api.testpmd import TestPmd
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.port import Port


@requires_link_topology(LinkTopology.TWO_LINKS)
class TestBlocklist(TestSuite):
    """DPDK device blocklisting test suite."""

    def verify_blocklisted_ports(self, ports_to_block: list[Port]) -> None:
        """Runs testpmd with the given ports blocklisted and verifies the ports."""
        with TestPmd(allowed_ports=[], blocked_ports=ports_to_block) as testpmd:
            allowlisted_ports = {port.device_name for port in testpmd.show_port_info_all()}
            blocklisted_ports = {port.pci for port in ports_to_block}

            # sanity check
            allowed_len = len(allowlisted_ports - blocklisted_ports)
            self.verify(allowed_len > 0, "At least one port should have been allowed")

            blocked = not allowlisted_ports & blocklisted_ports
            self.verify(blocked, "At least one port was not blocklisted")

    @func_test
    def no_blocklisted(self) -> None:
        """Run testpmd with no blocklisted device.

        Steps:
            Run testpmd without specifying allowed or blocked ports.
        Verify:
            That no ports were blocked.
        """
        self.verify_blocklisted_ports([])

    @func_test
    def one_port_blocklisted(self) -> None:
        """Run testpmd with one blocklisted port.

        Steps:
            Run testpmd with one only one blocklisted port and allowing all the other ones.
        Verify:
            That the port was successfully blocklisted.
        """
        self.verify_blocklisted_ports(self.topology.sut_ports[:1])

    @func_test
    def all_but_one_port_blocklisted(self) -> None:
        """Run testpmd with all but one blocklisted port.

        Steps:
            Run testpmd with only one allowed port, blocking all the other ones.
        Verify:
            That all specified ports were successfully blocklisted.
        """
        self.verify_blocklisted_ports(self.topology.sut_ports[:-1])
