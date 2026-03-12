# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 University of New Hampshire

"""Speed capability test suite.

This test suite ensures that testpmd recognizes the expected speed from a link in Gbps.
"""

from typing import Literal

from api.capabilities import (
    LinkTopology,
    requires_link_topology,
)
from api.test import verify
from api.testpmd import TestPmd
from framework.test_suite import BaseConfig, TestSuite, func_test


class Config(BaseConfig):
    """Performance test metrics."""

    test_parameters: dict[
        int,
        Literal[
            "1 Gbps",
            "10 Gbps",
            "25 Gbps",
            "40 Gbps",
            "100 Gbps",
            "200 Gbps",
            "400 Gbps",
            "800 Gbps",
            "1600 Gbps",
        ],
    ] = {
        0: "100 Gbps",
    }


@requires_link_topology(LinkTopology.ONE_LINK)
class TestSpeedCapabilities(TestSuite):
    """Speed capabilities test suite."""

    config: Config

    def set_up_suite(self):
        """Set up the test suite."""
        self.test_parameters = self.config.test_parameters

    @func_test
    def validate_port_speed(self) -> None:
        """Validate expected port speed is also the observed port speed.

        Steps:
            * Show port info for each available port

        Verify:
            * The resulting link speeds are equal to the configured link speeds
        """
        with TestPmd() as testpmd:
            for k, v in self.test_parameters.items():
                link_speed = testpmd.show_port_info(k).link_speed
                verify(
                    link_speed == v,
                    f"port {k} speed {link_speed} does not match configured '{v}'",
                )
