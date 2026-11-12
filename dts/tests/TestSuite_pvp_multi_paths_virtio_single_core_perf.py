# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Virtio-user single-core forwarding performance test suite.

This suite measures the packet forwarding throughput of DPDK's virtio-user PMD running on a
single CPU core. Performance is evaluated across multiple virtio path implementations, including
packed versus split rings, in-order delivery, mergeable buffers, and SIMD vectorization.
Test cases are parameterized using combinations of frame sizes, RX/TX descriptor ring counts,
and target MPPS baselines specified in the test configuration. High-speed traffic is injected
from an external generator through the physical host ports into the virtio-user instance,
looped back, and returned to the generator. The aggregate MPPS processed by the single virtio
core is verified against configured expected performance baselines to determine the test result.
"""

from pathlib import PurePath
from typing import cast

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from api.artifact import Artifact
from api.capabilities import (
    LinkTopology,
    requires_link_topology,
)
from api.packet import assess_performance_by_packet
from api.params.eal import Params, VirtualDevice
from api.params.types import LogicalCoreList, RSSSetting, TestPmdParamsDict
from api.test import get_current_test_case_name, verify, write_performance_json
from api.test_suite import PerfSuiteConfig, TestSuite, perf_suite, perf_test
from api.testpmd import TestPmd
from api.testpmd.config import (
    PortTopology,
    RXRingParams,
    SimpleForwardingModes,
    TXRingParams,
)


@perf_suite
@requires_link_topology(LinkTopology.TWO_LINKS)
class TestPvpMultiPathsVirtioSingleCorePerf(TestSuite):
    """Pvp multi paths virtio single core performance test suite."""

    config: PerfSuiteConfig
    vhost_net_path: PurePath

    def set_up_suite(self):
        """Set up the test suite.

        Creates a safe path for vhost socket files and creates an
        editable copy of the test parameters to store results.
        """
        self.vhost_net_path = Artifact(
            "sut", "vhost-net", save_local_copy=False, custom_path=PurePath(".")
        ).path
        self.test_parameters = [p.model_dump() for p in self.config.test_parameters]

    def _transmit(
        self, vhost: TestPmd, virtio: TestPmd, frame_size: int, repetitions: int = 1
    ) -> float:
        """Transmit packets using testpmd instances and compute average MPPS.

        Args:
            vhost: the running vhost testpmd shell.
            virtio: the running virtio testpmd shell.
            frame_size: The size of the frame to transmit.
            repetitions: The number of times to rerun the transmission.

        Returns:
            The MPPS (millions of packets per second) forwarded by the SUT.
        """
        assert repetitions > 0, "Invalid number of repetitions given, must be greater than 0."
        # Build packet with dummy values, and account for the 14B and 20B Ether and IP headers
        packet = (
            Ether(src="52:00:00:00:00:00")
            / IP(src="1.2.3.4", dst="192.18.1.0")
            / Raw(load="x" * (frame_size - 14 - 20))
        )

        vhost.start()
        virtio.start()
        rx_avg = 0.0

        for _ in range(repetitions):
            # Transmit for 5 seconds.
            stats = assess_performance_by_packet(packet=packet, duration=5)
            rx_avg += stats.rx_pps
        return rx_avg / (repetitions * 1_000_000)

    def _create_and_transmit(
        self,
        ring_format: int,
        in_order: int,
        buffers: int,
        vectorized: int,
        extra_args: TestPmdParamsDict,
        queues: int = 1,
    ) -> None:
        """Create testpmd instances with specified params and send traffic.

        Args:
            ring_format: selects the virtio queue descriptor layout.
            in_order: toggles in-order packet processing.
            buffers: enables mergeable rx buffers.
            vectorized: enables SIMD vectorized rx/tx data paths.
            extra_args: extra/generic TestPmd arguments.
            queues: number of rx/tx queues allocated to virtio device.
        """
        sut_dpdk_driver = self._ctx.sut_node.config.ports[0].os_driver_for_dpdk
        vhost_user_vdev = VirtualDevice(f"net_vhost0,iface={self.vhost_net_path},queues={queues}")
        virtio_user_vdev = VirtualDevice(
            "net_virtio_user0,"
            "mac=00:11:22:33:44:10,"
            f"path={self.vhost_net_path},"
            f"queues={queues},"
            f"packed_vq={ring_format},"
            f"mrg_rxbuf={buffers},"
            f"in_order={in_order},"
            f"vectorized={vectorized}"
        )

        for params in self.test_parameters:
            frame_size = params["frame_size"]
            num_descriptors = params["num_descriptors"]
            expected_mpps = params["expected_mpps"]

            default_args: TestPmdParamsDict = {
                "tx_ring": TXRingParams(descriptors=int(num_descriptors)),
                "rx_ring": RXRingParams(descriptors=int(num_descriptors)),
            }

            if sut_dpdk_driver == "mlx5_core":
                default_args["burst"] = 64
                default_args["mbcache"] = 512
            elif sut_dpdk_driver == "i40e":
                default_args["rx_queues"] = 1
                default_args["tx_queues"] = 1

            vhost_params: TestPmdParamsDict = {
                "prefix": "vhost",
                "allowed_ports": [],
                "memory_channels": 4,
                "port_topology": PortTopology.chained,
                "vdevs": [vhost_user_vdev],
                "lcore_list": LogicalCoreList([1, 2, 3]),
                "nb_cores": 2,
                **default_args,
            }

            virtio_params: TestPmdParamsDict = {
                "prefix": "virtio",
                "no_pci": True,
                "memory_channels": 4,
                "allowed_ports": [],
                "vdevs": [virtio_user_vdev],
                "lcore_list": LogicalCoreList([4, 5]),
                "nb_cores": 1,
                **extra_args,
                **default_args,
            }

            with (
                TestPmd(**vhost_params) as vhost,
                TestPmd(**virtio_params) as virtio,
            ):
                vhost.set_forward_mode(SimpleForwardingModes.io)
                virtio.set_forward_mode(SimpleForwardingModes.mac)
                vhost.set_portlist([0, 2, 1])

                assert expected_mpps > 0, "Invalid expected mpps, must be greater than 0."

                measured_mpps = round(
                    self._transmit(vhost, virtio, int(frame_size), repetitions=5), 3
                )

                performance_delta = round(
                    (float(measured_mpps) - float(expected_mpps)) / float(expected_mpps),
                    3,
                )

                params["measured_mpps"] = measured_mpps
                params["performance_delta"] = performance_delta
                params["passing"] = float(performance_delta) >= -self.config.delta_tolerance

        self._produce_stats_table(self.test_parameters)
        for params in self.test_parameters:
            verify(
                params["passing"] is True,
                f"""Packets forwarded is less than {(1 - self.config.delta_tolerance) * 100}%
                of the expected baseline.
                Measured MPPS = {params["measured_mpps"]}
                Expected MPPS = {params["expected_mpps"]}""",
            )

    def _produce_stats_table(self, test_parameters: list[dict]) -> None:
        """Display performance results in table format and write to structured JSON file.

        Args:
            test_parameters: The expected and real stats per set of test parameters.
        """
        test_case = get_current_test_case_name()
        header = f"{'Frame Size':>12} | {'TXD/RXD':>12} | {'Real MPPS':>12} | {'Expected MPPS':>14}"
        print(f"{test_case} Results:")
        print("-" * len(header))
        print(header)
        print("-" * len(header))
        for params in test_parameters:
            print(f"{params['frame_size']:>12} | {params['num_descriptors']:>12} | ", end="")
            print(f"{params['measured_mpps']:>12} | {params['expected_mpps']:>14}")
            print("-" * len(header))

        write_performance_json({"results": test_parameters})

    @perf_test
    def test_perf_vhost_single_core_virtio11_mergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: ring_format=1, buffers=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "tx_offloads": 0,
            "enable_hw_vlan_strip": True,
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=1, buffers=1, in_order=0, vectorized=0, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio11_non_mergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: ring_format=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "tx_offloads": 0,
            "enable_hw_vlan_strip": True,
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=1, buffers=0, in_order=0, vectorized=0, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio_inorder_mergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: buffers=1, in_order=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "tx_offloads": 0,
            "enable_hw_vlan_strip": True,
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=0, buffers=1, in_order=1, vectorized=0, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio_inorder_nonmergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: in_order=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "tx_offloads": 0,
            "enable_hw_vlan_strip": True,
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=0, buffers=0, in_order=1, vectorized=0, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio_mergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: buffers=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "tx_offloads": 0,
            "enable_hw_vlan_strip": True,
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=0, buffers=1, in_order=0, vectorized=0, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio_nonmergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: vectorized=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "enable_hw_vlan_strip": True,
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=0, buffers=0, in_order=0, vectorized=1, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio_vectorized(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: vectorized=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        self._create_and_transmit(ring_format=0, buffers=0, in_order=0, vectorized=1, extra_args={})

    @perf_test
    def test_perf_vhost_single_core_virtio11_inorder_mergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: ring_format=1, buffers=1, in_order=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "tx_offloads": 0,
            "enable_hw_vlan_strip": True,
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=1, buffers=1, in_order=1, vectorized=0, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio11_inorder_nonmergeable(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: ring_format=1, in_order=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
        }
        self._create_and_transmit(
            ring_format=1, buffers=0, in_order=1, vectorized=0, extra_args=extra_args
        )

    @perf_test
    def test_perf_vhost_single_core_virtio11_vectorized(self) -> None:
        """Validate expected single core forwarding performance.

        Steps:
            * Set TestPMD virtio parameters: ring_format=1, in_order=1, vectorized=1.
            * Create a packet according to the frame size specified in the test config.
            * Transmit from the traffic generator's ports 0 and 1 at above the expect.
            * Forward on TestPMD's interfaces 0 and 1 with 1 core.

        Verify:
            * The resulting MPPS forwarded is greater than expected_mpps*(1-delta_tolerance).
        """
        extra_args: TestPmdParamsDict = {
            "rss": cast(RSSSetting, RSSSetting.SetIPOnly()),
            "other_eal_param": Params.from_str("--force-max-simd-bitwidth=512"),
        }
        self._create_and_transmit(
            ring_format=1, buffers=0, in_order=1, vectorized=1, extra_args=extra_args, queues=2
        )
