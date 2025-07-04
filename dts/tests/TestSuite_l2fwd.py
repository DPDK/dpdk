# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Basic L2 forwarding test suite.

This testing suites runs basic L2 forwarding on testpmd across multiple different queue sizes.
The forwarding test is performed with several packets being sent at once.
"""

from framework.context import filter_cores
from framework.params.testpmd import EthPeer, SimpleForwardingModes
from framework.remote_session.testpmd_shell import TestPmdShell
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import requires
from framework.testbed_model.cpu import LogicalCoreCount
from framework.testbed_model.topology import TopologyType
from framework.utils import generate_random_packets


@requires(topology_type=TopologyType.two_links)
class TestL2fwd(TestSuite):
    """L2 forwarding test suite."""

    #: The total number of packets to generate and send for forwarding.
    NUMBER_OF_PACKETS_TO_SEND = 50
    #: The payload size to use for the generated packets in bytes.
    PAYLOAD_SIZE = 100

    def set_up_suite(self) -> None:
        """Set up the test suite.

        Setup:
            Generate the random packets that will be sent.
        """
        self.packets = generate_random_packets(self.NUMBER_OF_PACKETS_TO_SEND, self.PAYLOAD_SIZE)

    @filter_cores(LogicalCoreCount(cores_per_socket=4))
    @func_test
    def l2fwd_integrity(self) -> None:
        """Test the L2 forwarding integrity.

        Test:
            Configure a testpmd shell with a different numbers of queues (1, 2, 4 and 8) per run.
            Start up L2 forwarding, send random packets from the TG and verify they were all
            received back.
        """
        queues = [1, 2, 4, 8]

        self.topology.sut_ports[0]
        self.topology.tg_ports[0]

        with TestPmdShell(
            forward_mode=SimpleForwardingModes.mac,
            eth_peer=[EthPeer(1, self.topology.tg_port_ingress.mac_address)],
            disable_device_start=True,
        ) as shell:
            for queues_num in queues:
                self._logger.info(f"Testing L2 forwarding with {queues_num} queue(s)")
                shell.set_ports_queues(queues_num)
                shell.start()

                received_packets = self.send_packets_and_capture(self.packets)
                expected_packets = self.get_expected_packets(self.packets)
                self.match_all_packets(expected_packets, received_packets)

                shell.stop()
