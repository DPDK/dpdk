# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Single core forwarding performance test suite.

This suite measures the amount of packets which can be forwarded by DPDK using a single core.
The testsuites takes in as parameters a set of parameters, each consisting of a frame size,
Tx/Rx descriptor count, and the expected MPPS to be forwarded by the DPDK application. The
test leverages a performance traffic generator to send traffic at two paired TestPMD interfaces
on the SUT system, which forward to one another and then back to the traffic generator's ports.
The aggregate packets forwarded by the two TestPMD ports are compared against the expected MPPS
baseline which is given in the test config, in order to determine the test result.
"""

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from api.capabilities import (
    LinkTopology,
    requires_link_topology,
)
from api.packet import assess_performance_by_packet
from api.test import verify, write_performance_json
from api.testpmd import TestPmd
from api.testpmd.config import RXRingParams, TXRingParams
from framework.params.types import TestPmdParamsDict
from framework.test_suite import BaseConfig, TestSuite, perf_test


class Config(BaseConfig):
    """Performance test metrics."""

    test_parameters: list[dict[str, int | float]] = [
        {"frame_size": 64, "num_descriptors": 1024, "expected_mpps": 1.00},
        {"frame_size": 128, "num_descriptors": 1024, "expected_mpps": 1.00},
        {"frame_size": 256, "num_descriptors": 1024, "expected_mpps": 1.00},
        {"frame_size": 512, "num_descriptors": 1024, "expected_mpps": 1.00},
        {"frame_size": 1024, "num_descriptors": 1024, "expected_mpps": 1.00},
        {"frame_size": 1518, "num_descriptors": 1024, "expected_mpps": 1.00},
    ]
    delta_tolerance: float = 0.05


@requires_link_topology(LinkTopology.TWO_LINKS)
class TestSingleCoreForwardPerf(TestSuite):
    """Single core forwarding performance test suite."""

    config: Config

    def set_up_suite(self):
        """Set up the test suite."""
        self.test_parameters = self.config.test_parameters
        self.delta_tolerance = self.config.delta_tolerance

    def _transmit(self, testpmd: TestPmd, frame_size: int) -> float:
        """Create a testpmd session with every rule in the given list, verify jump behavior.

        Args:
            testpmd: The testpmd shell to use for forwarding packets.
            frame_size: The size of the frame to transmit.

        Returns:
            The MPPS (millions of packets per second) forwarded by the SUT.
        """
        # Build packet with dummy values, and account for the 14B and 20B Ether and IP headers
        packet = (
            Ether(src="52:00:00:00:00:00")
            / IP(src="1.2.3.4", dst="192.18.1.0")
            / Raw(load="x" * (frame_size - 14 - 20))
        )

        testpmd.start()

        # Transmit for 30 seconds.
        stats = assess_performance_by_packet(packet=packet, duration=30)

        rx_mpps = stats.rx_pps / 1_000_000

        return rx_mpps

    def _produce_stats_table(self, test_parameters: list[dict[str, int | float]]) -> None:
        """Display performance results in table format and write to structured JSON file.

        Args:
            test_parameters: The expected and real stats per set of test parameters.
        """
        header = f"{'Frame Size':>12} | {'TXD/RXD':>12} | {'Real MPPS':>12} | {'Expected MPPS':>14}"
        print("-" * len(header))
        print(header)
        print("-" * len(header))
        for params in test_parameters:
            print(f"{params['frame_size']:>12} | {params['num_descriptors']:>12} | ", end="")
            print(f"{params['measured_mpps']:>12.2f} | {params['expected_mpps']:>14.2f}")
            print("-" * len(header))

        write_performance_json({"results": test_parameters})

    @perf_test
    def single_core_forward_perf(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        # Find SUT DPDK driver to determine driver specific performance optimization flags
        sut_dpdk_driver = self._ctx.sut_node.config.ports[0].os_driver_for_dpdk

        for params in self.test_parameters:
            frame_size = params["frame_size"]
            num_descriptors = params["num_descriptors"]

            driver_specific_testpmd_args: TestPmdParamsDict = {
                "tx_ring": TXRingParams(descriptors=num_descriptors),
                "rx_ring": RXRingParams(descriptors=num_descriptors),
                "nb_cores": 1,
            }

            if sut_dpdk_driver == "mlx5_core":
                driver_specific_testpmd_args["burst"] = 64
                driver_specific_testpmd_args["mbcache"] = 512
            elif sut_dpdk_driver == "i40e":
                driver_specific_testpmd_args["rx_queues"] = 2
                driver_specific_testpmd_args["tx_queues"] = 2

            with TestPmd(
                **driver_specific_testpmd_args,
            ) as testpmd:
                params["measured_mpps"] = self._transmit(testpmd, frame_size)
                params["performance_delta"] = (
                    float(params["measured_mpps"]) - float(params["expected_mpps"])
                ) / float(params["expected_mpps"])
                params["pass"] = float(params["performance_delta"]) >= -self.delta_tolerance

        self._produce_stats_table(self.test_parameters)

        for params in self.test_parameters:
            verify(
                params["pass"] is True,
                f"""Packets forwarded is less than {(1 -self.delta_tolerance)*100}%
                of the expected baseline.
                Measured MPPS = {params["measured_mpps"]}
                Expected MPPS = {params["expected_mpps"]}""",
            )
